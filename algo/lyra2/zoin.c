#include <memory.h>
#include "miner.h"
#include "algo-gate-api.h"
#include "lyra2.h"
#include "avxdefs.h"

__thread uint64_t* zoin_wholeMatrix;

void zoin_hash(void *state, const void *input, uint32_t height)
{
	uint32_t _ALIGN(256) hash[16];

        LYRA2Z( zoin_wholeMatrix, hash, 32, input, 80, input, 80, 2, 330, 256);

	memcpy(state, hash, 32);
}

int scanhash_zoin( int thr_id, struct work *work, uint32_t max_nonce,
                    uint64_t *hashes_done )
{
	uint32_t hash[8] __attribute__ ((aligned (64))); 
	uint32_t endiandata[20] __attribute__ ((aligned (64)));
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;
	const uint32_t Htarg = ptarget[7];
	const uint32_t first_nonce = pdata[19];
	uint32_t nonce = first_nonce;
	if (opt_benchmark)
		ptarget[7] = 0x0000ff;

	for (int i=0; i < 19; i++) {
		be32enc(&endiandata[i], pdata[i]);
	}

	do {
		be32enc(&endiandata[19], nonce);
		zoin_hash( hash, endiandata, work->height );

		if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
			work_set_target_ratio(work, hash);
			pdata[19] = nonce;
			*hashes_done = pdata[19] - first_nonce;
			return 1;
		}
		nonce++;

	} while (nonce < max_nonce && !work_restart[thr_id].restart);

	pdata[19] = nonce;
	*hashes_done = pdata[19] - first_nonce + 1;
	return 0;
}

void zoin_set_target( struct work* work, double job_diff )
{
 work_set_target( work, job_diff / (256.0 * opt_diff_factor) );
}
/*
bool zoin_get_work_height( struct work* work, struct stratum_ctx* sctx )
{
   work->height = sctx->bloc_height;
   return false;
}
*/

bool zoin_thread_init()
{
   const int64_t ROW_LEN_INT64 = BLOCK_LEN_INT64 * 256; // nCols
   const int64_t ROW_LEN_BYTES = ROW_LEN_INT64 * 8;

   int i = (int64_t)ROW_LEN_BYTES * 330; // nRows;
   zoin_wholeMatrix = _mm_malloc( i, 64 );

   if ( zoin_wholeMatrix == NULL )
     return false;

#if defined (__AVX2__)
   memset_zero_m256i( (__m256i*)zoin_wholeMatrix, i/32 );
#elif defined(__AVX__)
   memset_zero_m128i( (__m128i*)zoin_wholeMatrix, i/16 );
#else
   memset( zoin_wholeMatrix, 0, i );
#endif
   return true;
}

bool register_zoin_algo( algo_gate_t* gate )
{
  gate->optimizations = SSE2_OPT | AES_OPT | AVX_OPT | AVX2_OPT;
  gate->miner_thread_init = (void*)&zoin_thread_init;
  gate->scanhash   = (void*)&scanhash_zoin;
  gate->hash       = (void*)&zoin_hash;
  gate->hash_alt   = (void*)&zoin_hash;
  gate->get_max64  = (void*)&get_max64_0xffffLL;
  gate->set_target = (void*)&zoin_set_target;
//  gate->prevent_dupes = (void*)&zoin_get_work_height;
  return true;
};

