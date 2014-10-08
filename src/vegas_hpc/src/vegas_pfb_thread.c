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
#include "l8lbw1_fixups.h"
extern int g_use_L8_packets_for_L1_modes; // flag to enable L8 into L1 fix.

#define MULTIPLE_BLOCKS 8  // Compress this many blocks into 1 for L8 into L1 fix

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
    int num_blocks_needed = 1;
    
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
        if (strcmp(mdname, "l8/lbw1") == 0 && g_use_L8_packets_for_L1_modes)
        {
            packet_compression = 1;
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
    
    if (packet_compression)
    {
        num_blocks_needed = MULTIPLE_BLOCKS;
    }

    while (run) {

        /* Note waiting status */
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "waiting");
        vegas_status_unlock_safe(&st);

        int full_blocks[MULTIPLE_BLOCKS], free_blk, nextblk = curblock_in;
        /* Wait for buf to have data */
        for (free_blk=0; free_blk < num_blocks_needed; )
        {
            rv = vegas_databuf_wait_filled(db_in, nextblk);
            if (!run)  break;            
            if (rv!=0) continue;
            full_blocks[free_blk] = nextblk;
            nextblk = (nextblk + 1) % db_in->n_block;
            ++free_blk;
        }

        /* Note waiting status, current input block */
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEY, "processing");
        hputi4(st.buf, "PFBBLKIN", curblock_in);
        vegas_status_unlock_safe(&st);

        hdr_in = vegas_databuf_header(db_in, curblock_in);

        // If merging/compressing packets, make an l8lbw1 block from
        // 1 or 4 l8lbw8 input blocks
        if (packet_compression)
        {            
            fixup_l8lbw1_block_merge(db_in, num_blocks_needed, full_blocks);
        }
        
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
        do_pfb(db_in, full_blocks[0], db_out, &curblock_out, first, st, acc_len);

        /* Mark input block as free */
        for (free_blk=0; free_blk < num_blocks_needed; ++free_blk)
        {
            vegas_databuf_set_free(db_in, full_blocks[free_blk]);
        }
        /* Go to next input block */
        curblock_in = (curblock_in + num_blocks_needed) % db_in->n_block;

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

