#ifndef _PFB_GPU_H
#define _PFB_GPU_H

#include "vegas_databuf.h"

#define FALSE               0
#define TRUE                1

#define VEGAS_NUM_TAPS      8


#if defined __cplusplus
extern "C"
#endif
int init_cuda_context(int, int, int, int);

#if defined __cplusplus
extern "C"
#endif
int reset_state(size_t input_block_sz, size_t output_block_sz, int num_subbands, int num_chans);

#if defined __cplusplus
extern "C"
#endif
void do_pfb(struct vegas_databuf *db_in,
            int curblock_in,
            struct vegas_databuf *db_out,
            int *curblock_out,
            int first,
            struct vegas_status st,
            int acc_len);

int do_fft();

int accumulate();

void zero_accumulator();

int get_accumulated_spectrum_from_device(char *out);

int is_valid(int heap_start, int num_heaps);

int is_blanked(int heap_start, int num_heaps);

/* Free up any allocated memory */
#if defined __cplusplus
extern "C"
#endif
void cleanup_gpu();

#endif
