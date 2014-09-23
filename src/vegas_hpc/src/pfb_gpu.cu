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

#include "BlankingStateMachine.h"
#include "gpu_context.h"

#define STATUS_KEY "GPUSTAT"

/* ASSUMPTIONS: 1. All blocks contain the same number of heaps. */

extern int run;

/**
 * Global variables: maybe move this to a struct that is passed to each function?
 */
#include "gpu_context.h"


// Make the damn object global until we complete refactoring ...
GpuContext *gpuCtx = 0;

// static size_t g_buf_out_block_size;
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
                   double heap_mjd,                      // MJD from index_input
                   int first_t_series_status)            // switch state of first accumulation
{
    struct freq_spead_heap *freq_heap_out;
    char * payload_addr_out;
    struct databuf_index *index_out;
    int rtn;
    
    freq_heap_out = vegas_datablock_freq_heap_header(db_out, curblk_out, iHeapOut);
    index_out = (struct databuf_index*)vegas_databuf_index(db_out, curblk_out);

    if (sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK + 
        index_out->heap_size*(index_out->num_heaps+1) > db_out->block_size ||
        iHeapOut >= db_out->index_size)
    {
        printf("DATABUF ERROR: heapsize*nheaps > blocksize!! (%d > %zd) index_size=%d\n",
            index_out->heap_size*index_out->num_heaps, db_out->block_size, db_out->index_size); 
        printf("DATABUF ERROR: blocknum=%d, iHeapOut=%d,iTotHeapOut=%d,iSpecPerAcc=%d\n",
            curblk_out, iHeapOut,iTotHeapOut, iSpecPerAcc);
    } 
        
    payload_addr_out = vegas_datablock_freq_heap_data(db_out, curblk_out, iHeapOut);

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
    freq_heap_out->status_bits = first_t_series_status;
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
    int nsubband_x_nchan;
    size_t nsubband_x_nchan_fsize;
    size_t nsubband_x_nchan_csize;

    nsubband_x_nchan = gpuCtx->_nsubband * gpuCtx->_nchan;
    nsubband_x_nchan_fsize = nsubband_x_nchan * sizeof(float4);
    nsubband_x_nchan_csize = nsubband_x_nchan * sizeof(char4);
    
    /* Setup input and first output data block stuff */
    index_in = (struct databuf_index*)vegas_databuf_index(db_in, curblock_in);
    /* Get the number of heaps per block of data that will be processed by the GPU */
    num_in_heaps_per_proc = nsubband_x_nchan_csize / time_heap_datasize(index_in);
    iBlockInDataSize = index_in->num_heaps * time_heap_datasize(index_in);

    num_in_heaps_tail = ((VEGAS_NUM_TAPS - 1) * nsubband_x_nchan_csize)
                         / time_heap_datasize(index_in);
    num_in_heaps_gpu_buffer = index_in->num_heaps + num_in_heaps_tail;

    /* Calculate the maximum number of output heaps per block */
    g_iMaxNumHeapOut = (gpuCtx->_out_block_size - (sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK)) / nsubband_x_nchan_fsize;

    hdr_out = vegas_databuf_header(db_out, *curblock_out);
    index_out = (struct databuf_index*)vegas_databuf_index(db_out, *curblock_out);
    // index_out->num_heaps = 0;
    memcpy(hdr_out, vegas_databuf_header(db_in, curblock_in), VEGAS_STATUS_SIZE);

    /* Set basic params in output index */
    index_out->heap_size = sizeof(struct freq_spead_heap) + (nsubband_x_nchan_fsize);
    /* Read in heap from buffer */
    heap_addr_in = (char*)vegas_datablock_time_heap_header(db_in, curblock_in, heap_in);

    /* Here, the payload_addr_in is the start of the contiguous block of data that will be
       copied to the GPU (heap_in = 0) */
    payload_addr_in = vegas_datablock_time_heap_data(db_in, curblock_in, heap_in);

    /* Copy data block to GPU */
    if (first)
    {
        // bloksz replaces a calculated value which caused the check below to fail
        // in the presence of dropped packets. We used the blocksize from the data buffer
        // instead here to get things going. Not sure how dropped data at start of
        // scan should be treated.
        int bloksz;
        bloksz = iBlockInDataSize; // calculated above
        /* Sanity check for the first iteration */
        if ((bloksz % (nsubband_x_nchan_csize)) != 0)
        {
            (void) fprintf(stderr, "ERROR: Data size mismatch! BlockInDataSize=%d NumSubBands=%d nchan=%d\n",
                                    bloksz, gpuCtx->_nsubband, gpuCtx->_nchan);
            run = 0;
            return;
        }
        // Cuda Note: cudaMemcpy host to device is asynchronous, be supposedly safe.
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d,
                                payload_addr_in,
                                bloksz,
                                cudaMemcpyHostToDevice));
        CUDA_SAFE_CALL(cudaThreadSynchronize());
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
        }
                                
        /* duplicate the last (VEGAS_NUM_TAPS - 1) segments at the end for 
           the next iteration */
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d + (bloksz / sizeof(char4)),
                                  gpuCtx->_pc4Data_d + (bloksz - ((VEGAS_NUM_TAPS - 1) * nsubband_x_nchan_csize))/sizeof(char4),
                                  ((VEGAS_NUM_TAPS - 1) * nsubband_x_nchan_csize),
                                  cudaMemcpyDeviceToDevice));
        CUDA_SAFE_CALL(cudaThreadSynchronize());
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
        }

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
        // Zero out accumulators for 1st integration
        gpuCtx->zero_accumulator();
        printf("num_heaps=%d num_in_tail=%d\n", index_in->num_heaps, num_in_heaps_tail);
    }
    else
    {
        /* If this is not the first run, need to handle block boundary, while doing the PFB */
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d,
                                gpuCtx->_pc4Data_d + (iBlockInDataSize / sizeof(char4)),
                                ((VEGAS_NUM_TAPS - 1) * nsubband_x_nchan_csize),
                                cudaMemcpyDeviceToDevice));
        CUDA_SAFE_CALL(cudaThreadSynchronize());
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
        }
                                
        // Cuda Note: cudaMemcpy host to device is asynchronous, be supposedly safe.                                
        CUDA_SAFE_CALL(cudaMemcpy(gpuCtx->_pc4Data_d + ((VEGAS_NUM_TAPS - 1) * nsubband_x_nchan),
                                payload_addr_in,
                                iBlockInDataSize,
                                cudaMemcpyHostToDevice));
        CUDA_SAFE_CALL(cudaThreadSynchronize());
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
        }
                                
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
                iProcData += (VEGAS_NUM_TAPS * nsubband_x_nchan_csize);
                /* update the data read pointer */
                gpuCtx->_pc4DataRead_d += (VEGAS_NUM_TAPS * nsubband_x_nchan);
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
                heap_addr_in = (char*)vegas_datablock_time_heap_header(db_in, curblock_in, heap_in);
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

        gpuCtx->blanking_inputs(is_blanked(heap_in, num_in_heaps_per_proc));
        ++g_iTotHeapOut; // unconditional spectrum counter
                                
        /* Accumulate power x, power y, stokes real and imag, if the blanking
           bit is not set */
        if (!(gpuCtx->blank_current_fft()))
        {
            iRet = gpuCtx->accumulate();
            if (iRet != VEGAS_OK)
            {
                (void) fprintf(stdout, "ERROR: Accumulation failed!\n");
                run = 0;
                break;
            }
            ++g_iSpecPerAcc;
            // record the first unblanked state in this accumulation sequence
            if (1 == g_iSpecPerAcc)
            {
                gpuCtx->_first_time_heap_in_accum_status_bits = g_auiStatusBits[heap_in];
                memcpy(&gpuCtx->_first_time_heap_in_accum, heap_addr_in, sizeof(gpuCtx->_first_time_heap_in_accum));
                gpuCtx->_first_time_heap_mjd = index_in->cpu_gpu_buf[heap_in].heap_rcvd_mjd;               
            }                    
        }
        
        if (g_iSpecPerAcc == acc_len || gpuCtx->needs_flush())
        {
            /* dump to buffer */
            // If no accumulations have occurred, then just clear the accumulator and start again.
            if (g_iSpecPerAcc > 0)
            {
                iRet = dump_to_buffer(db_out,             
                                      *curblock_out,
                                      g_iHeapOut,
                                      &gpuCtx->_first_time_heap_in_accum,
                                      g_iTotHeapOut,
                                      g_iSpecPerAcc,
                                      gpuCtx->_first_time_heap_mjd,
                                      gpuCtx->_first_time_heap_in_accum_status_bits);
            
                if (iRet != VEGAS_OK)
                {
                    (void) fprintf(stdout, "ERROR: Getting accumulated spectrum failed!\n");
                    run = 0;
                    break;
                }                                  
                ++g_iHeapOut;
            }
            else
            {
                printf("Scanlength: GPU:asked to dump buffer but no accumulations present\n");
            }

            /* zero accumulators */
            gpuCtx->zero_accumulator();
            /* reset time */
            g_iSpecPerAcc = 0;
        }

        iProcData += nsubband_x_nchan_csize;
        /* update the data read pointer */
        gpuCtx->_pc4DataRead_d += (nsubband_x_nchan);

        /* Calculate input heap addresses for the next round of processing */
        heap_in += num_in_heaps_per_proc;
        heap_addr_in = (char*)vegas_datablock_time_heap_header(db_in, curblock_in, heap_in);

        /* if output block is full */
        if (g_iHeapOut == g_iMaxNumHeapOut)
        {
            /* Set the number of heaps written to this block */
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
            index_out->heap_size = sizeof(struct freq_spead_heap) + (nsubband_x_nchan_fsize);
        }

        pfb_count = (pfb_count + 1) % VEGAS_NUM_TAPS;
    }

    return;
}

/* function that performs the FFT */
int GpuContext::do_fft()
{
    cufftResult iCUFFTRet = CUFFT_SUCCESS;
    cudaError_t iCUDARet = cudaSuccess;

    /* execute plan */
    iCUFFTRet = cufftExecC2C(_stPlan,
                             (cufftComplex*) _pf4FFTIn_d,
                             (cufftComplex*) _pf4FFTOut_d,
                             CUFFT_FORWARD);
    if (iCUFFTRet != CUFFT_SUCCESS)
    {
        (void) fprintf(stderr, "ERROR: FFT failed!");
        iCUDARet = cudaGetLastError();
        if (iCUDARet != cudaSuccess)
        {
            (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
            run = 0;
            return VEGAS_ERR_GEN;
        }
    }
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
    cudaError_t iCUDARet = cudaSuccess;
    CUDA_SAFE_CALL(cudaMemset(_pf4SumStokes_d,
                                       '\0',
                                       (_nsubband
                                       * _nchan
                                       * sizeof(float4))));
    CUDA_SAFE_CALL(cudaThreadSynchronize());                                       
    if (iCUDARet != cudaSuccess)
    {
        (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
    }

    return;
}

int GpuContext::get_accumulated_spectrum_from_device(char *out)
{
    cudaError_t iCUDARet = cudaSuccess;
    // Cuda note: Device to host memcpy is always synchronous
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
    iCUDARet = cudaGetLastError();
    if (iCUDARet != cudaSuccess)
    {
        (void) fprintf(stderr, cudaGetErrorString(iCUDARet));
    }

#ifdef DEBUG_ZERO_CHANNELS    
    // DEBUG check for near or zero channels X*X and Y*Y should never be =< 0.0                            
    int i, ndata, n_null = 0;
    ndata = _nsubband*_nchan;
    float4 *data = (float4 *)out;
    int first_bad = 0;
    
    for (i=0; i<ndata; ++i)
    {
        if (data[i].x <= 0.0 || data[i].y <= 0.0)
        {
            n_null++;
            first_bad=first_bad == 0 ? i : first_bad;
        }
    }
    if (n_null != 0)
    {
        printf("GPU: %d nil channels starting at %d\n", n_null, first_bad);
    }
#endif
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
A note about blanking:
The blanking status is copied from the time series input into the array
g_auiStatusBits, with time acending with index, like so:
    g_auiStatusBits[32] = t0
    g_auiStatusBits[33] = t0 + dt
    g_auiStatusBits[34] = t0 + dt + dt
    
So when we think about labeling the frequency heap outputs, the convention
is to use the '1st' non-blanked time-series (e.g. index 32 above) to fill 
in the timestamp, counter, mjd etc.

However, when we think about how to process blanking, we need to use the
most recent(e.g index 34 above), status to drive the blanking state machine.
Below, the check which sets 0x2 is taken from the most current time-series status.

 is_blanked(tail, length)
 * Check the input time series for blanking and encode
 * the result.
 * Return value: 
 *  - bit 0x4 -- indicates cal or sig/ref state changed during input
 *  - bit 0x2 -- indicates if the most recent time sample had blanking asserted
 *  - bit 0x1 -- indicates if any of the time samples had blanking asserted
 */
int is_blanked(int heap_start, int num_heaps)
{
    int state_changed = 0;
    int banked_at_start = (g_auiStatusBits[heap_start + num_heaps- 1] & 0x8)  ? 0x2 : 0x0;
    int is_blanked = (banked_at_start || (g_auiStatusBits[heap_start] & 0x8)) ? 0x1 : 0x0;
   
    for (int i = heap_start + 1; i < (heap_start + num_heaps); ++i)
    {
        if ((g_auiStatusBits[i] & 0x3) != (g_auiStatusBits[i-1] & 0x3))
        {
            state_changed = 0x4;
        }
        if (g_auiStatusBits[i] & 0x08)
        {
            is_blanked = 0x1;
        }
    }
    return (banked_at_start | state_changed | is_blanked);
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
