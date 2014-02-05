#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <cufft.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "fitshead.h"
#include "vegas_error.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "vegas_status.h"
#include "vegas_databuf.h"
#ifdef __cplusplus
}
#endif
#include "vegas_defines.h"
#include "pfb_gpu.h"
#include "pfb_gpu_kernels.h"
#include "spead_heap.h"

#define STATUS_KEY "GPUSTAT"

/* ASSUMPTIONS: 1. All blocks contain the same number of heaps. */

extern int run;

/**
 * Global variables: maybe move this to a struct that is passed to each function?
 */
 
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

};

GpuContext::GpuContext() :
        _pf4FFTIn_d(0),
        _pf4FFTOut_d(0),
        _pc4InBuf(0),
        _pc4Data_d(0),
        _pc4DataRead_d(0),
        _dimBPFB(),
        _dimGPFB(),
        _dimBAccum(),
        _dimGAccum(),
        _pfPFBCoeff(0),
        _pfPFBCoeff_d(0),
        _pf4SumStokes_d(0),
        _nchan(0),
        _nsubband(0)
{
    memset(&_stPlan, 0, sizeof(_stPlan));    
}

GpuContext::GpuContext(GpuContext *p, int nsubband, int nchan, int in_blok_siz, int out_blok_siz)
{

    if (p != 0)
    {
        // Move resources from p into this object and null out p's reference
        _pf4FFTIn_d    = p->_pf4FFTIn_d;     p->_pf4FFTIn_d = 0;
        _pf4FFTOut_d   = p->_pf4FFTOut_d;    p->_pf4FFTOut_d = 0;
        _pc4InBuf      = p->_pc4InBuf;       p->_pc4InBuf = 0;
        _pc4Data_d     = p->_pc4Data_d;      p->_pc4Data_d = 0;
        _pc4DataRead_d = p->_pc4DataRead_d;  p->_pc4DataRead_d = 0;
        _dimBPFB       = p->_dimBPFB;
        _dimGPFB       = p->_dimGPFB;
        _dimBAccum     = p->_dimBAccum;
        _pfPFBCoeff    = p->_pfPFBCoeff;     p->_pfPFBCoeff = 0;
        _pfPFBCoeff_d  = p->_pfPFBCoeff_d;   p->_pfPFBCoeff_d = 0;
        _pf4SumStokes_d= p->_pf4SumStokes_d; p->_pf4SumStokes_d = 0;
        _stPlan        = p->_stPlan;         p->_stPlan = 0;    
    }
    else
    {
        _pf4FFTIn_d    = 0;
        _pf4FFTOut_d   = 0;
        _pc4InBuf      = 0;
        _pc4Data_d     = 0;
        _pc4DataRead_d = 0;
        _pfPFBCoeff    = 0;
        _pfPFBCoeff_d  = 0;
        _pf4SumStokes_d= 0;
        _stPlan        = 0;
        _nchan         = 0;
        _nsubband      = 0;    
    }
    
    if (_nsubband == nsubband &&
        _nchan    == nchan &&
        _in_block_size == in_blok_siz &&
        _out_block_size == out_blok_siz)
    {
        // We should be done
        return;
    }
    else
    {
        release_resources();
       
        // Now allocate new resources for the new configuration
        _nsubband = nsubband;
        _nchan = nchan;
        _in_block_size = in_blok_siz;
        _out_block_size = out_blok_siz;
        init_resources();
    }
}

bool
GpuContext::verify_setup(int nsubband, int nchan, int in_block_size, int out_block_size)
{
    // Does the setup match?
    if (_nsubband == nsubband &&
        _nchan    == nchan &&
        _in_block_size  == in_block_size &&
        _out_block_size == out_block_size)
        return true;
    return false;
}



// Make the damn object global until we complete refactoring ...
GpuContext *gpuCtx = 0;

static size_t g_buf_out_block_size;
static unsigned int g_iPrevBlankingState = FALSE;
static int g_iTotHeapOut = 0;
static int g_iMaxNumHeapOut = 0;
static int g_iHeapOut = 0;

/* these arrays need to be only a little longer than MAX_HEAPS_PER_BLK, but
   since we don't know the exact length, just allocate twice that value */
static unsigned int g_auiStatusBits[2*MAX_HEAPS_PER_BLK] = {0};
static unsigned int g_auiHeapValid[2*MAX_HEAPS_PER_BLK] = {0};
static int g_iSpecPerAcc = 0;

void __CUDASafeCall(cudaError_t iCUDARet,
                               const char* pcFile,
                               const int iLine,
                               void (*pCleanUp)(void));

#define CUDASafeCall(iRet)   __CUDASafeCall(iRet,       \
                                                                  __FILE__,   \
                                                                  __LINE__,   \
                                                                  &cleanup_gpu)

#define CUDA_SAFE_CALL(call) \
do { \
    cudaError_t err = call; \
    if (cudaSuccess != err) { \
        fprintf (stderr, "Cuda error in file '%s' in line %i : %s.", \
                 __FILE__, __LINE__, cudaGetErrorString(err) ); \
        exit(EXIT_FAILURE); \
    } \
} while (0)


extern "C"
int init_cuda_context(int subbands, int chans, int inBlokSz, int outBlokSz)
{
    int iDevCount = 0;

    /* since CUDASafeCall() calls cudaGetErrorString(),
       it should not be used here - will cause crash if no CUDA device is
       found */
    (void) cudaGetDeviceCount(&iDevCount);
    if (0 == iDevCount)
    {
        (void) fprintf(stderr, "ERROR: No CUDA-capable device found!\n");
        return EXIT_FAILURE;
    }        

    if (subbands == 0 || chans == 0)
    {

        /* just use the first device */
        printf("pfb_gpu.cu: CUDA_SAFE_CALL(cudaSetDevice(0))\n");
        CUDA_SAFE_CALL(cudaSetDevice(0));
        printf("pfb_gpu.cu: CUDA_SAFE_CALL(cudaFree(0)\n");
        CUDA_SAFE_CALL(cudaFree(0));
        printf("#################### GPU CONTEXT INITIALIZED ####################\n");
    }
    else
    {
        // Create a new mode specific set of resources.
        GpuContext *newctx, *oldctx;
        oldctx = gpuCtx;
        newctx = new GpuContext(oldctx, subbands, chans, inBlokSz, outBlokSz);
        gpuCtx = newctx;
        delete oldctx;
    }
    return EXIT_SUCCESS;
}


/* Initialize all necessary memory, etc for doing PFB
 * at the given params.
 */
extern "C" 
int reset_state(size_t input_block_sz, size_t output_block_sz, int num_subbands, int num_chans)
{
    g_iPrevBlankingState = TRUE;
    g_iTotHeapOut = 0;
    g_iHeapOut = 0;
    g_iSpecPerAcc = 0;
    
    // Now verify we have the right setup
    if (gpuCtx == 0 || 
        true != gpuCtx->verify_setup(num_subbands, num_chans, input_block_sz, output_block_sz))
    {
        printf("Error: runtime and pre-init GPU setups didn't match\n");
        // For backward compatibility we try to initialize here.
        if (EXIT_SUCCESS != init_cuda_context(num_subbands, num_chans, input_block_sz, output_block_sz))
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int GpuContext::init_resources()
{
    int iDevCount = 0;
    cudaDeviceProp stDevProp = {0};
    cufftResult iCUFFTRet = CUFFT_SUCCESS;
    int iRet = EXIT_SUCCESS;
    int iMaxThreadsPerBlock = 0;
    size_t buf_in_block_size;
    int iFileCoeff = 0;
    char acFileCoeff[256] = {0};

    
    buf_in_block_size    = _in_block_size;
    g_buf_out_block_size = _out_block_size;

    /* since CUDASafeCall() calls cudaGetErrorString(),
       it should not be used here - will cause crash if no CUDA device is
       found */
    (void) cudaGetDeviceCount(&iDevCount);
    if (0 == iDevCount)
    {
        (void) fprintf(stderr, "ERROR: No CUDA-capable device found!\n");
        run = 0;
        return EXIT_FAILURE;
    }

    /* just use the first device */
    printf("pfb_gpu.cu: CUDA_SAFE_CALL(cudaSetDevice(0))\n");
    CUDA_SAFE_CALL(cudaSetDevice(0));

    CUDA_SAFE_CALL(cudaGetDeviceProperties(&stDevProp, 0));
    iMaxThreadsPerBlock = stDevProp.maxThreadsPerBlock;
    printf("pfb_gpu.cu: iMaxThreadsPerBlock = %i\n", iMaxThreadsPerBlock);

    _pfPFBCoeff = (float *) malloc(_nsubband
                                   * VEGAS_NUM_TAPS
                                   * _nchan
                                   * sizeof(float));
    if (NULL == _pfPFBCoeff)
    {
        (void) fprintf(stderr,
                       "ERROR: GpuContext Memory allocation failed! %s.\n",
                       strerror(errno));
        return EXIT_FAILURE;
    }

    /* allocate memory for the filter coefficient array on the device */

    printf("pfb_gpu.cu: before CUDA_SAFE_CALL(cudaFree(0))\n");
    CUDA_SAFE_CALL(cudaFree(0));
    printf("pfb_gpu.cu: after CUDA_SAFE_CALL(cudaFree(0))\n");

    printf("pfb_gpu.cu:  before CUDA_SAFE_CALL(cudaMalloc((void...\n");
    printf("subbands=%i, taps=%i, nchan=%i, floatsize=%i\n", _nsubband, VEGAS_NUM_TAPS, _nchan, sizeof(float));
    CUDA_SAFE_CALL(cudaMalloc((void **) &_pfPFBCoeff_d,
                                       _nsubband
                                       * VEGAS_NUM_TAPS
                                       * _nchan
                                       * sizeof(float)));
    printf("pfb_gpu.cu:  CUDA_SAFE_CALL(cudaMalloc((void...\n");

    /* read filter coefficients */
    /* Locate the coefficient directory.  This searches for the configuration
     * directory in one of YGOR_TELESCOPE, VEGAS_DIR or CONFIG_DIR
       If none of the environment variables above are specified
       then we punt and use the current working directory.
     */
     
     
    char *ygor_root = getenv("YGOR_TELESCOPE");
    char *vdir_root = getenv("VEGAS_DIR");
    char *config_root = getenv("CONFIG_DIR");
    char conf_dir_root[128];
    
    if (ygor_root)
    {
        /* Use YGOR_TELESCOPE if available */
        snprintf(conf_dir_root, sizeof(conf_dir_root), "%s/etc/config", ygor_root);
    }
    else if (config_root)
    {
        snprintf(conf_dir_root, sizeof(conf_dir_root), "%s", config_root);
    }
    else if (vdir_root)
    {
        snprintf(conf_dir_root, sizeof(conf_dir_root), "%s", vdir_root);
    }
    else
    {
        snprintf(conf_dir_root, sizeof(conf_dir_root), ".");
    }

    /* build file name */
    (void) snprintf(acFileCoeff, sizeof(acFileCoeff),
                   "%s/%s_%s_%d_%d_%d%s",
                   conf_dir_root,                   
                   FILE_COEFF_PREFIX,
                   FILE_COEFF_DATATYPE,
                   VEGAS_NUM_TAPS,
                   _nchan,
                   _nsubband,
                   FILE_COEFF_SUFFIX);

    iFileCoeff = open(acFileCoeff, O_RDONLY);
    if (iFileCoeff < EXIT_SUCCESS)
    {
        (void) fprintf(stderr,
                       "ERROR: Opening filter coefficients file %s "
                       "failed! %s.\n",
                       acFileCoeff,
                       strerror(errno));
        return EXIT_FAILURE;
    }

    iRet = read(iFileCoeff,
                _pfPFBCoeff,
                _nsubband * VEGAS_NUM_TAPS * _nchan * sizeof(float));
    if (iRet != (_nsubband * VEGAS_NUM_TAPS * _nchan * sizeof(float)))
    {
        (void) fprintf(stderr,
                       "ERROR: Reading filter coefficients failed! %s.\n",
                       strerror(errno));
        return EXIT_FAILURE;
    }
    (void) close(iFileCoeff);

    /* copy filter coefficients to the device */
    CUDA_SAFE_CALL(cudaMemcpy(_pfPFBCoeff_d,
                              _pfPFBCoeff,
                              _nsubband * VEGAS_NUM_TAPS * _nchan * sizeof(float),
                              cudaMemcpyHostToDevice));

    /* allocate memory for data array - 32MB is the block size for the VEGAS
       input buffer, allocate 32MB + space for (VEGAS_NUM_TAPS - 1) blocks of
       data
       NOTE: the actual data in a 32MB block will be only
       (num_heaps * heap_size), but since we don't know that value until data
       starts flowing, allocate the maximum possible size */
    CUDA_SAFE_CALL(cudaMalloc((void **) &_pc4Data_d,
                                       (buf_in_block_size
                                        + ((VEGAS_NUM_TAPS - 1)
                                           * _nsubband
                                           * _nchan
                                           * sizeof(char4)))));
    printf("pfb_gpu.cu: CUDA_SAFE_CALL(cudaMalloc((void...)\n");
    _pc4DataRead_d = _pc4Data_d;
    
    /* calculate kernel parameters */
    /* ASSUMPTION: gpuCtx._nchan >= iMaxThreadsPerBlock */
    _dimBPFB.x =   iMaxThreadsPerBlock;
    _dimBAccum.x = iMaxThreadsPerBlock;
    _dimGPFB.x =   (_nsubband * _nchan) / iMaxThreadsPerBlock;
    _dimGAccum.x = (_nsubband * _nchan) / iMaxThreadsPerBlock;

    CUDA_SAFE_CALL(cudaMalloc((void **) &_pf4FFTIn_d,
                                 _nsubband * _nchan * sizeof(float4)));
    CUDA_SAFE_CALL(cudaMalloc((void **) &_pf4FFTOut_d,
                                 _nsubband * _nchan * sizeof(float4)));
    CUDA_SAFE_CALL(cudaMalloc((void **) &_pf4SumStokes_d,
                                 _nsubband * _nchan * sizeof(float4)));
    CUDA_SAFE_CALL(cudaMemset(_pf4SumStokes_d,
                              0,
                              _nsubband * _nchan * sizeof(float4)));

    printf("pfb_gpu.cu: 4 CUDA_SAFE_CALL(cudaMalloc...) calls\n");

    /* create plan */
    iCUFFTRet = cufftPlanMany(&_stPlan,
                              FFTPLAN_RANK,
                              &_nchan,
                              &_nchan,
                              fft_in_stride(),
                              FFTPLAN_IDIST,
                              &_nchan,
                              fft_in_stride(),
                              FFTPLAN_ODIST,
                              CUFFT_C2C,
                              fft_batch() );
    if (iCUFFTRet != CUFFT_SUCCESS)
    {
        (void) fprintf(stderr, "ERROR: Plan creation failed!\n");
        run = 0;
        return EXIT_FAILURE;
    }
    printf("GPU resources resized for %d subbands and %d channels\n", _nsubband, _nchan);
    printf("#################### GPU RE-INIT COMPLETE ####################\n");
    return EXIT_SUCCESS;
}

void
GpuContext::release_resources()
{
    // Free existing resources
    printf("Releasing GPU resources \n");
    
    if (_pc4InBuf != NULL)
    {
        free(_pc4InBuf);
        _pc4InBuf = NULL;
    }
    if (_pc4Data_d != NULL)
    {
        (void) cudaFree(_pc4Data_d);
        _pc4Data_d = NULL;
    }
    if (_pf4FFTIn_d != NULL)
    {
        (void) cudaFree(_pf4FFTIn_d);
        _pf4FFTIn_d = NULL;
    }
    if (_pf4FFTOut_d != NULL)
    {
        (void) cudaFree(_pf4FFTOut_d);
        _pf4FFTOut_d = NULL;
    }
    if (_pf4SumStokes_d != NULL)
    {
        (void) cudaFree(_pf4SumStokes_d);
        _pf4SumStokes_d = NULL;
    }

    /* destroy plan */
    /* TODO: check if plan exists */
    if (_stPlan)
    {
        (void) cufftDestroy(_stPlan);
        _stPlan = NULL;
    }
}

struct freq_spead_heap *
frequency_heap(struct vegas_databuf *db, int iblk, int iHeap)
{
    struct freq_spead_heap *freq_heap;
    char *ptr;
    ptr = (char *)(vegas_databuf_data(db, iblk) +
                   sizeof(struct freq_spead_heap) * iHeap);
    freq_heap = (struct freq_spead_heap*)ptr;                
    return freq_heap;
}

/* dump to buffer */
int dump_to_buffer(struct vegas_databuf *db_out,         // Output databuffer
                   int curblk_out,                       // Current output block
                   int iHeapOut,                         // output frequency heap number in current block
                   struct time_spead_heap *firsttimeheap,// first time sample of input 
                   int iTotHeapOut,                      // spectrum number/counter
                   int iSpecPerAcc,                      // GPU accumulations in this heap
                   double heap_mjd )                     // MJD from index_input
{
    struct freq_spead_heap *freq_heap_out;
    char * payload_addr_out;
    struct databuf_index *index_out;
    int rtn;
    
    freq_heap_out = frequency_heap(db_out, curblk_out, iHeapOut);
    index_out = (struct databuf_index*)vegas_databuf_index(db_out, curblk_out);
    
    payload_addr_out = (char*)(vegas_databuf_data(db_out, curblk_out) +
                        sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK +
                        (index_out->heap_size - sizeof(struct freq_spead_heap)) * iHeapOut);                        

    /* Write new heap header fields */
    freq_heap_out->time_cntr_id = 0x20;
    freq_heap_out->time_cntr_top8 = firsttimeheap->time_cntr_top8;
    freq_heap_out->time_cntr = firsttimeheap->time_cntr;
    freq_heap_out->spectrum_cntr_id = 0x21;
    freq_heap_out->spectrum_cntr = iTotHeapOut;
    freq_heap_out->integ_size_id = 0x22;
    freq_heap_out->integ_size = iSpecPerAcc;
    freq_heap_out->mode_id = 0x23;
    freq_heap_out->mode = firsttimeheap->mode;
    freq_heap_out->status_bits_id = 0x24;
    freq_heap_out->status_bits = firsttimeheap->status_bits;
    freq_heap_out->payload_data_off_addr_mode = 0;
    freq_heap_out->payload_data_off_id = 0x25;
    freq_heap_out->payload_data_off = 0;
    
/////DEBUG
    memset(firsttimeheap, 0, sizeof(struct time_spead_heap));

    /* Update output index */
    index_out->cpu_gpu_buf[iHeapOut].heap_valid = 1;
    index_out->cpu_gpu_buf[iHeapOut].heap_cntr = iTotHeapOut;
    index_out->cpu_gpu_buf[iHeapOut].heap_rcvd_mjd = heap_mjd;

    /* copy out GPU data into buffer */
    rtn = gpuCtx->get_accumulated_spectrum_from_device(payload_addr_out);
    index_out->num_heaps += (rtn == VEGAS_OK ? 1 : 0);
    return rtn;
}

/* Actually do the PFB by calling CUDA kernels */
extern "C"
void do_pfb(struct vegas_databuf *db_in,
            int curblock_in,
            struct vegas_databuf *db_out,
            int *curblock_out,
            int first,
            struct vegas_status st,
            int acc_len)
{
    /* Declare local variables */
    char *hdr_out = NULL;
    struct databuf_index *index_in = NULL;
    struct databuf_index *index_out = NULL;
    int heap_in = 0;
    char *heap_addr_in = NULL;
    struct time_spead_heap first_time_heap_in_accum;
    int iProcData = 0;
    cudaError_t iCUDARet = cudaSuccess;
    int iRet = VEGAS_OK;
    char* payload_addr_in = NULL;
    int num_in_heaps_per_proc = 0;
    int pfb_count = 0;
    int num_in_heaps_gpu_buffer = 0;
    int num_in_heaps_tail = 0;
    int i = 0;
    int iBlockInDataSize;
    double first_time_heap_mjd;

    /* Setup input and first output data block stuff */
    index_in = (struct databuf_index*)vegas_databuf_index(db_in, curblock_in);
    /* Get the number of heaps per block of data that will be processed by the GPU */
    num_in_heaps_per_proc = (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4)) / (index_in->heap_size - sizeof(struct time_spead_heap));
    iBlockInDataSize = (index_in->num_heaps * index_in->heap_size) - (index_in->num_heaps * sizeof(struct time_spead_heap));

    num_in_heaps_tail = (((VEGAS_NUM_TAPS - 1) * gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4))
                         / (index_in->heap_size - sizeof(struct time_spead_heap)));
    num_in_heaps_gpu_buffer = index_in->num_heaps + num_in_heaps_tail;

    /* Calculate the maximum number of output heaps per block */
    g_iMaxNumHeapOut = (g_buf_out_block_size - (sizeof(struct time_spead_heap) * MAX_HEAPS_PER_BLK)) / (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(float4));

    hdr_out = vegas_databuf_header(db_out, *curblock_out);
    index_out = (struct databuf_index*)vegas_databuf_index(db_out, *curblock_out);
    // index_out->num_heaps = 0;
    memcpy(hdr_out, vegas_databuf_header(db_in, curblock_in),
            VEGAS_STATUS_SIZE);

    /* Set basic params in output index */
    index_out->heap_size = sizeof(struct freq_spead_heap) + (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(float4));
    /* Read in heap from buffer */
    heap_addr_in = (char*)(vegas_databuf_data(db_in, curblock_in) +
                        sizeof(struct time_spead_heap) * heap_in);
    // first_time_heap_in_accum = (struct time_spead_heap*)(heap_addr_in);
    memcpy(&first_time_heap_in_accum, heap_addr_in, sizeof(first_time_heap_in_accum));
    first_time_heap_mjd = index_in->cpu_gpu_buf[heap_in].heap_rcvd_mjd;
    /* Here, the payload_addr_in is the start of the contiguous block of data that will be
       copied to the GPU (heap_in = 0) */
    payload_addr_in = (char*)(vegas_databuf_data(db_in, curblock_in) +
                        sizeof(struct time_spead_heap) * MAX_HEAPS_PER_BLK +
                        (index_in->heap_size - sizeof(struct time_spead_heap)) * heap_in );

    /* Copy data block to GPU */
    if (first)
    {
        // bloksz replaces a calculated value which caused the check below to fail
        // in the presence of dropped packets. We used the blocksize from the data buffer
        // instead here to get things going. Not sure how dropped data at start of
        // scan should be treated.
        int bloksz = db_in->block_size;
        /* Sanity check for the first iteration */
        if ((bloksz % (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4))) != 0)
        {
            (void) fprintf(stderr, "ERROR: Data size mismatch! BlockInDataSize=%d NumSubBands=%d nchan=%d\n",
                                    bloksz, gpuCtx->_nsubband, gpuCtx->_nchan);
            run = 0;
            return;
        }
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d,
                                payload_addr_in,
                                bloksz,
                                cudaMemcpyHostToDevice));
        /* duplicate the last (VEGAS_NUM_TAPS - 1) segm                payload_addr_out = (char*)(vegas_databuf_data(db_out, g_iPFBCurBlockOut) +
                                sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK +
                                (index_out->heap_size - sizeof(struct freq_spead_heap)) * g_iHeapOut);
ents at the end for
           the next iteration */
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d + (bloksz / sizeof(char4)),
                                gpuCtx->_pc4Data_d + (bloksz / sizeof(char4)) - ((VEGAS_NUM_TAPS - 1) * gpuCtx->_nsubband * gpuCtx->_nchan),
                                ((VEGAS_NUM_TAPS - 1) * gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4)),
                                cudaMemcpyDeviceToDevice));

        /* copy the status bits and valid flags for all heaps to arrays separate
           from the index, so that it can be combined with the corresponding
           values from the previous block */
        struct time_spead_heap* time_heap = (struct time_spead_heap*) vegas_databuf_data(db_in, curblock_in);
        for (i = 0; i < index_in->num_heaps; ++i)
        {
            g_auiStatusBits[i] = time_heap->status_bits;
            g_auiHeapValid[i] = index_in->cpu_gpu_buf[i].heap_valid;
            ++time_heap;
        }
        /* duplicate the last (VEGAS_NUM_TAPS - 1) segments at the end for the
           next iteration */
        for ( ; i < index_in->num_heaps + num_in_heaps_tail; ++i)
        {
            g_auiStatusBits[i] = g_auiStatusBits[i-num_in_heaps_tail];
            g_auiHeapValid[i] = g_auiHeapValid[i-num_in_heaps_tail];
        }
    }
    else
    {
        /* If this is not the first run, need to handle block boundary, while doing the PFB */
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d,
                                gpuCtx->_pc4Data_d + (iBlockInDataSize / sizeof(char4)),
                                ((VEGAS_NUM_TAPS - 1) * gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4)),
                                cudaMemcpyDeviceToDevice));
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d + ((VEGAS_NUM_TAPS - 1) * gpuCtx->_nsubband * gpuCtx->_nchan),
                                payload_addr_in,
                                iBlockInDataSize,
                                cudaMemcpyHostToDevice));
        /* copy the status bits and valid flags for all heaps to arrays separate
           from the index, so that it can be combined with the corresponding
           values from the previous block */
        for (i = 0; i < num_in_heaps_tail; ++i)
        {
            g_auiStatusBits[i] = g_auiStatusBits[index_in->num_heaps+i];
            g_auiHeapValid[i] = g_auiHeapValid[index_in->num_heaps+i];
        }
        struct time_spead_heap* time_heap = (struct time_spead_heap*) vegas_databuf_data(db_in, curblock_in);
        for ( ; i < num_in_heaps_tail + index_in->num_heaps; ++i)
        {
            g_auiStatusBits[i] = time_heap->status_bits;
            g_auiHeapValid[i] = index_in->cpu_gpu_buf[i-num_in_heaps_tail].heap_valid;
            ++time_heap;
        }
    }

    gpuCtx->_pc4DataRead_d = gpuCtx->_pc4Data_d;
    iProcData = 0;
    while (iBlockInDataSize > iProcData)  /* loop till (num_heaps * heap_size) of data is processed */
    {
        if (0 == pfb_count)
        {
            /* Check if all heaps necessary for this PFB are valid */
            if (!(is_valid(heap_in, (VEGAS_NUM_TAPS * num_in_heaps_per_proc))))
            {
                /* Skip all heaps that go into this PFB if there is an invalid heap */
                iProcData += (VEGAS_NUM_TAPS * gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4));
                /* update the data read pointer */
                gpuCtx->_pc4DataRead_d += (VEGAS_NUM_TAPS * gpuCtx->_nsubband * gpuCtx->_nchan);
                if (iProcData >= iBlockInDataSize)
                {
                    break;
                }

                /* Calculate input heap addresses for the next round of processing */
                heap_in += (VEGAS_NUM_TAPS * num_in_heaps_per_proc);
                if (heap_in > num_in_heaps_gpu_buffer)
                {
                    /* This is not supposed to happen (but may happen if odd number of pkts are dropped
                       right at the end of the buffer, so we therefore do not exit) */
                    (void) fprintf(stdout,
                                   "WARNING: Heap count %d exceeds available number of heaps %d!\n",
                                   heap_in,
                                   num_in_heaps_gpu_buffer);
                }
                heap_addr_in = (char*)(vegas_databuf_data(db_in, curblock_in) +
                                    sizeof(struct time_spead_heap) * heap_in);
                // The check above indicates this is an invalid heap, therefore we don't treat
                // it as the 'first heap'                    
                // memcpy(&first_time_heap_in_accum, heap_addr_in, sizeof(first_time_heap_in_accum));
                continue;
            }
        }

        /* Perform polyphase filtering */
        DoPFB<<<gpuCtx->_dimGPFB, gpuCtx->_dimBPFB>>>(gpuCtx->_pc4DataRead_d,
                                                    gpuCtx->_pf4FFTIn_d,
                                                    gpuCtx->_pfPFBCoeff_d);
        CUDA_SAFE_CALL(cudaThreadSynchronize());
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stdout,
                           "ERROR: File <%s>, Line %d: %s\n",
                           __FILE__,
                           __LINE__,
                           cudaGetErrorString(iCUDARet));
            run = 0;
            break;
        }

        iRet = gpuCtx->do_fft();
        if (iRet != VEGAS_OK)
        {
            (void) fprintf(stdout, "ERROR: FFT failed!\n");
            run = 0;
            break;
        }

        /* Accumulate power x, power y, stokes real and imag, if the blanking
           bit is not set */
        if (!(is_blanked(heap_in, num_in_heaps_per_proc)))
        {
            iRet = gpuCtx->accumulate();
            if (iRet != VEGAS_OK)
            {
                (void) fprintf(stdout, "ERROR: Accumulation failed!\n");
                run = 0;
                break;
            }
            ++g_iSpecPerAcc;
            g_iPrevBlankingState = FALSE;
        }
        else
        {
            /* state just changed */
            if (FALSE == g_iPrevBlankingState)
            {
                /* dump to buffer */
                iRet = dump_to_buffer(db_out,             // Output databuffer
                                      *curblock_out,                       // Current output block
                                      g_iHeapOut,                         // output frequency heap number in current block
                                      &first_time_heap_in_accum,// first time sample of input 
                                      g_iTotHeapOut,                      // spectrum number/counter
                                      g_iSpecPerAcc,                      // GPU accumulations in this heap
                                      first_time_heap_mjd); // MJD from index_input
                                                                       
                if (iRet != VEGAS_OK)
                {
                    (void) fprintf(stdout, "ERROR: Getting accumulated spectrum failed (blank state changed)!\n");
                    run = 0;
                    break;
                }

                ++g_iHeapOut;
                ++g_iTotHeapOut;

                /* zero accumulators */
                gpuCtx->zero_accumulator();
                /* reset time */
                g_iSpecPerAcc = 0;
                g_iPrevBlankingState = TRUE;
            }
        }
        if (g_iSpecPerAcc == acc_len)
        {
            /* dump to buffer */
            iRet = dump_to_buffer(db_out,             
                                  *curblock_out,
                                  g_iHeapOut,
                                  &first_time_heap_in_accum,
                                  g_iTotHeapOut,
                                  g_iSpecPerAcc,
                                  first_time_heap_mjd);
                                  
            if (iRet != VEGAS_OK)
            {
                (void) fprintf(stdout, "ERROR: Getting accumulated spectrum failed!\n");
                run = 0;
                break;
            }                                  
            ++g_iHeapOut;
            ++g_iTotHeapOut;

            /* zero accumulators */
            gpuCtx->zero_accumulator();
            /* reset time */
            g_iSpecPerAcc = 0;
        }

        iProcData += (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(char4));
        /* update the data read pointer */
        gpuCtx->_pc4DataRead_d += (gpuCtx->_nsubband * gpuCtx->_nchan);

        /* Calculate input heap addresses for the next round of processing */
        heap_in += num_in_heaps_per_proc;
        heap_addr_in = (char*)(vegas_databuf_data(db_in, curblock_in) +
                            sizeof(struct time_spead_heap) * heap_in);
        if (0 == g_iSpecPerAcc)
        {
            // first_time_heap_in_accum = (struct time_spead_heap*)(heap_addr_in);
            memcpy(&first_time_heap_in_accum, heap_addr_in, sizeof(first_time_heap_in_accum));
            first_time_heap_mjd = index_in->cpu_gpu_buf[heap_in].heap_rcvd_mjd;
        }

        /* if output block is full */
        if (g_iHeapOut == g_iMaxNumHeapOut)
        {
            /* Set the number of heaps written to this block */
            // JJB index_out->num_heaps = g_iHeapOut;
            // printf("gpu filled num_heaps=%d snum=%d\n", index_out->num_heaps, g_iTotHeapOut);

            /* Mark output buffer as filled */
            vegas_databuf_set_filled(db_out, *curblock_out);

            // printf("Debug: vegas_pfb_thread going to next output block\n");

            /* Note current output block */
            /* NOTE: vegas_status_lock_safe() and vegas_status_unlock_safe() are macros
               that have been explicitly expanded here, due to compilation issues */
            //vegas_status_lock_safe(&st);
                pthread_cleanup_push((void (*) (void *))&vegas_status_unlock, (void *) &st);
                vegas_status_lock(&st);
            hputi4(st.buf, "PFBBLKOU", *curblock_out);
            //vegas_status_unlock_safe(&st);
                vegas_status_unlock(&st);
                pthread_cleanup_pop(0);

            /*  Wait for next output block */
            *curblock_out = (*curblock_out + 1) % db_out->n_block;
            while ((vegas_databuf_wait_free(db_out, *curblock_out)!=0) && run) {
                //vegas_status_lock_safe(&st);
                    pthread_cleanup_push((void (*)(void *))&vegas_status_unlock, (void *) &st);
                    vegas_status_lock(&st);

                hputs(st.buf, STATUS_KEY, "blocked");
                //vegas_status_unlock_safe(&st);
                    vegas_status_unlock(&st);
                    pthread_cleanup_pop(0);
            }

            g_iHeapOut = 0;

            hdr_out = vegas_databuf_header(db_out, *curblock_out);
            index_out = (struct databuf_index*)vegas_databuf_index(db_out, *curblock_out);
            index_out->num_heaps = 0;
            memcpy(hdr_out, vegas_databuf_header(db_in, curblock_in),
                    VEGAS_STATUS_SIZE);

            /* Set basic params in output index */
            index_out->heap_size = sizeof(struct freq_spead_heap) + (gpuCtx->_nsubband * gpuCtx->_nchan * sizeof(float4));
        }

        pfb_count = (pfb_count + 1) % VEGAS_NUM_TAPS;
    }

    return;
}

/* function that performs the FFT */
int GpuContext::do_fft()
{
    cufftResult iCUFFTRet = CUFFT_SUCCESS;

    /* execute plan */
    iCUFFTRet = cufftExecC2C(_stPlan,
                             (cufftComplex*) _pf4FFTIn_d,
                             (cufftComplex*) _pf4FFTOut_d,
                             CUFFT_FORWARD);
    if (iCUFFTRet != CUFFT_SUCCESS)
    {
        (void) fprintf(stderr, "ERROR: FFT failed!");
        run = 0;
        return VEGAS_ERR_GEN;
    }

    return VEGAS_OK;
}

int GpuContext::accumulate()
{
    cudaError_t iCUDARet = cudaSuccess;

    Accumulate<<<_dimGAccum, _dimBAccum>>>(_pf4FFTOut_d,
                                           _pf4SumStokes_d);
    CUDA_SAFE_CALL(cudaThreadSynchronize());
    iCUDARet = cudaGetLastError();
    if (iCUDARet != cudaSuccess)
    {
        (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
        run = 0;
        return VEGAS_ERR_GEN;
    }

    return VEGAS_OK;
}

void GpuContext::zero_accumulator()
{
    CUDA_SAFE_CALL(cudaMemset(_pf4SumStokes_d,
                                       '\0',
                                       (_nsubband
                                       * _nchan
                                       * sizeof(float4))));

    return;
}

int GpuContext::get_accumulated_spectrum_from_device(char *out)
{
    /* copy the negative frequencies out first */
    CUDASafeCall(cudaMemcpy(out,
                            _pf4SumStokes_d + (_nsubband * _nchan / 2),
                            (_nsubband
                             * (_nchan / 2)
                             * sizeof(float4)),
                            cudaMemcpyDeviceToHost));
    /* copy the positive frequencies out */
    CUDASafeCall(cudaMemcpy(out + (_nsubband * (_nchan / 2) * sizeof(float4)),
                            _pf4SumStokes_d,
                            (_nsubband
                             * (_nchan / 2)
                             * sizeof(float4)),
                            cudaMemcpyDeviceToHost));
    return VEGAS_OK;
}

/*
 * function to be used to check if any heap within the current PFB is invalid,
 * in which case, the entire PFB should be discarded.
 * NOTE: this function does not check ALL heaps - it returns at the first
 * invalid heap.
 */
int is_valid(int heap_start, int num_heaps)
{
    for (int i = heap_start; i < (heap_start + num_heaps); ++i)
    {
        if (!g_auiHeapValid[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * function that checks if blanking has started within this accumulation.
 * ASSUMPTION: the blanking bit does not toggle within this time interval.
 */
int is_blanked(int heap_start, int num_heaps)
{
    for (int i = heap_start; i < (heap_start + num_heaps); ++i)
    {
        if (g_auiStatusBits[i] & 0x08)
        {
            return TRUE;
        }
    }

    return FALSE;
}

void __CUDASafeCall(cudaError_t iCUDARet,
                               const char* pcFile,
                               const int iLine,
                               void (*pCleanUp)(void))
{
    if (iCUDARet != cudaSuccess)
    {
        (void) fprintf(stderr,
                       "ERROR: File <%s>, Line %d: %s\n",
                       pcFile,
                       iLine,
                       cudaGetErrorString(iCUDARet));
        run = 0;
        return;
    }

    return;
}



/*
 * Frees up any allocated memory.
 */
extern "C"
void cleanup_gpu()
{
#if 0
    /* free memory */
    if (gpuCtx._pc4InBuf != NULL)
    {
        free(gpuCtx._pc4InBuf);
        gpuCtx._pc4InBuf = NULL;
    }
    if (gpuCtx._pc4Data_d != NULL)
    {
        (void) cudaFree(gpuCtx._pc4Data_d);
        gpuCtx._pc4Data_d = NULL;
    }
    if (gpuCtx._pf4FFTIn_d != NULL)
    {
        (void) cudaFree(gpuCtx._pf4FFTIn_d);
        gpuCtx._pf4FFTIn_d = NULL;
    }
    if (gpuCtx._pf4FFTOut_d != NULL)
    {
        (void) cudaFree(gpuCtx._pf4FFTOut_d);
        gpuCtx._pf4FFTOut_d = NULL;
    }
    if (gpuCtx._pf4SumStokes_d != NULL)
    {
        (void) cudaFree(gpuCtx._pf4SumStokes_d);
        gpuCtx._pf4SumStokes_d = NULL;
    }

    /* destroy plan */
    /* TODO: check if plan exists */
    if (gpuCtx._stPlan)
    {
        (void) cufftDestroy(gpuCtx._stPlan);
        gpuCtx._stPlan = NULL;
    }
    printf("#################### GPU CONTEXT CLEANED UP ####################\n");
#endif 
    return;
}
