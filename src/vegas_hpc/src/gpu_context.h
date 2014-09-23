#ifndef gpu_context_h
#define gpu_context_h

#include <cufft.h> 
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "BlankingStateMachine.h"
#include "spead_heap.h"

#define CUDA_SAFE_CALL(call) \
do { \
    cudaError_t err = call; \
    if (cudaSuccess != err) { \
        fprintf (stderr, "Cuda error in file '%s' in line %i : %s.", \
                 __FILE__, __LINE__, cudaGetErrorString(err) ); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define FILE_COEFF_PREFIX   "coeff"
#define FILE_COEFF_DATATYPE "float"
#define FILE_COEFF_SUFFIX   ".dat"

#define FFTPLAN_RANK        1
// #define FFTPLAN_ISTRIDE     (2 * g_iNumSubBands)
// #define FFTPLAN_OSTRIDE     (2 * g_iNumSubBands)
#define FFTPLAN_IDIST       1
#define FFTPLAN_ODIST       1
// #define FFTPLAN_BATCH       (2 * g_iNumSubBands)


class GpuContext
{
public:
    // stuff associated with gpu
    GpuContext();
    GpuContext(GpuContext *, int nchan, int nsubbands, int inblocksz, int outblksz);
    cufftHandle _stPlan;
    float4* _pf4FFTIn_d;
    float4* _pf4FFTOut_d;
    char4*  _pc4InBuf;
    char4*  _pc4Data_d;              /* raw data starting address */
    char4*  _pc4DataRead_d;          /* raw data read pointer */
    dim3    _dimBPFB;
    dim3    _dimGPFB;
    dim3    _dimBAccum;
    dim3    _dimGAccum;
    float * _pfPFBCoeff;
    float * _pfPFBCoeff_d;
    float4* _pf4SumStokes_d;
    
    int     _nchan;
    int     _nsubband;
    int     _in_block_size;
    int     _out_block_size;
    int     _init_status;
    int     _first_time_heap_in_accum_status_bits;
    double  _first_time_heap_mjd;
    
    BlankingStateMachine _blanker;
    
    struct time_spead_heap _first_time_heap_in_accum;
    
    int fft_in_stride()  { return 2*_nsubband; };
    int fft_out_stride() { return 2*_nsubband; };
    int fft_batch()      { return 2*_nsubband; };
    int accumulate();
    int do_fft();
    void zero_accumulator();
    int get_accumulated_spectrum_from_device(char *h_out);
    int init_resources();
    void release_resources();
    int init_status()    { return _init_status; }
    bool verify_setup(int num_subbands, int num_chans, 
                      int input_block_sz, int output_block_sz);
    void blanking_inputs(int);
    int  blank_current_fft();
    int  needs_flush();
    int  sw_status_changed(int swstat) { return _blanker.sw_status_changed(swstat); }
    BlankingStateMachine * blanker() { return &_blanker; } // for debug only!!

};


#endif
