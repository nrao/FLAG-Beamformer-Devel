/* vegas_pfb_thread.c
 *
 * Performs PFB on incoming time samples
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

#include "fitshead.h"
#include "sdfits.h"
#include "vegas_error.h"
#include "vegas_status.h"
#include "vegas_databuf.h"
#include "vegas_params.h"
#include "pfb_gpu.h"
#include "spead_heap.h"

#define STATUS_KEY "GPUSTAT"
#include "vegas_threads.h"

struct cmplx_sample
{
    int8_t re;
    int8_t im;
};

struct time_sample
{
    struct cmplx_sample pol[2];
};

// represent a set of 8 subbands with 2 polarizations with complex values
struct l8_time_sample
{
    struct time_sample subband[8];
};

// represent the contents of a l8/lbw8 packet
// e.g p.data[time_sample].data[n].subband[0];
struct time_spead_heap_packet_l8
{
    struct l8_time_sample data[256];
};

// representation of a l8/lbw1 packet
struct time_spead_heap_packet_l1
{
    struct time_sample data[2048];
};


/* Parse info from buffer into param struct */
extern void vegas_read_subint_params(char *buf, 
                                     struct vegas_params *g,
                                     struct sdfits *p);
extern void vegas_read_obs_params(char *buf, 
                                     struct vegas_params *g,
                                     struct sdfits *p);

void vegas_pfb_thread(void *_args) {

    /* Get args */
    struct vegas_thread_args *args = (struct vegas_thread_args *)_args;
    int rv;

    /* Set cpu affinity */
    rv = sched_setaffinity(0, sizeof(cpu_set_t), &args->cpuset);
    if (rv<0) { 
        vegas_error("vegas_pfb_thread", "Error setting cpu affinity.");
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
        vegas_error("vegas_pfb_thread", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct vegas_status st;
    rv = vegas_status_attach(&st);
    if (rv!=VEGAS_OK) {
        vegas_error("vegas_pfb_thread", 
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

    /* Init structs */
    struct vegas_params gp;
    struct sdfits sf;
    pthread_cleanup_push((void *)vegas_free_sdfits, &sf);

    /* Attach to databuf shared mem */
    struct vegas_databuf *db_in, *db_out;
    db_in = vegas_databuf_attach(args->input_buffer);
    if (db_in==NULL) {
        char msg[256];
        sprintf(msg, "Error attaching to databuf(%d) shared memory.",
                args->input_buffer);
        vegas_error("vegas_pfb_thread", msg);
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)vegas_databuf_detach, db_in);
    db_out = vegas_databuf_attach(args->output_buffer);
    if (db_out==NULL) {
        char msg[256];
        sprintf(msg, "Error attaching to databuf(%d) shared memory.",
                args->output_buffer);
        vegas_error("vegas_pfb_thread", msg);
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void *)vegas_databuf_detach, db_out);

    /* Loop */
    char *hdr_in = NULL;
    int curblock_in=0;
    int curblock_out = 0;
    int first=1;
    int acc_len = 0;
    int nchan = 0;
    int nsubband = 0;
    struct databuf_index *index_out;
    int packet_compression = 0;
    char mdname[80];
    char *tempbuf = 0;
    
    signal(SIGINT,cc);
    
    index_out = (struct databuf_index*)vegas_databuf_index(db_out, curblock_out);
    index_out->num_heaps = 0;

    vegas_status_lock_safe(&st);
    if (hgeti4(st.buf, "NCHAN", &nchan)==0) 
    {
        fprintf(stderr, "ERROR: %s not in status shm!\n", "NCHAN");
    }
    if (hgeti4(st.buf, "NSUBBAND", &nsubband)==0) 
    {
        fprintf(stderr, "ERROR: %s not in status shm!\n", "NSUBBAND");
    }
    if (hgeti4(st.buf, "ACC_LEN", &acc_len)==0) 
    {
        fprintf(stderr, "WARNING: %s not in status shm! Using computed value\n", "ACC_LEN");
    }
    
    if (hgets(st.buf, "MODENAME", sizeof(mdname), mdname)) 
    {
        if (!strcmp(mdname, "l8/lbw1"))
        {
            packet_compression = 1;
            tempbuf = (char *)malloc(32*1024*1024); // 32mb == block size
        }
    }
        
    vegas_status_unlock_safe(&st);
    if (EXIT_SUCCESS != reset_state(db_in->block_size,
                                    db_out->block_size,
                                    nsubband,
                                    nchan))
    {
        (void) fprintf(stderr, "ERROR: GPU initialisation failed!\n");
        run = 0;
    }

    while (run) {

        /* Note waiting status */
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        vegas_status_unlock_safe(&st);

        /* Wait for buf to have data */
        rv = vegas_databuf_wait_filled(db_in, curblock_in);
        if (rv!=0) continue;

        /* Note waiting status, current input block */
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "processing");
        hputi4(st.buf, "PFBBLKIN", curblock_in);
        vegas_status_unlock_safe(&st);

        hdr_in = vegas_databuf_header(db_in, curblock_in);
        struct databuf_index *index_in;
        index_in = (struct databuf_index*)vegas_databuf_index(db_in, curblock_in);
#if 1
        if (packet_compression)
        {
            struct time_spead_heap *l8_hdr;
            struct time_spead_heap *l1_hdr;
            struct time_spead_heap_packet_l8 *l8;
            struct time_spead_heap_packet_l1 *l1;    
            int i, s, out_heap, out_sample, heap;
            
            l8_hdr = (struct time_spead_heap *)vegas_databuf_data(db_in, curblock_in);
            l1_hdr = (struct time_spead_heap *)tempbuf;
            
            l8 = (struct time_spead_heap_packet_l8 *)&l8_hdr[MAX_HEAPS_PER_BLK];
            l1 = (struct time_spead_heap_packet_l1 *)&l1_hdr[MAX_HEAPS_PER_BLK];
            
            out_heap = 0;
            out_sample = 0;
            
            // for each heap     
            for (heap=0; heap<index_in->num_heaps; ++heap)
            {   
                // for each subband zero entry in the l8 packet           
                for (s=0; s<256; ++s)
                {
                    l1[out_heap].data[out_sample++] = l8[heap].data[s].subband[0];
                }

                if (out_sample >= 2048)
                {
                    l1_hdr[out_heap] = l8_hdr[heap];
                    out_heap++;
                    out_sample = 0;
                }
            }
            // at this point we have copied all the subband 0 samples into the temporary buffer
            // This means we have 8 times less data
            // copy the compressed data back into the data buffer
            memcpy(l8_hdr, l1_hdr, sizeof(struct time_spead_heap) * MAX_HEAPS_PER_BLK + out_heap * sizeof(struct time_spead_heap_packet_l1));
            // and update the index to indicate the smaller data
            for (i=index_in->num_heaps/8; i<index_in->num_heaps; ++i)
            {
                index_in->cpu_gpu_buf[i].heap_valid = 0;
            }
            index_in->num_heaps = index_in->num_heaps/8;
        }
#endif        
        /* Get params */
        if (first)
        {
            vegas_read_obs_params(hdr_in, &gp, &sf);
            /* Check the value of ACC_LEN. If not in shmem, use a computed value. */
            if (acc_len == 0)
            {
                acc_len = (int)round(fabs(sf.hdr.chan_bw) * sf.hdr.hwexposr);
            }
        }
        vegas_read_subint_params(hdr_in, &gp, &sf);

        /* Call PFB function */
        do_pfb(db_in, curblock_in, db_out, &curblock_out, first, st, acc_len);

        /* Mark input block as free */
        vegas_databuf_set_free(db_in, curblock_in);
        /* Go to next input block */
        curblock_in = (curblock_in + 1) % db_in->n_block;

        /* Check for cancel */
        pthread_testcancel();

        if (first) {
            first=0;
        }
    }
    run=0;

    //cudaThreadExit();
    pthread_exit(NULL);

    pthread_cleanup_pop(0); /* Closes vegas_databuf_detach(out) */
    pthread_cleanup_pop(0); /* Closes vegas_databuf_detach(in) */
    pthread_cleanup_pop(0); /* Closes vegas_free_sdfits */
    pthread_cleanup_pop(0); /* Closes vegas_thread_set_finished */
    pthread_cleanup_pop(0); /* Closes set_exit_status */
    pthread_cleanup_pop(0); /* Closes vegas_status_detach */

}

