#include <cufft.h>
#include "gpu_context.h"
#include "pfb_gpu.h"
#include "vegas_error.h"

// Ugly, but so much depends upon it
extern int run;

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
        _nsubband(0),
        _blanker()
{
    memset(&_stPlan, 0, sizeof(_stPlan));
    memset(&_first_time_heap_in_accum, 0, sizeof(_first_time_heap_in_accum));    
}

GpuContext::GpuContext(GpuContext *p, int nsubband, int nchan, int in_blok_siz, int out_blok_siz)
{
    _blanker.reset();
    memset(&_first_time_heap_in_accum, 0, sizeof(_first_time_heap_in_accum));
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
        _nsubband      = p->_nsubband;       p->_nsubband = 0;
        _nchan         = p->_nchan;          p->_nchan   = 0;    
    }
    else
    {
        // If we have no object to consume, initialize everything to nil
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
    
    // Do we have new buffer geometry?
    if (_nsubband == nsubband &&
        _nchan    == nchan &&
        _in_block_size == in_blok_siz &&
        _out_block_size == out_blok_siz)
    {
        // Nothing changed, so we should be done
        printf("### No GPU reallocations necessary\n");
        return;
    }
    else
    {
        release_resources();       
        // setup the new configuration
        _nsubband = nsubband;
        _nchan = nchan;
        _in_block_size = in_blok_siz;
        _out_block_size = out_blok_siz;
        // Now allocate new resources for the new configuration
        init_resources();
    }
}

bool
GpuContext::verify_setup(int nsubband, int nchan, int in_block_size, int out_block_size)
{
    _blanker.reset();
    memset(&_first_time_heap_in_accum, 0, sizeof(_first_time_heap_in_accum));
    _first_time_heap_mjd = 0.0;
    _first_time_heap_in_accum_status_bits = 0; 
       
    // Does the setup match?
    if (_nsubband == nsubband &&
        _nchan    == nchan &&
        _in_block_size  == in_block_size &&
        _out_block_size == out_block_size)
        return true;
    return false;
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
    _first_time_heap_in_accum_status_bits = 0;

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
       input buffer, allocate enough to hold two entire data blocks.
     */
    CUDA_SAFE_CALL(cudaMalloc((void **) &_pc4Data_d,
                                       (buf_in_block_size * 2)
                                        ));
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
GpuContext::blanking_inputs(int status)
{
    _blanker.new_input(status);
}

int
GpuContext::blank_current_fft()
{
    return _blanker.blank_current_fft();
}

int
GpuContext::needs_flush()
{
    return _blanker.needs_flush();
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

