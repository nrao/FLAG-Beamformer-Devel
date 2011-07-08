/* 
 * vegasp3mgpu.byte.2dtex.cu
 * VEGAS Priority 3 Mode - Stand-Alone GPU Implementation
 *
 * Created by Jayanth Chennamangalam on 2011.06.13
 */

#include "vegasp3mgpu.byte.2dtex.h"

#define BENCHMARKING    0

int g_iIsDone = FALSE;

int g_iMaxThreadsPerBlock = 0;

BYTE *g_pbInBuf = NULL;
BYTE *g_pbInBufRead = NULL;
int g_iReadCount = 0;
int g_iNumReads = 0;

cudaArray* g_pcuabData_d;   /* raw data, LEN_DATA * g_iNFFT */
texture<signed char, 2, cudaReadModeNormalizedFloat> g_stTexData;
cudaChannelFormatDesc g_stChanDescData;

int g_iPFBReadIdx = 0;
int g_iPFBWriteIdx = 0;

int g_iNFFT = DEF_LEN_SPEC;

dim3 g_dimBlockPFB(1, 1, 1);
dim3 g_dimGridPFB(1, 1);
dim3 g_dimBlockCopy(1, 1, 1);
dim3 g_dimGridCopy(1, 1);
dim3 g_dimBlockAccum(1, 1, 1);
dim3 g_dimGridAccum(1, 1);

cufftComplex *g_pccFFTInX = NULL;
cufftComplex *g_pccFFTInX_d = NULL;
cufftComplex *g_pccFFTOutX = NULL;
cufftComplex *g_pccFFTOutX_d = NULL;
cufftHandle g_stPlanX = {0};
cufftComplex *g_pccFFTInY = NULL;
cufftComplex *g_pccFFTInY_d = NULL;
cufftComplex *g_pccFFTOutY = NULL;
cufftComplex *g_pccFFTOutY_d = NULL;
cufftHandle g_stPlanY = {0};

float *g_pfSumPowX = NULL;
float *g_pfSumPowY = NULL;
float *g_pfSumStokesRe = NULL;
float *g_pfSumStokesIm = NULL;

#if GPUACCUM
float *g_pfSumPowX_d = NULL;
float *g_pfSumPowY_d = NULL;
float *g_pfSumStokesRe_d = NULL;
float *g_pfSumStokesIm_d = NULL;
#endif

int g_iIsPFBOn = DEF_PFB_ON;
int g_iNTaps = 1;                       /* 1 if no PFB, NUM_TAPS if PFB */
int g_iFileCoeff = 0;
char g_acFileCoeff[256] = {0};
signed char (*g_pacPFBCoeff)[][NUM_BYTES_PER_SAMP] = NULL;
cudaArray* g_pcuabPFBCoeff_d;
texture<signed char, 2, cudaReadModeNormalizedFloat> g_stTexPFBCoeff;
cudaChannelFormatDesc g_stChanDescPFBCoeff;


int g_iFileData = 0;
char g_acFileData[256] = {0};

/* PGPLOT global */
float *g_pfFreq = NULL;
float g_fFSamp = 1.0;                   /* 1 [frequency] */

#if BENCHMARKING
    float g_fTimeCpIn = 0.0;
    float g_fAvgCpIn = 0.0;
    float g_fTimeUnpack = 0.0;
    float g_fAvgUnpack = 0.0;
    cudaEvent_t g_cuStart;
    cudaEvent_t g_cuStop;
    int g_iCount = 0;
#endif

int main(int argc, char *argv[])
{
    int iRet = GUPPI_OK;
#if (!GPUACCUM)
    int i = 0;
#endif
    int iTime = 0;
    int iAcc = DEF_ACC;
#if BENCHMARKING
    float fTimePFB = 0.0;
    float fAvgPFB = 0.0;
    float fTimeCpInFFT = 0.0;
    float fAvgCpInFFT = 0.0;
    float fTimeFFT = 0.0;
    float fAvgFFT = 0.0;
    float fTimeCpOut = 0.0;
    float fAvgCpOut = 0.0;
    float fTimeAccum = 0.0;
    float fAvgAccum = 0.0;
    float fAvgTotal = 0.0;
#else
    struct timeval stStart = {0};
    struct timeval stStop = {0};
#endif
    const char *pcProgName = NULL;
    int iNextOpt = 0;
    /* valid short options */
    const char* const pcOptsShort = "hn:pa:s:";
    /* valid long options */
    const struct option stOptsLong[] = {
        { "help",           0, NULL, 'h' },
        { "nfft",           1, NULL, 'n' },
        { "pfb",            0, NULL, 'p' },
        { "nacc",           1, NULL, 'a' },
        { "fsamp",          1, NULL, 's' },
        { NULL,             0, NULL, 0   }
    };

    /* get the filename of the program from the argument list */
    pcProgName = argv[0];

    /* parse the input */
    do
    {
        iNextOpt = getopt_long(argc, argv, pcOptsShort, stOptsLong, NULL);
        switch (iNextOpt)
        {
            case 'h':   /* -h or --help */
                /* print usage info and terminate */
                PrintUsage(pcProgName);
                return EXIT_SUCCESS;

            case 'n':   /* -n or --nfft */
                /* set option */
                g_iNFFT = (int) atoi(optarg);
                break;

            case 'p':   /* -p or --pfb */
                /* set option */
                g_iIsPFBOn = TRUE;
                break;

            case 'a':   /* -a or --nacc */
                /* set option */
                iAcc = (int) atoi(optarg);
                break;

            case 's':   /* -s or --fsamp */
                /* set option */
                g_fFSamp = (float) atof(optarg);
                break;

            case '?':   /* user specified an invalid option */
                /* print usage info and terminate with error */
                (void) fprintf(stderr, "ERROR: Invalid option!\n");
                PrintUsage(pcProgName);
                return EXIT_FAILURE;

            case -1:    /* done with options */
                break;

            default:    /* unexpected */
                assert(0);
        }
    } while (iNextOpt != -1);

    /* no arguments */
    if (argc <= optind)
    {
        (void) fprintf(stderr, "ERROR: Data file not specified!\n");
        PrintUsage(pcProgName);
        return GUPPI_ERR_GEN;
    }

    (void) strncpy(g_acFileData, argv[optind], 256);
    g_acFileData[255] = '\0';

    /* initialise */
    iRet = Init();
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr, "ERROR! Init failed!\n");
        CleanUp();
        return GUPPI_ERR_GEN;
    }

#if BENCHMARKING
    (void) printf("* Benchmarking run commencing...\n");
    VEGASCUDASafeCall(cudaEventCreate(&g_cuStart));
    VEGASCUDASafeCall(cudaEventCreate(&g_cuStop));
    (void) printf("* Events created.\n");
#else
    (void) gettimeofday(&stStart, NULL);
#endif
    while (IsRunning())
    {
#if BENCHMARKING
        ++g_iCount;
#endif
        if (g_iIsPFBOn)
        {
            /* do pfb */
#if BENCHMARKING
            VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
            VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
            DoPFB<<<g_dimGridPFB, g_dimBlockPFB>>>(g_iPFBReadIdx,
                                                   g_iNTaps,
                                                   g_pccFFTInX_d,
                                                   g_pccFFTInY_d);
            VEGASCUDASafeCall(cudaThreadSynchronize());
#if BENCHMARKING
            VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
            VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
            VEGASCUDASafeCall(cudaEventElapsedTime(&fTimePFB, g_cuStart, g_cuStop));
            fAvgPFB = (fTimePFB + ((g_iCount - 1) * fAvgPFB)) / g_iCount;
#endif
        }
        else
        {
#if BENCHMARKING
            VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
            VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
            #if 0
            CopyDataForFFT<<<g_dimGridCopy, g_dimBlockCopy>>>(g_pccFFTInX_d,
                                                              g_pccFFTInY_d);
            #else
            CopyDataForFFT<<<g_dimGridCopy, g_dimBlockCopy>>>(g_pcuabData_d,
                                                              g_pccFFTInX_d,
                                                              g_pccFFTInY_d);
            #endif
            VEGASCUDASafeCall(cudaThreadSynchronize());
#if BENCHMARKING
            VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
            VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
            VEGASCUDASafeCall(cudaEventElapsedTime(&fTimeCpInFFT, g_cuStart, g_cuStop));
            fAvgCpInFFT = (fTimeCpInFFT + ((g_iCount - 1) * fAvgCpInFFT)) / g_iCount;
#endif
        }

        /* do fft */
#if BENCHMARKING
        VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
        VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
        (void) DoFFT();

#if BENCHMARKING
        VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
        VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
        VEGASCUDASafeCall(cudaEventElapsedTime(&fTimeFFT, g_cuStart, g_cuStop));
        fAvgFFT = (fTimeFFT + ((g_iCount - 1) * fAvgFFT)) / g_iCount;
#endif

#if (!GPUACCUM)
#if BENCHMARKING
        VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
        VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
        VEGASCUDASafeCall(cudaMemcpy(g_pccFFTOutX,
                   g_pccFFTOutX_d,
                   g_iNFFT * sizeof(cufftComplex),
                   cudaMemcpyDeviceToHost));
        VEGASCUDASafeCall(cudaMemcpy(g_pccFFTOutY,
                   g_pccFFTOutY_d,
                   g_iNFFT * sizeof(cufftComplex),
                   cudaMemcpyDeviceToHost));
#if BENCHMARKING
        VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
        VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
        VEGASCUDASafeCall(cudaEventElapsedTime(&fTimeCpOut, g_cuStart, g_cuStop));
        fAvgCpOut = (fTimeCpOut + ((g_iCount - 1) * fAvgCpOut)) / g_iCount;
#endif
#endif

        /* accumulate power x, power y, stokes, if the blanking bit is
           not set */
        if (!IsBlankingSet())
        {
            if (0/* blanking to non-blanking */)
            {
                /* TODO: when blanking is unset, start accumulating */
                /* reset time */
                iTime = 0;
                /* zero accumulators */
                (void) memset(g_pfSumPowX, '\0', g_iNFFT * sizeof(float));
                (void) memset(g_pfSumPowY, '\0', g_iNFFT * sizeof(float));
                (void) memset(g_pfSumStokesRe, '\0', g_iNFFT * sizeof(float));
                (void) memset(g_pfSumStokesIm, '\0', g_iNFFT * sizeof(float));
            }
            else
            {
#if BENCHMARKING
                VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
                VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
                #if GPUACCUM
                Accumulate<<<g_dimGridAccum, g_dimBlockAccum>>>(g_pccFFTOutX_d,
                                                                g_pccFFTOutY_d,
                                                                g_pfSumPowX_d,
                                                                g_pfSumPowY_d,
                                                                g_pfSumStokesRe_d,
                                                                g_pfSumStokesIm_d);
                VEGASCUDASafeCall(cudaThreadSynchronize());
                #else
                for (i = 0; i < g_iNFFT; ++i)
                {
                    /* Re(X)^2 + Im(X)^2 */
                    g_pfSumPowX[i] += (g_pccFFTOutX[i].x * g_pccFFTOutX[i].x)
                                      + (g_pccFFTOutX[i].y * g_pccFFTOutX[i].y);
                    /* Re(Y)^2 + Im(Y)^2 */
                    g_pfSumPowY[i] += (g_pccFFTOutY[i].x * g_pccFFTOutY[i].x)
                                      + (g_pccFFTOutY[i].y * g_pccFFTOutY[i].y);
                    /* Re(XY*) */
                    g_pfSumStokesRe[i] += (g_pccFFTOutX[i].x * g_pccFFTOutY[i].x)
                                          + (g_pccFFTOutX[i].y * g_pccFFTOutY[i].y);
                    /* Im(XY*) */
                    g_pfSumStokesIm[i] += (g_pccFFTOutX[i].y * g_pccFFTOutY[i].x)
                                          - (g_pccFFTOutX[i].x * g_pccFFTOutY[i].y);
                }
                #endif
#if BENCHMARKING
                VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
                VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
                VEGASCUDASafeCall(cudaEventElapsedTime(&fTimeAccum, g_cuStart, g_cuStop));
                fAvgAccum = (fTimeAccum + ((g_iCount - 1) * fAvgAccum)) / g_iCount;
#endif
                ++iTime;
                if (iTime == iAcc)
                {
                    #if PLOT
                    /* NOTE: Plot() will modify data! */
                    Plot();
                    usleep(500000);
                    #endif

                    /* dump to buffer */
                    #if GPUACCUM
#if BENCHMARKING
                    VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
                    VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
                    VEGASCUDASafeCall(cudaMemcpy(g_pfSumPowX,
                                                 g_pfSumPowX_d,
                                                 g_iNFFT * sizeof(float),
                                                 cudaMemcpyDeviceToHost));
                    VEGASCUDASafeCall(cudaMemcpy(g_pfSumPowY,
                                                 g_pfSumPowY_d,
                                                 g_iNFFT * sizeof(float),
                                                 cudaMemcpyDeviceToHost));
                    VEGASCUDASafeCall(cudaMemcpy(g_pfSumStokesRe,
                                                 g_pfSumStokesRe_d,
                                                 g_iNFFT * sizeof(float),
                                                 cudaMemcpyDeviceToHost));
                    VEGASCUDASafeCall(cudaMemcpy(g_pfSumStokesIm,
                                                 g_pfSumStokesIm_d,
                                                 g_iNFFT * sizeof(float),
                                                 cudaMemcpyDeviceToHost));
#if BENCHMARKING
                    VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
                    VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
                    VEGASCUDASafeCall(cudaEventElapsedTime(&fTimeCpOut, g_cuStart, g_cuStop));
                    fAvgCpOut = (fTimeCpOut + ((g_iCount - 1) * fAvgCpOut)) / g_iCount;
#endif
                    #endif

                    /* reset time */
                    iTime = 0;
                    /* zero accumulators */
                    #if GPUACCUM
                    VEGASCUDASafeCall(cudaMemset(g_pfSumPowX_d, '\0', g_iNFFT * sizeof(float)));
                    VEGASCUDASafeCall(cudaMemset(g_pfSumPowY_d, '\0', g_iNFFT * sizeof(float)));
                    VEGASCUDASafeCall(cudaMemset(g_pfSumStokesRe_d, '\0', g_iNFFT * sizeof(float)));
                    VEGASCUDASafeCall(cudaMemset(g_pfSumStokesIm_d, '\0', g_iNFFT * sizeof(float)));
                    #else
                    (void) memset(g_pfSumPowX, '\0', g_iNFFT * sizeof(float));
                    (void) memset(g_pfSumPowY, '\0', g_iNFFT * sizeof(float));
                    (void) memset(g_pfSumStokesRe, '\0', g_iNFFT * sizeof(float));
                    (void) memset(g_pfSumStokesIm, '\0', g_iNFFT * sizeof(float));
                    #endif
                }
            }
        }
        else
        {
            /* TODO: */
            if (1/* non-blanking to blanking */)
            {
                /* write status, dump data to disk buffer */
            }
            else
            {
                /* do nothing, wait for blanking to stop */
            }
        }

        /* read data from input buffer, convert 8_7 to float */
        iRet = ReadData();
        if (iRet != GUPPI_OK)
        {
            (void) fprintf(stderr, "ERROR: Data reading failed!\n");
            break;
        }
    }
#if (!BENCHMARKING)
    (void) gettimeofday(&stStop, NULL);
    (void) printf("Time taken (barring Init()): %gs\n",
                  ((stStop.tv_sec + (stStop.tv_usec * USEC2SEC))
                   - (stStart.tv_sec + (stStart.tv_usec * USEC2SEC))));
#endif

    CleanUp();

#if BENCHMARKING
    fAvgTotal = g_fAvgCpIn + g_fAvgUnpack + fAvgPFB + fAvgCpInFFT + fAvgFFT + fAvgAccum + fAvgCpOut;
    (void) printf("    Average elapsed time for %d\n", g_iCount);
    (void) printf("        calls to cudaMemcpy(Host2Device)          : %5.3fms, %2d%%\n",
                  g_fAvgCpIn,
                  (int) ((g_fAvgCpIn / fAvgTotal) * 100));
    (void) printf("        calls to Unpack()                         : %5.3fms, %2d%%\n",
                  g_fAvgUnpack,
                  (int) ((g_fAvgUnpack / fAvgTotal) * 100));
    if (g_iIsPFBOn)
    {
        (void) printf("        calls to DoPFB()                          : %5.3fms, %2d%%\n",
                      fAvgPFB,
                      (int) ((fAvgPFB / fAvgTotal) * 100));
    }
    else
    {
        (void) printf("        calls to CopyDataForFFT()                 : %5.3fms, %2d%%\n",
                      fAvgCpInFFT,
                      (int) ((fAvgCpInFFT / fAvgTotal) * 100));
    }
    (void) printf("        calls to DoFFT()                          : %5.3fms, %2d%%\n",
                  fAvgFFT,
                  (int) ((fAvgFFT / fAvgTotal) * 100));
    (void) printf("        calls to Accumulate()/accumulation loop   : %5.3fms, %2d%%\n",
                  fAvgAccum,
                  (int) ((fAvgAccum / fAvgTotal) * 100));
#if GPUACCUM
    (void) printf("        x4 calls to cudaMemcpy(Device2Host)       : %5.3fms, %2d%%\n",
                  fAvgCpOut,
                  (int) ((fAvgCpOut / fAvgTotal) * 100));
#else
    (void) printf("        x2 calls to cudaMemcpy(Device2Host)       : %5.3fms, %2d%%\n",
                  fAvgCpOut,
                  (int) ((fAvgCpOut / fAvgTotal) * 100));
#endif
    VEGASCUDASafeCall(cudaEventDestroy(g_cuStart));
    VEGASCUDASafeCall(cudaEventDestroy(g_cuStop));
    (void) printf("* Events destroyed.\n");
    (void) printf("* Benchmarking run completed.\n");
#endif

    return GUPPI_OK;
}

/* function that creates the FFT plan, allocates memory, initialises counters,
   etc. */
int Init()
{
    int i = 0;
    int j = 0;
    int iDevCount = 0;
    cudaDeviceProp stDevProp = {0};
    int iRet = GUPPI_OK;

    iRet = RegisterSignalHandlers();
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr, "ERROR: Signal-handler registration failed!\n");
        return GUPPI_ERR_GEN;
    }

    VEGASCUDASafeCall(cudaGetDeviceCount(&iDevCount));
    if (0 == iDevCount)
    {
        (void) fprintf(stderr, "ERROR: No CUDA-capable device found!\n");
        return EXIT_FAILURE;
    }
    else if (iDevCount > 1)
    {
        /* TODO: figure this out */
        (void) fprintf(stderr,
                       "ERROR: More than one CUDA-capable device "
                       "found! Don't know how to proceed!\n");
        return EXIT_FAILURE;
    }

    /* TODO: make it automagic */
    VEGASCUDASafeCall(cudaSetDevice(0));

    VEGASCUDASafeCall(cudaGetDeviceProperties(&stDevProp, 0));
    g_iMaxThreadsPerBlock = stDevProp.maxThreadsPerBlock;

    if (g_iIsPFBOn)
    {
        /* set number of taps to NUM_TAPS if PFB is on, else number of
           taps = 1 */
        g_iNTaps = NUM_TAPS;

        g_pacPFBCoeff = (signed char(*) [][NUM_BYTES_PER_SAMP]) malloc(NUM_BYTES_PER_SAMP * g_iNTaps * g_iNFFT * sizeof(signed char));
        if (NULL == g_pacPFBCoeff)
        {
            (void) fprintf(stderr,
                           "ERROR: Memory allocation failed! %s.\n",
                           strerror(errno));
            return GUPPI_ERR_GEN;
        }

        /* allocate memory for the filter coefficient array on the device */
        g_stChanDescPFBCoeff = cudaCreateChannelDesc<signed char>();
        VEGASCUDASafeCall(cudaMallocArray(&g_pcuabPFBCoeff_d,
                                          &g_stChanDescPFBCoeff,
                                          NUM_BYTES_PER_SAMP,
                                          (g_iNTaps * g_iNFFT * sizeof(signed char))));

        /* read filter coefficients */
        /* build file name */
        (void) sprintf(g_acFileCoeff,
                       "%s%d_%d%s",
                       FILE_COEFF_PREFIX,
                       g_iNTaps,
                       g_iNFFT,
                       FILE_COEFF_SUFFIX);
        g_iFileCoeff = open(g_acFileCoeff, O_RDONLY);
        if (GUPPI_ERR_GEN == g_iFileCoeff)
        {
            (void) fprintf(stderr,
                           "ERROR: Opening filter coefficients file %s failed! %s.\n",
                           g_acFileCoeff,
                           strerror(errno));
            return GUPPI_ERR_GEN;
        }

        for (i = 0; i < (g_iNTaps * g_iNFFT); ++i)
        {
            for (j = 0; j < NUM_BYTES_PER_SAMP; ++j)
            {
                iRet = read(g_iFileCoeff,
                            &((*g_pacPFBCoeff)[i][j]),
                            sizeof(signed char));
                if (GUPPI_ERR_GEN == iRet)
                {
                    (void) fprintf(stderr,
                                   "ERROR: Reading filter coefficients failed! %s.\n",
                                   strerror(errno));
                    return GUPPI_ERR_GEN;
                }
            }
        }
        (void) close(g_iFileCoeff);

        /* copy filter coefficients to the device */
        VEGASCUDASafeCall(cudaMemcpy2DToArray(g_pcuabPFBCoeff_d,
                                              0,
                                              0,
                                              g_pacPFBCoeff,
                                              NUM_BYTES_PER_SAMP * sizeof(signed char),
                                              NUM_BYTES_PER_SAMP * sizeof(signed char),
                                              g_iNTaps * g_iNFFT,
                                              cudaMemcpyHostToDevice));
    }

    /* allocate memory for data array contents */
    g_stChanDescData = cudaCreateChannelDesc<signed char>();
    VEGASCUDASafeCall(cudaMallocArray(&g_pcuabData_d,
                                      &g_stChanDescData,
                                      NUM_BYTES_PER_SAMP,
                                      (g_iNTaps * g_iNFFT * sizeof(BYTE))));

    /* temporarily read a file, instead of input buffer */
    g_iFileData = open(g_acFileData, O_RDONLY);
    if (GUPPI_ERR_GEN == g_iFileData)
    {
        (void) fprintf(stderr,
                       "ERROR! Opening data file %s failed! %s.\n",
                       g_acFileData,
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }

    /* load data into memory */
    iRet = LoadData();
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr,
                       "ERROR! Data loading failed!\n");
        return GUPPI_ERR_GEN;
    }

    /* calculate kernel parameters */
    if (g_iNFFT < g_iMaxThreadsPerBlock)
    {
        g_dimBlockPFB.x = g_iNFFT;
        #if GPUACCUM
        g_dimBlockAccum.x = g_iNFFT;
        #endif
    }
    else
    {
        g_dimBlockPFB.x = g_iMaxThreadsPerBlock;
        #if GPUACCUM
        g_dimBlockAccum.x = g_iMaxThreadsPerBlock;
        #endif
    }
    g_dimGridPFB.x = (int) ceilf(((float) g_iNFFT) / g_iMaxThreadsPerBlock);

    g_dimBlockCopy.x = NUM_BYTES_PER_SAMP;
    g_dimBlockCopy.y = (int) (((float) g_iMaxThreadsPerBlock) / NUM_BYTES_PER_SAMP);
    g_dimBlockCopy.z = 1;
    g_dimGridCopy.x = (int) (((float) g_iNFFT) / g_dimBlockCopy.y);
    g_dimGridCopy.y = 1;
    #if GPUACCUM
    g_dimGridAccum.x = (int) ceilf(((float) g_iNFFT) / g_iMaxThreadsPerBlock);
    #endif
    g_pbInBufRead = g_pbInBuf;
    VEGASCUDASafeCall(cudaMemcpy2DToArray(g_pcuabData_d,
                                          0,
                                          0,
                                          g_pbInBufRead,
                                          NUM_BYTES_PER_SAMP * sizeof(BYTE),
                                          NUM_BYTES_PER_SAMP * sizeof(BYTE),
                                          g_iNTaps * g_iNFFT,
                                          cudaMemcpyHostToDevice));
    g_pbInBufRead += g_iNTaps * LEN_DATA;
    g_iReadCount += g_iNTaps;
    if (g_iReadCount == g_iNumReads)
    {
        (void) printf("Data read done!\n");
        g_iIsDone = TRUE;
    }

    /* bind texture to memory */
    VEGASCUDASafeCall(cudaBindTextureToArray(&g_stTexData,
                                             g_pcuabData_d,
                                             &g_stChanDescData));
    if (g_iIsPFBOn)
    {
        VEGASCUDASafeCall(cudaBindTextureToArray(&g_stTexPFBCoeff,
                                                 g_pcuabPFBCoeff_d,
                                                 &g_stChanDescPFBCoeff));
    }

    g_iPFBWriteIdx = 0;     /* next write into the first buffer */
    g_iPFBReadIdx = 0;      /* PFB to be performed from first buffer */

    g_pccFFTInX = (cufftComplex *) malloc(g_iNFFT * sizeof(cufftComplex));
    if (NULL == g_pccFFTInX)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pccFFTInX_d,
                          g_iNFFT * sizeof(cufftComplex)));
    g_pccFFTInY = (cufftComplex *) malloc(g_iNFFT * sizeof(cufftComplex));
    if (NULL == g_pccFFTInY)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pccFFTInY_d,
                      g_iNFFT * sizeof(cufftComplex)));
    g_pccFFTOutX = (cufftComplex *) malloc(g_iNFFT * sizeof(cufftComplex));
    if (NULL == g_pccFFTOutX)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pccFFTOutX_d,
                      g_iNFFT * sizeof(cufftComplex)));
    g_pccFFTOutY = (cufftComplex *) malloc(g_iNFFT * sizeof(cufftComplex));
    if (NULL == g_pccFFTOutY)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pccFFTOutY_d,
                      g_iNFFT * sizeof(cufftComplex)));

    g_pfSumPowX = (float *) calloc(g_iNFFT, sizeof(float));
    if (NULL == g_pfSumPowX)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    g_pfSumPowY = (float *) calloc(g_iNFFT, sizeof(float));
    if (NULL == g_pfSumPowY)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    g_pfSumStokesRe = (float *) calloc(g_iNFFT, sizeof(float));
    if (NULL == g_pfSumStokesRe)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    g_pfSumStokesIm = (float *) calloc(g_iNFFT, sizeof(float));
    if (NULL == g_pfSumStokesIm)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
#if GPUACCUM
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pfSumPowX_d, g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMemset(g_pfSumPowX_d, '\0', g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pfSumPowY_d, g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMemset(g_pfSumPowY_d, '\0', g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pfSumStokesRe_d, g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMemset(g_pfSumStokesRe_d, '\0', g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMalloc((void **) &g_pfSumStokesIm_d, g_iNFFT * sizeof(float)));
    VEGASCUDASafeCall(cudaMemset(g_pfSumStokesIm_d, '\0', g_iNFFT * sizeof(float)));
#endif

    /* create plans */
    (void) cufftPlan1d(&g_stPlanX, g_iNFFT, CUFFT_C2C, 1);
    (void) cufftPlan1d(&g_stPlanY, g_iNFFT, CUFFT_C2C, 1);

#if PLOT
    /* just for plotting */
    InitPlot();
#endif

    return GUPPI_OK;
}

/* function that reads data from the data file and loads it into memory during
   initialisation */
int LoadData()
{
    struct stat stFileStats = {0};
    int iRet = GUPPI_OK;

    iRet = stat(g_acFileData, &stFileStats);
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr,
                       "ERROR: Failed to stat %s: %s!\n",
                       g_acFileData,
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }

    g_pbInBuf = (BYTE *) malloc(stFileStats.st_size * sizeof(BYTE));
    if (NULL == g_pbInBuf)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }

    iRet = read(g_iFileData, g_pbInBuf, stFileStats.st_size);
    if (GUPPI_ERR_GEN == iRet)
    {
        (void) fprintf(stderr,
                       "ERROR: Data reading failed! %s.\n",
                       strerror(errno));
        return GUPPI_ERR_GEN;
    }
    else if (iRet != stFileStats.st_size)
    {
        (void) printf("File read done!\n");
    }

    /* calculate the number of reads required */
    g_iNumReads = stFileStats.st_size / LEN_DATA;

    return GUPPI_OK;
}

/* function that reads data from input buffer */
int ReadData()
{
    /* write new data to the write buffer */
#if BENCHMARKING
    VEGASCUDASafeCall(cudaEventRecord(g_cuStart, 0));
    VEGASCUDASafeCall(cudaEventSynchronize(g_cuStart));
#endif
    VEGASCUDASafeCall(cudaMemcpy2DToArray(g_pcuabData_d,
                                          0,
                                          g_iPFBWriteIdx * g_iNFFT,
                                          g_pbInBufRead,
                                          NUM_BYTES_PER_SAMP * sizeof(BYTE),
                                          NUM_BYTES_PER_SAMP * sizeof(BYTE),
                                          g_iNFFT,
                                          cudaMemcpyHostToDevice));
#if BENCHMARKING
    VEGASCUDASafeCall(cudaEventRecord(g_cuStop, 0));
    VEGASCUDASafeCall(cudaEventSynchronize(g_cuStop));
    VEGASCUDASafeCall(cudaEventElapsedTime(&g_fTimeCpIn, g_cuStart, g_cuStop));
    g_fAvgCpIn = (g_fTimeCpIn + ((g_iCount - 1) * g_fAvgCpIn)) / g_iCount;
#endif
    g_pbInBufRead += LEN_DATA;
    ++g_iReadCount;
    if (g_iReadCount == g_iNumReads)
    {
        (void) printf("Data read done!\n");
        g_iIsDone = TRUE;
    }

    if (g_iPFBWriteIdx != (g_iNTaps - 1))
    {
        ++g_iPFBWriteIdx;
    }
    else
    {
        g_iPFBWriteIdx = 0;
    }
    if (g_iPFBReadIdx != (g_iNTaps - 1))
    {
        ++g_iPFBReadIdx;
    }
    else
    {
        g_iPFBReadIdx = 0;
    }

    return GUPPI_OK;
}

/* function that performs the PFB */
__global__ void DoPFB(int iPFBReadIdx,
                      int iNTaps,
                      cufftComplex *pccFFTInX,
                      cufftComplex *pccFFTInY)
{
#if 0
    int i = iPFBReadIdx;
    int j = 0;
    int k = (blockIdx.x * blockDim.x) + threadIdx.x;
    int iNFFT = gridDim.x * blockDim.x;
    cufftComplex ccAccumX;
    cufftComplex ccAccumY;
    signed char (*pabData)[][NUM_BYTES_PER_SAMP] = (signed char(*) [][NUM_BYTES_PER_SAMP]) pbData;

    ccAccumX.x = 0.0;
    ccAccumX.y = 0.0;
    ccAccumY.x = 0.0;
    ccAccumY.y = 0.0;

    for (j = 0; j < iNTaps; ++j)
    {
        ccAccumX.x += (*pabData)[(i * iNFFT) + k][0] * tex2D(g_stTexPFBCoeff, 0, (j * iNFFT) + k);
        ccAccumX.y += (*pabData)[(i * iNFFT) + k][1] * tex2D(g_stTexPFBCoeff, 1, (j * iNFFT) + k);
        ccAccumY.x += (*pabData)[(i * iNFFT) + k][2] * tex2D(g_stTexPFBCoeff, 2, (j * iNFFT) + k);
        ccAccumY.y += (*pabData)[(i * iNFFT) + k][3] * tex2D(g_stTexPFBCoeff, 3, (j * iNFFT) + k);
        if (i != (iNTaps - 1))
        {
            ++i;
        }
        else
        {
            i = 0;
        }
    }

    pccFFTInX[k] = ccAccumX;
    pccFFTInY[k] = ccAccumY;
#endif
    return;
}

#if 0
__global__ void DoPFB(signed char *pbDataX,
                      signed char *pbDataY,
                      int iPFBReadIdx,
                      int iNTaps,
                      signed char *pcPFBCoeff,
                      cufftComplex *pccFFTInX,
                      cufftComplex *pccFFTInY)
{
    int i = iPFBReadIdx;
    int j = 0;
    int k = (blockIdx.x * blockDim.x) + threadIdx.x;
    int iNFFT = gridDim.x * blockDim.x;
    cufftComplex ccAccumX;
    cufftComplex ccAccumY;
    signed char (*pabDataX)[][2] = (signed char(*) [][2]) pbDataX;
    signed char (*pabDataY)[][2] = (signed char(*) [][2]) pbDataY;

    ccAccumX.x = 0.0;
    ccAccumX.y = 0.0;
    ccAccumY.x = 0.0;
    ccAccumY.y = 0.0;

    for (j = 0; j < iNTaps; ++j)
    {
        ccAccumX.x += (*pabDataX)[(i * iNFFT) + k][0] * pcPFBCoeff[(j * iNFFT) + k];
        ccAccumX.y += (*pabDataX)[(i * iNFFT) + k][1] * pcPFBCoeff[(j * iNFFT) + k];
        ccAccumY.x += (*pabDataY)[(i * iNFFT) + k][0] * pcPFBCoeff[(j * iNFFT) + k];
        ccAccumY.y += (*pabDataY)[(i * iNFFT) + k][1] * pcPFBCoeff[(j * iNFFT) + k];
        if (i != (iNTaps - 1))
        {
            ++i;
        }
        else
        {
            i = 0;
        }
    }

    pccFFTInX[k] = ccAccumX;
    pccFFTInY[k] = ccAccumY;

    return;
}
#endif

#if 0
__global__ void CopyDataForFFT(cufftComplex *pccFFTInX,
                               cufftComplex *pccFFTInY)
{
    int i = (blockIdx.x * blockDim.y) + threadIdx.y;
    int x = threadIdx.x;
    float f = 0.0;

    f = tex2D(g_stTexData, x, i);
    switch (x)
    {
        case 0: pccFFTInX[i].x = f; break;
        case 1: pccFFTInX[i].y = f; break;
        case 2: pccFFTInY[i].x = f; break;
        case 3: pccFFTInY[i].y = f; break;
    }

    return;
}
#else
__global__ void CopyDataForFFT(cudaArray *pcuabData,
                               cufftComplex *pccFFTInX,
                               cufftComplex *pccFFTInY)
{
    int i = (blockIdx.x * blockDim.y) + threadIdx.y;
    int x = threadIdx.x;
    signed char (*pabData)[][4] = (signed char(*) [][4]) pcuabData;
    __shared__ float afTile[128][4];

    afTile[i][x] = (*pabData)[i][x];

    __syncthreads();

    switch (x)
    {
        case 0: pccFFTInX[i].x = afTile[i][x]; break;
        case 1: pccFFTInX[i].y = afTile[i][x]; break;
        case 2: pccFFTInY[i].x = afTile[i][x]; break;
        case 3: pccFFTInY[i].y = afTile[i][x]; break;
    }

    return;
}
#endif

/* function that performs the FFT */
int DoFFT()
{
    /* execute plan */
    (void) cufftExecC2C(g_stPlanX, g_pccFFTInX_d, g_pccFFTOutX_d, CUFFT_FORWARD);
    (void) cufftExecC2C(g_stPlanY, g_pccFFTInY_d, g_pccFFTOutY_d, CUFFT_FORWARD);

    return GUPPI_OK;
}

#if GPUACCUM
__global__ void Accumulate(cufftComplex *pccFFTOutX,
                           cufftComplex *pccFFTOutY,
                           float *pfSumPowX,
                           float *pfSumPowY,
                           float *pfSumStokesRe,
                           float *pfSumStokesIm)
{
    int i = (blockIdx.x * blockDim.x) + threadIdx.x;

    /* Re(X)^2 + Im(X)^2 */
    pfSumPowX[i] += (pccFFTOutX[i].x * pccFFTOutX[i].x)
                    + (pccFFTOutX[i].y * pccFFTOutX[i].y);
    /* Re(Y)^2 + Im(Y)^2 */
    pfSumPowY[i] += (pccFFTOutY[i].x * pccFFTOutY[i].x)
                    + (pccFFTOutY[i].y * pccFFTOutY[i].y);
    /* Re(XY*) */
    pfSumStokesRe[i] += (pccFFTOutX[i].x * pccFFTOutY[i].x)
                        + (pccFFTOutX[i].y * pccFFTOutY[i].y);
    /* Im(XY*) */
    pfSumStokesIm[i] += (pccFFTOutX[i].y * pccFFTOutY[i].x)
                        - (pccFFTOutX[i].x * pccFFTOutY[i].y);

    return;
}
#endif

int IsRunning()
{
    return (!g_iIsDone);
}

int IsBlankingSet()
{
    /* check for status and return TRUE or FALSE */
    return FALSE;
}

/* function that frees resources */
void CleanUp()
{
    /* free resources */
    free(g_pbInBuf);

    (void) cudaFreeArray(g_pcuabData_d);
    free(g_pccFFTInX);
    (void) cudaFree(g_pccFFTInX_d);
    free(g_pccFFTInY);
    (void) cudaFree(g_pccFFTInY_d);
    free(g_pccFFTOutX);
    (void) cudaFree(g_pccFFTOutX_d);
    free(g_pccFFTOutY);
    (void) cudaFree(g_pccFFTOutY_d);

    free(g_pacPFBCoeff);
    (void) cudaFreeArray(g_pcuabPFBCoeff_d);

    free(g_pfSumPowX);
    free(g_pfSumPowY);
    free(g_pfSumStokesRe);
    free(g_pfSumStokesIm);

    /* destroy plans */
    (void) cufftDestroy(g_stPlanX);
    (void) cufftDestroy(g_stPlanY);

    (void) close(g_iFileData);

#if PLOT
    /* for plotting */
    free(g_pfFreq);
    cpgclos();
#endif

    return;
}

#if PLOT
void InitPlot()
{
    int iRet = GUPPI_OK;
    int i = 0;

    iRet = cpgopen(PG_DEV);
    if (iRet <= 0)
    {
        (void) fprintf(stderr,
                       "ERROR: Opening graphics device %s failed!\n",
                       PG_DEV);
        return;
    }

    cpgsch(2);
    cpgsubp(1, 4);

    g_pfFreq = (float *) malloc(g_iNFFT * sizeof(float));
    if (NULL == g_pfFreq)
    {
        (void) fprintf(stderr,
                       "ERROR: Memory allocation failed! %s.\n",
                       strerror(errno));
        return;
    }

    /* load the frequency axis */
    for (i = 0; i < g_iNFFT; ++i)
    {
        g_pfFreq[i] = ((float) i * g_fFSamp) / g_iNFFT;
    }

    return;
}

void Plot()
{
    float fMinFreq = g_pfFreq[0];
    float fMaxFreq = g_pfFreq[g_iNFFT-1];
    float fMinY = FLT_MAX;
    float fMaxY = -(FLT_MAX);
    int i = 0;

    /* take log10 of data */
    for (i = 0; i < g_iNFFT; ++i)
    {
        g_pfSumPowX[i] = 10 * log10f(g_pfSumPowX[i]);
        g_pfSumPowY[i] = 10 * log10f(g_pfSumPowY[i]);
        g_pfSumStokesRe[i] = log10f(g_pfSumStokesRe[i]);
        g_pfSumStokesIm[i] = log10f(g_pfSumStokesIm[i]);
    }

    /* plot g_pfSumPowX */
    for (i = 0; i < g_iNFFT; ++i)
    {
        if (g_pfSumPowX[i] > fMaxY)
        {
            fMaxY = g_pfSumPowX[i];
        }
        if (g_pfSumPowX[i] < fMinY)
        {
            fMinY = g_pfSumPowX[i];
        }
    }
    cpgpanl(1, 1);
    cpgeras();
    cpgsvp(PG_VP_ML, PG_VP_MR, PG_VP_MB, PG_VP_MT);
    cpgswin(fMinFreq, fMaxFreq, fMinY, fMaxY);
    cpglab("Bin Number",
           "",
           "SumPowX");
    cpgbox("BCNST", 0.0, 0, "BCNST", 0.0, 0);
    cpgsci(PG_CI_PLOT);
    cpgline(g_iNFFT, g_pfFreq, g_pfSumPowX);
    cpgsci(PG_CI_DEF);

    /* plot g_pfSumPowY */
    fMinY = FLT_MAX;
    fMaxY = -(FLT_MAX);
    for (i = 0; i < g_iNFFT; ++i)
    {
        if (g_pfSumPowY[i] > fMaxY)
        {
            fMaxY = g_pfSumPowY[i];
        }
        if (g_pfSumPowY[i] < fMinY)
        {
            fMinY = g_pfSumPowY[i];
        }
    }
    for (i = 0; i < g_iNFFT; ++i)
    {
        g_pfSumPowY[i] -= fMaxY;
        //printf("%g\n", g_pfSumPowY[i]);
    }
    fMinY -= fMaxY;
    fMaxY = 0;
    //printf("********************************\n");
    cpgpanl(1, 2);
    cpgeras();
    cpgsvp(PG_VP_ML, PG_VP_MR, PG_VP_MB, PG_VP_MT);
    cpgswin(fMinFreq, fMaxFreq, fMinY, fMaxY);
    cpglab("Bin Number",
           "",
           "SumPowY");
    cpgbox("BCNST", 0.0, 0, "BCNST", 0.0, 0);
    cpgsci(PG_CI_PLOT);
    cpgline(g_iNFFT, g_pfFreq, g_pfSumPowY);
    cpgsci(PG_CI_DEF);

    /* plot g_pfSumStokesRe */
    fMinY = FLT_MAX;
    fMaxY = -(FLT_MAX);
    for (i = 0; i < g_iNFFT; ++i)
    {
        if (g_pfSumStokesRe[i] > fMaxY)
        {
            fMaxY = g_pfSumStokesRe[i];
        }
        if (g_pfSumStokesRe[i] < fMinY)
        {
            fMinY = g_pfSumStokesRe[i];
        }
    }
    cpgpanl(1, 3);
    cpgeras();
    cpgsvp(PG_VP_ML, PG_VP_MR, PG_VP_MB, PG_VP_MT);
    cpgswin(fMinFreq, fMaxFreq, fMinY, fMaxY);
    cpglab("Bin Number",
           "",
           "SumStokesRe");
    cpgbox("BCNST", 0.0, 0, "BCNST", 0.0, 0);
    cpgsci(PG_CI_PLOT);
    cpgline(g_iNFFT, g_pfFreq, g_pfSumStokesRe);
    cpgsci(PG_CI_DEF);

    /* plot g_pfSumStokesIm */
    fMinY = FLT_MAX;
    fMaxY = -(FLT_MAX);
    for (i = 0; i < g_iNFFT; ++i)
    {
        if (g_pfSumStokesIm[i] > fMaxY)
        {
            fMaxY = g_pfSumStokesIm[i];
        }
        if (g_pfSumStokesIm[i] < fMinY)
        {
            fMinY = g_pfSumStokesIm[i];
        }
    }
    cpgpanl(1, 4);
    cpgeras();
    cpgsvp(PG_VP_ML, PG_VP_MR, PG_VP_MB, PG_VP_MT);
    cpgswin(fMinFreq, fMaxFreq, fMinY, fMaxY);
    cpglab("Bin Number",
           "",
           "SumStokesIm");
    cpgbox("BCNST", 0.0, 0, "BCNST", 0.0, 0);
    cpgsci(PG_CI_PLOT);
    cpgline(g_iNFFT, g_pfFreq, g_pfSumStokesIm);
    cpgsci(PG_CI_DEF);

    return;
}
#endif

/*
 * Registers handlers for SIGTERM and CTRL+C
 */
int RegisterSignalHandlers()
{
    struct sigaction stSigHandler = {{0}};
    int iRet = GUPPI_OK;

    /* register the CTRL+C-handling function */
    stSigHandler.sa_handler = HandleStopSignals;
    iRet = sigaction(SIGINT, &stSigHandler, NULL);
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr,
                       "ERROR: Handler registration failed for signal %d!\n",
                       SIGINT);
        return GUPPI_ERR_GEN;
    }

    /* register the SIGTERM-handling function */
    stSigHandler.sa_handler = HandleStopSignals;
    iRet = sigaction(SIGTERM, &stSigHandler, NULL);
    if (iRet != GUPPI_OK)
    {
        (void) fprintf(stderr,
                       "ERROR: Handler registration failed for signal %d!\n",
                       SIGTERM);
        return GUPPI_ERR_GEN;
    }

    return GUPPI_OK;
}

/*
 * Catches SIGTERM and CTRL+C and cleans up before exiting
 */
void HandleStopSignals(int iSigNo)
{
    /* clean up */
    CleanUp();

    /* exit */
    exit(GUPPI_OK);

    /* never reached */
    return;
}

void __VEGASCUDASafeCall(cudaError_t iRet,
                         const char* pcFile,
                         const int iLine,
                         void (*pCleanUp)(void))
{
    if (iRet != cudaSuccess)
    {
        (void) fprintf(stderr,
                       "ERROR: File <%s>, Line %d: %s\n",
                       pcFile,
                       iLine,
                       cudaGetErrorString(iRet));
        /* free resources */
        (*pCleanUp)();
        exit(GUPPI_ERR_GEN);
    }

    return;
}

/*
 * Prints usage information
 */
void PrintUsage(const char *pcProgName)
{
    (void) printf("Usage: %s [options] <data-file>\n",
                  pcProgName);
    (void) printf("    -h  --help                           ");
    (void) printf("Display this usage information\n");
    (void) printf("    -n  --nfft <value>                   ");
    (void) printf("Number of points in FFT\n");
    (void) printf("    -p  --pfb                            ");
    (void) printf("Enable PFB\n");
    (void) printf("    -a  --nacc <value>                   ");
    (void) printf("Number of spectra to add\n");
#if PLOT
    (void) printf("    -s  --fsamp <value>                  ");
    (void) printf("Sampling frequency\n");
#endif

    return;
}

