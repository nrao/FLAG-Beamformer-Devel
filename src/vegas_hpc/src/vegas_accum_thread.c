/* vegas_accum_thread.c
 *
 * Adds the heaps, received from the network, to the appropriate accumulators.
 * At set intervals (e.g. every 1 second) the accumulators are dumped to the output
 * buffer, for writing to the disk.
 */

#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <stdint.h>
#include <math.h>

#include "vegas_defines.h"
#include "fitshead.h"
#include "sdfits.h"
#include "vegas_error.h"
#include "vegas_status.h"
#include "vegas_databuf.h"
#include "spead_heap.h"

#define STATUS_KEY "ACCSTAT"
#include "vegas_threads.h"

#define NUM_SW_STATES   8
#define MAX_NUM_SUB     8
#define MAX_NUM_CH      32768
#define NUM_STOKES      4

#define INT_PAYLOAD     1
#define FLOAT_PAYLOAD   2

#define BLANKING_MASK   (0x8)
#define CAL_SR_MASK     (0x7)

struct BlockStats
{
    int nblock_int;
    int npacket;
    int n_pkt_drop;
    int n_heap_drop;
};

// Read a status buffer all of the key observation paramters
extern void vegas_read_obs_params(char *buf, 
                                  struct vegas_params *g, 
                                  struct sdfits *p);

/* Parse info from buffer into param struct */
extern void vegas_read_subint_params(char *buf, 
                                     struct vegas_params *g,
                                     struct sdfits *p);
                                     
extern int check_scan_length(int64_t time_counter, double fpgafreq, double scanlen);

void write_full_integration(struct vegas_databuf *db_out, int *cur_block_out, 
                       struct vegas_databuf *db_in,  int  cur_block_in, 
                       char *accum_dirty, float **accumulator, 
                       struct sdfits_data_columns* data_cols,
                       struct sdfits *sdf,struct BlockStats *blkstat);

/* Dynamically allocates memory for the vector accumulators */
void create_accumulators(float ***accumulator, int num_chans, int num_subbands)
{
    int i;

    /* Want: float accumulator[NUM_SW_STATES][num_chan][num_subband][NUM_STOKES] */

    *accumulator = malloc(NUM_SW_STATES * sizeof(float*));

    if(*accumulator == NULL) {
        vegas_error("vegas_net_thread", "malloc failed");
        pthread_exit(NULL);
    }

    for(i = 0; i < NUM_SW_STATES; i++)
    {
        (*accumulator)[i] = malloc(num_chans * num_subbands * NUM_STOKES * sizeof(float));
    
        if((*accumulator)[i] == NULL) {
            vegas_error("vegas_net_thread", "malloc failed");
            pthread_exit(NULL);
        }
    }
}


/* Frees up memory that was allocated for the accumulators */
void destroy_accumulators(float **accumulator)
{
    int i;

    for(i = 0; i < NUM_SW_STATES; i++)
        free(accumulator[i]);

    free(accumulator);
}



/* Resets the vector accumulators */
void reset_accumulators(float **accumulator, struct sdfits_data_columns* data_cols,
                        char* accum_dirty, int num_subbands, int num_chans)
{
    int i, j, k, l;
    int sum = 0;

    for(i = 0; i < NUM_SW_STATES; i++)
    {
        if(accum_dirty[i])
        {
            for(j = 0; j < num_chans; j++)
            {
                for(k = 0; k < num_subbands; k++)
                {
                    for(l = 0; l < NUM_STOKES; l++)
                        accumulator[i][j*num_subbands*NUM_STOKES + k*NUM_STOKES + l] = 0.0;
                }
            }

            data_cols[i].time = 0.0;
            data_cols[i].exposure = 0.0;
            data_cols[i].sttspec = sum;
            data_cols[i].stpspec = 0;
            data_cols[i].data = NULL;

            accum_dirty[i] = 0;
        }

    } 
}


/* The main CPU accumulator thread */
void vegas_accum_thread(void *_args) {

    float **accumulator;      //indexed accumulator[accum_id][chan][subband][stokes]
    char accum_dirty[NUM_SW_STATES];
    struct sdfits_data_columns data_cols[NUM_SW_STATES];
    int payload_type = 0;
    int i, j, k, rv;
    int use_scanlen;
    double fpgafreq;
    double scan_length_seconds;
    int accumid_xor_mask = 0;


    /* Get arguments */
    struct vegas_thread_args *args = (struct vegas_thread_args *)_args;

    /* Set cpu affinity */
    rv = sched_setaffinity(0, sizeof(cpu_set_t), &args->cpuset);
    if (rv<0) { 
        vegas_error("vegas_accum_thread", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }

    /* Set priority */
    rv=0;
    if (args->priority != 0)
    {
        struct sched_param priority_param;
        priority_param.sched_priority = args->priority;
        rv = pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority_param);
    }
    if (rv<0) {
        vegas_error("vegas_accum_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct vegas_status st;
    rv = vegas_status_attach(&st);
    if (rv!=VEGAS_OK) {
        vegas_error("vegas_accum_thread", 
                "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)vegas_status_detach, &st);
    pthread_cleanup_push((void *)set_exit_status, &st);
    pthread_cleanup_push((void *)vegas_thread_set_finished, args);

    /* Init status */
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEY, "init");
    vegas_status_unlock_safe(&st);

    /* Read in general parameters */
    struct vegas_params gp;
    struct sdfits sf;
    pthread_cleanup_push((void *)vegas_free_sdfits, &sf);

    /* Attach to databuf shared mem */
    struct vegas_databuf *db_in, *db_out;
    db_in = vegas_databuf_attach(args->input_buffer);
    char errmsg[256];
    if (db_in==NULL) {
        sprintf(errmsg,
                "Error attaching to input databuf(%d) shared memory.", 
                args->input_buffer);
        vegas_error("vegas_accum_thread", errmsg);
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)vegas_databuf_detach, db_in);
    db_out = vegas_databuf_attach(args->output_buffer);
    if (db_out==NULL) {
        sprintf(errmsg,
                "Error attaching to output databuf(%d) shared memory.", 
                args->output_buffer);
        vegas_error("vegas_accum_thread", errmsg);
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)vegas_databuf_detach, db_out);

    /* Determine high/low bandwidth mode */
    char bw_mode[16];
    if (hgets(st.buf, "BW_MODE", 16, bw_mode))
    {
        if(strncmp(bw_mode, "high", 4) == 0)
            payload_type = INT_PAYLOAD;
        else if(strncmp(bw_mode, "low", 3) == 0)
            payload_type = FLOAT_PAYLOAD;
        else
            vegas_error("vegas_net_thread", "Unsupported bandwidth mode");
    }
    else
        vegas_error("vegas_net_thread", "BW_MODE not set");

    /* Read nchan and nsubband from status shared memory */
    vegas_read_obs_params(st.buf, &gp, &sf);
    use_scanlen = read_scan_length(st.buf, &fpgafreq, &scan_length_seconds);
    /* read switching signal inversion mask, if present */
    if (hgeti4(st.buf, "_SWSGPLY", &accumid_xor_mask) == 0)
    {
        // switching signals used as is
        accumid_xor_mask = 0x0;
    }
    

    /* Allocate memory for vector accumulators */
    create_accumulators(&accumulator, sf.hdr.nchan, sf.hdr.nsubband);
    pthread_cleanup_push((void *)destroy_accumulators, accumulator);

    /* Clear the vector accumulators */
    for(i = 0; i < NUM_SW_STATES; i++) accum_dirty[i] = 1;
    reset_accumulators(accumulator, data_cols, accum_dirty, sf.hdr.nsubband, sf.hdr.nchan);

    /* Loop */
    int curblock_in=0, curblock_out=0;
    int first=1;
    float reqd_exposure=0;
    double accum_time=0;
    int integ_num = 0;
    float pfb_rate = 0.0;
    int heap, accumid, struct_offset, array_offset;
    char *hdr_in=NULL, *hdr_out=NULL;
    struct databuf_index *index_in, *index_out;
    uint64_t scan_length_time_counter;
    uint64_t raw_time_counter=0;
    struct BlockStats blkstats;

    // int nblock_int=0, npacket=0, n_pkt_drop=0, n_heap_drop=0;
    int debug_tick = 0;

    signal(SIGINT,cc);
    while (run) {

        /* Note waiting status */
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        vegas_status_unlock_safe(&st);

        /* Wait for buf to have data */
        rv = vegas_databuf_wait_filled(db_in, curblock_in);
        if (rv!=0) continue;

        /* Note waiting status and current block*/
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "accumulating");
        hputi4(st.buf, "ACCBLKIN", curblock_in);
        vegas_status_unlock_safe(&st);

        /* Read param struct for this block */
        hdr_in = vegas_databuf_header(db_in, curblock_in);
        if (first) 
            vegas_read_obs_params(hdr_in, &gp, &sf);
        else
            vegas_read_subint_params(hdr_in, &gp, &sf);

        /* Do any first time stuff: first time code runs, not first time process this block */
        if (first) {

            /* Set up first output header. This header is copied from block to block
               each time a new block is created */
            hdr_out = vegas_databuf_header(db_out, curblock_out);
            memcpy(hdr_out, vegas_databuf_header(db_in, curblock_in),
                    VEGAS_STATUS_SIZE);

            /* Read required exposure and PFB rate from status shared memory */
            reqd_exposure = sf.data_columns.exposure;
            pfb_rate = fabs(sf.hdr.efsampfr) / (2 * sf.hdr.nchan);

            /* Initialise the index in the output block */
            index_out = (struct databuf_index*)vegas_databuf_index(db_out, curblock_out);
            index_out->num_datasets = 0;
            index_out->array_size = sf.hdr.nsubband * sf.hdr.nchan * NUM_STOKES * 4;
            
            scan_length_time_counter = 0;
            first=0;
            check_scan_length(0,0,0); // resets function static local variables


            blkstats.nblock_int = 0;
            blkstats.npacket = 0;
            blkstats.n_pkt_drop = 0;
            blkstats.n_heap_drop = 0;
        }

        /* Loop through each spectrum (heap) in input buffer */
        index_in = (struct databuf_index*)vegas_databuf_index(db_in, curblock_in);

        for(heap = 0; heap < index_in->num_heaps; heap++)
        {
            /* If invalid, record it and move to next heap */
            if(!index_in->cpu_gpu_buf[heap].heap_valid)
            {
                blkstats.n_heap_drop++;
                continue;
            }

            /* Read in heap from buffer */
            char* heap_addr = (char*)(vegas_databuf_data(db_in, curblock_in) +
                                sizeof(struct freq_spead_heap) * heap);
            struct freq_spead_heap* freq_heap = (struct freq_spead_heap*)(heap_addr);

            char* payload_addr = (char*)(vegas_databuf_data(db_in, curblock_in) +
                                sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK +
                                (index_in->heap_size - sizeof(struct freq_spead_heap)) * heap );
            int32_t *i_payload = (int32_t*)(payload_addr);
            float *f_payload = (float*)(payload_addr);

            /* This optionally inverts the sense of switching signal inputs.
             * 0x8 - inverts blanking
             * 0x4 - not used?
             * 0x2 - inverts sig/ref 
             * 0x1 - inverts cal
             * The bits above 0x8 are cleared.
             */
            freq_heap->status_bits = (freq_heap->status_bits ^ accumid_xor_mask) & (CAL_SR_MASK|BLANKING_MASK);
            /* for accumid we only care for the lower bits */
            accumid = freq_heap->status_bits & CAL_SR_MASK;         

            /*Debug: print heap */
#if 0
            printf("FH: %d, %d, %d, %d, %d, %d %f %f %d\n", freq_heap->time_cntr, freq_heap->spectrum_cntr,
                freq_heap->integ_size, freq_heap->mode, freq_heap->status_bits,
                freq_heap->payload_data_off, accum_time, reqd_exposure, integ_num);
#endif
            // calculate the 40 bit raw time counter
            raw_time_counter = (((uint64_t)freq_heap->time_cntr_top8) << 32)
                                     + (uint64_t)freq_heap->time_cntr;
            
            /* If we have accumulated for long enough, write vectors to output block */
            if(accum_time >= reqd_exposure)
            {
#if 0            
                // DEBUG status of switching signals
                char swstatbuf[64];
                sprintf(swstatbuf, "%c %c %c %c",                       
                        freq_heap->status_bits & 0x2 ? 'R' : 'S',
                        freq_heap->status_bits & 0x1 ? 'N' : 'C',
                        freq_heap->status_bits & 0x8 ? 'B' : 'L',
                        debug_tick ? '+' : '=');
                debug_tick = !debug_tick;
                vegas_status_lock_safe(&st);
                hputs(st.buf, "SWSIGSTA", swstatbuf);
                vegas_status_unlock_safe(&st);
                // end debug
#endif                 
                /* Write block number to status buffer */
                vegas_status_lock_safe(&st);
                hputi4(st.buf, "ACCBLKOU", curblock_out);
                vegas_status_unlock_safe(&st);
                
                write_full_integration(db_out, &curblock_out, 
                                       db_in,   curblock_in, 
                                       accum_dirty, accumulator, data_cols, &sf, &blkstats);
                accum_time = 0;
                integ_num += 1;
                if (use_scanlen)
                {
                    if (check_scan_length(raw_time_counter, fpgafreq, scan_length_seconds))
                    {
                        // set the incomplete block as filled so that the next stage
                        // sees the partial block. num_datasets indicates the amount of 
                        // partial data.      
                        vegas_databuf_set_filled(db_out, curblock_out);
                        pthread_exit(0);
                    }
                }
                

                reset_accumulators(accumulator, data_cols, accum_dirty,
                                sf.hdr.nsubband, sf.hdr.nchan);
            }
            

            /* Only add spectrum to accumulator if blanking bit is low */
            if((freq_heap->status_bits & BLANKING_MASK) == 0)
            {
                /* Fill in data columns header fields */
                if(!accum_dirty[accumid])
                {
                    /*Record SPEAD header fields*/
                    data_cols[accumid].time = index_in->cpu_gpu_buf[heap].heap_rcvd_mjd;
                    data_cols[accumid].time_counter = (((uint64_t)freq_heap->time_cntr_top8) << 32)
                                                        + (uint64_t)freq_heap->time_cntr;
                    data_cols[accumid].integ_num = integ_num;
                    data_cols[accumid].sttspec = freq_heap->spectrum_cntr;
                    data_cols[accumid].accumid = accumid;

                    /* Fill in rest of fields from status buffer */
                    strcpy(data_cols[accumid].object, sf.data_columns.object);
                    data_cols[accumid].azimuth = sf.data_columns.azimuth;
                    data_cols[accumid].elevation = sf.data_columns.elevation;
                    data_cols[accumid].bmaj = sf.data_columns.bmaj;
                    data_cols[accumid].bmin = sf.data_columns.bmin;
                    data_cols[accumid].bpa = sf.data_columns.bpa;
                    data_cols[accumid].centre_freq_idx = sf.data_columns.centre_freq_idx;
                    data_cols[accumid].ra = sf.data_columns.ra;
                    data_cols[accumid].dec = sf.data_columns.dec;
                    data_cols[accumid].exposure = 0.0;

                    for(i = 0; i < NUM_SW_STATES; i++)
                        data_cols[accumid].centre_freq[i] = sf.data_columns.centre_freq[i];

                    accum_dirty[accumid] = 1;
                }

                data_cols[accumid].exposure += (float)(freq_heap->integ_size)/pfb_rate;
                data_cols[accumid].stpspec = freq_heap->spectrum_cntr;

                /* Add spectrum to appropriate vector accumulator (high-bw mode) */
                if(payload_type == INT_PAYLOAD)
                {
                    for(i = 0; i < sf.hdr.nchan; i++)
                    {
                        for(j = 0; j < sf.hdr.nsubband; j++)
                        {
                            for(k = 0; k < NUM_STOKES; k++)
                            {
                                accumulator[accumid]
                                           [i*sf.hdr.nsubband*NUM_STOKES + j*NUM_STOKES + k] +=
                                    (float)i_payload[i*sf.hdr.nsubband*NUM_STOKES + j*NUM_STOKES + k];
                            }
                        }
                    }
                }

                /* Add spectrum to appropriate vector accumulator (low-bw mode) */
                else
                {
                    for(i = 0; i < sf.hdr.nchan; i++)
                    {
                        for(j = 0; j < sf.hdr.nsubband; j++)
                        {
                            for(k = 0; k < NUM_STOKES; k++)
                            {
                                accumulator[accumid]
                                           [i*sf.hdr.nsubband*NUM_STOKES + j*NUM_STOKES + k] +=
                                    f_payload[i*sf.hdr.nsubband*NUM_STOKES + j*NUM_STOKES + k];
                            }
                        }
                    }
                }

            }
            
            accum_time += (double)freq_heap->integ_size / pfb_rate;
        }

        /* Update packet count and loss fields from input header */
        blkstats.nblock_int++;
        blkstats.npacket += gp.num_pkts_rcvd;
        blkstats.n_pkt_drop += gp.num_pkts_dropped;

        /* Done with current input block */
        vegas_databuf_set_free(db_in, curblock_in);
        curblock_in = (curblock_in + 1) % db_in->n_block;

        /* Check for cancel */
        pthread_testcancel();
    }

    pthread_exit(NULL);
    pthread_cleanup_pop(0); /* Closes set_exit_status */
    pthread_cleanup_pop(0); /* Closes set_finished */
    pthread_cleanup_pop(0); /* Closes vegas_free_sdfits */
    pthread_cleanup_pop(0); /* Closes ? */
    pthread_cleanup_pop(0); /* Closes destroy_accumulators */
    pthread_cleanup_pop(0); /* Closes vegas_status_detach */
    pthread_cleanup_pop(0); /* Closes vegas_databuf_detach */
}

int check_scan_length(int64_t time_counter, double fpgafreq, double scanlen)
{
    static int64_t last_time_counter = 0;
    static int64_t upper_bits = 0;
    double scan_length_clock;
    int64_t scanlen_time_counter;
    
    if (fpgafreq < 0.1)
    {
        // I hate to do this, but this is a signal to reset our static vars
        last_time_counter = 0;
        upper_bits = 0;
        return 0;
    }
    
    if (last_time_counter > time_counter)
    {
        /* 40 bit time counter has rolled over */
        upper_bits += 1LL<<40;   
    }
    last_time_counter = time_counter;
    scanlen_time_counter = upper_bits + time_counter;
    /* check if scan length has been reached */
    scan_length_clock = (double)(scanlen_time_counter) / fpgafreq;

    if (scan_length_clock > scanlen)
    {
        // printf("Scanlength completed %f, %f\n", scan_length_clock, scanlen);
        return(1);
    }
    return(0);
}

void write_full_integration(struct vegas_databuf *db_out, int *cur_block_out, 
                       struct vegas_databuf *db_in,  int  curblock_in, 
                       char *accum_dirty, float **accumulator, 
                       struct sdfits_data_columns* data_cols,
                       struct sdfits *sdf, struct BlockStats *blkstat)
{
    int i;
    struct databuf_index* index_out;
    int curblock_out = *cur_block_out;
    char *hdr_out = vegas_databuf_header(db_out, curblock_out);
    char *hdr_in  = vegas_databuf_header(db_in, curblock_in);
    int struct_offset, array_offset;
    
    int rv = 0;
    
    for(i = 0; i < NUM_SW_STATES; i++)
    {
        /*If a particular accumulator is dirty, write it to output buffer */
        if(accum_dirty[i])
        {
            /*If insufficient space, first mark block as filled and request new block*/
            index_out = (struct databuf_index*)(vegas_databuf_index(db_out, curblock_out));

            if( (index_out->num_datasets+1) *
                (index_out->array_size + sizeof(struct sdfits_data_columns)) > 
                db_out->block_size)
            {
                // printf("Accumulator finished with output block %d\n", curblock_out);

                /* Update packet count and loss fields in output header */
                hputi4(hdr_out, "NBLOCK",  blkstat->nblock_int);
                hputi4(hdr_out, "NPKT",    blkstat->npacket);
                hputi4(hdr_out, "NPKTDROP",blkstat->n_pkt_drop);
                hputi4(hdr_out, "NHPDROP", blkstat->n_heap_drop);

                /* Close out current integration */
                vegas_databuf_set_filled(db_out, curblock_out);
                /* Wait for next output buf */
                curblock_out = (curblock_out + 1) % db_out->n_block;
                vegas_databuf_wait_free(db_out, curblock_out);

                while ((rv=vegas_databuf_wait_free(db_out, curblock_out)) != VEGAS_OK)
                {
                    if (rv==VEGAS_TIMEOUT) {
                        vegas_warn("vegas_accum_thread", "timeout while waiting for output block");
                        continue;
                    } else {
                        vegas_error("vegas_accum_thread", "error waiting for free databuf");
                        run=0;
                        *cur_block_out = curblock_out;
                        pthread_exit(NULL);
                        break;
                    }
                }

                hdr_out = vegas_databuf_header(db_out, curblock_out);
                memcpy(hdr_out, vegas_databuf_header(db_in, curblock_in),
                        VEGAS_STATUS_SIZE);

                /* Initialise the index in new output block */
                index_out = (struct databuf_index*)vegas_databuf_index(db_out, curblock_out);
                index_out->num_datasets = 0;
                index_out->array_size = sdf->hdr.nsubband * sdf->hdr.nchan * NUM_STOKES * 4;
                
                blkstat->nblock_int=0;
                blkstat->npacket=0;
                blkstat->n_pkt_drop=0;
                blkstat->n_heap_drop=0;
            }            

            /*Update index for output buffer*/
            index_out = (struct databuf_index*)(vegas_databuf_index(db_out, curblock_out));

            if(index_out->num_datasets == 0)
                struct_offset = 0;
            else
                struct_offset = index_out->disk_buf[index_out->num_datasets-1].array_offset +
                                index_out->array_size;

            array_offset =  struct_offset + sizeof(struct sdfits_data_columns);
            index_out->disk_buf[index_out->num_datasets].struct_offset = struct_offset;
            index_out->disk_buf[index_out->num_datasets].array_offset = array_offset;
            
            /*Copy sdfits_data_columns struct to disk buffer */
            memcpy(vegas_databuf_data(db_out, curblock_out) + struct_offset,
                    &data_cols[i], sizeof(struct sdfits_data_columns));

            /*Copy data array to disk buffer */
            memcpy(vegas_databuf_data(db_out, curblock_out) + array_offset,
                    accumulator[i], index_out->array_size);
            
            /*Update SDFITS data_columns pointer to data array */
            ((struct sdfits_data_columns*)
            (vegas_databuf_data(db_out, curblock_out) + struct_offset))->data = 
            (unsigned char*)(vegas_databuf_data(db_out, curblock_out) + array_offset);

            index_out->num_datasets = index_out->num_datasets + 1;
        }
    
    }
    *cur_block_out = curblock_out;
}

