//# Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//#
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning GBT software should be addressed as follows:
//# GBT Operations
//# National Radio Astronomy Observatory
//# P. O. Box 2
//# Green Bank, WV 24944-0002 USA

#define ELAPSED_NS(start,stop) \
(((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>

extern "C"
{
#include "vegas_error.h"
#include "vegas_status.h"
#include "bf_databuf.h"
#include "spead_heap.h"
#include "fitshead.h"
#define STATUS_KEYW "DISKSTAT"
#include "vegas_threads.h"
#include "fifo.h"
};
#include "DiskBufferChunk.h"
#include "BfFitsIO.h"
#include "BfFitsThread.h"

// static int verbose = false;
#define dbprintf if(verbose) printf


// Blasted vegas_status_lock_safe macro in has an incorrect type cast
// for the pthread_cleanup_push function argument. Must redo it here.
// The unlock macro was fine.
#undef vegas_status_lock_safe

#define vegas_status_lock_safe(s) \
    pthread_cleanup_push((void (*)(void*))vegas_status_unlock, s); \
    vegas_status_lock(s)


// variable for while loop exit
int scan_finished = 0;
void stop_thread(int sig)
{
    scan_finished = 1;
}
const int MAX_CMD_LEN = 64;

//called by main.cc to enter primary method 
extern "C"
void *runGbtFitsWriter(void *ptr)
{
    struct vegas_thread_args *args = (struct vegas_thread_args *)ptr;
    return BfFitsThread::run(args);
}

//primary function
void *
BfFitsThread::run(struct vegas_thread_args *args)
{
    bool cov_mode1 = (bool)args->cov_mode1;
    bool cov_mode2 = (bool)args->cov_mode2;
    bool cov_mode3 = (bool)args->cov_mode3;
    int rv;
    //create BfFitsIO pointer (the "fits writer")
    BfFitsIO *fitsio;

    timespec loop_start, loop_stop;
    timespec fits_start, fits_stop;


    // pass on the instance id from the args to our class member
    int instance_id = args->input_buffer;

    printf("BfFitsThread::run, instance_id = %d\n", instance_id);

    pthread_cleanup_push((void (*)(void*))&BfFitsThread::set_finished, args);

    /* Set cpu affinity */
    ///cpu_set_t cpuset, cpuset_orig;
    //sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    //CPU_ZERO(&cpuset);
    //CPU_SET(6, &cpuset);
    //rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    rv=0;
    if (rv<0)
    {
        vegas_error("BfFitsThread::run", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }

    /* Set priority */
    if (rv<0)
    {
        vegas_error("BfFitsThread::run", "Error setting priority level.");
        perror("set_priority");
    }

    /* Attach to status shared mem area */
    struct vegas_status st;
    rv = vegas_status_attach_inst(&st, instance_id);
    if (rv!=VEGAS_OK)
    {
        vegas_error("BfFitsThread::run",
                    "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void (*)(void*))&BfFitsThread::status_detach, &st);
    pthread_cleanup_push((void (*)(void*))&BfFitsThread::setExitStatus, &st);

    int databufid = 3; // disk buffer

    // Attach to the data buffer shared memory.
    // Different modes are taken into account due to the different buffer sizes
    void *gdb;
    int shmid, semid;
    if (cov_mode1) {
	databufid = 4; // this is for FINE CHANNEL CORRELATOR ONLY
        gdb = (void *)bf_databuf_attach(databufid, instance_id);
        if (gdb != 0)
            semid = ((bf_databuf *)gdb)->header.semid;
    } 

    else if (cov_mode2){
        gdb = (void *)bfpaf_databuf_attach(databufid, instance_id);
        if (gdb != 0)
            semid = ((bfpaf_databuf *)gdb)->header.semid;
    }

    else if (cov_mode3){
        gdb = (void *)bffrb_databuf_attach(databufid, instance_id);
        if (gdb != 0)
            semid = ((bffrb_databuf *)gdb)->header.semid;
    }

    else {
        gdb = (void *)bfp_databuf_attach(databufid, instance_id);
        if (gdb != 0)
            semid = ((bfp_databuf *)gdb)->header.semid;
    }


    // If we couldn't attach, exit
    if(gdb == 0)
    {
        vegas_error("BfFitsThread::run", "databuffer attach error cannot continue");
        pthread_exit(NULL);
    }


    // make sure we detach from this buffer when the thread exits
    pthread_cleanup_push((void (*)(void*))&BfFitsThread::databuf_detach, gdb);

    /* Set the thread status to init */
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEYW, "Init");
    vegas_status_unlock_safe(&st);


    /* Initialize some key parameters */
    struct sdfits sf;
    double start_time = 0;
    sf.data_columns.data = NULL;
    sf.filenum = 0;
    sf.new_file = 1;

    pthread_cleanup_push((void (*)(void*))&BfFitsThread::free_sdfits, &sf); //is this needed?

    // Query status memory for keywords & settings
    // Make a local copy of the status area
    // If keyword does not exist, attempt to fill-in a default value.
    char status_buf[VEGAS_STATUS_SIZE];
    char datadir[64] = {0};
    vegas_status_lock_safe(&st);
    memcpy(status_buf, st.buf, VEGAS_STATUS_SIZE);
    vegas_status_unlock_safe(&st);

    // Look for the DATADIR keyword, this forms the first portion of the path
    if (!hgets(status_buf, "DATADIR", sizeof(datadir), datadir))
    {
        vegas_error("Vegas FITS writer", "DATADIR status memory keyword not set");
        pthread_exit(0);
    }
    // Create a BfFitsIO writer subclass based on mode
    if (cov_mode1){
        fitsio = new BfFitsIO(datadir, false, instance_id,0);
    } 
    else if (cov_mode2){
        fitsio = new BfFitsIO(datadir, false, instance_id,1);
    }
    else if (cov_mode3){
        fitsio = new BfFitsIO(datadir, false, instance_id,2);
    }
    else {  
    	fitsio = new BfFitsIO(datadir, false, instance_id,3);
    }
    pthread_cleanup_push((void (*)(void*))&BfFitsThread::close, fitsio);

    // pass a copy of the status memory to the writer
    fitsio->copyStatusMemory(status_buf);

    // print status buffer to terminal
    printf("status_buf: %s\n", status_buf);

    // I'm assuming STRTDMJD is the starttime in DMJD
    if (hgetr8(status_buf, "STRTDMJD", &start_time) == 0)
    {
        // use the current time
        timeval tv;
        gettimeofday(&tv, 0);
        printf("gettimeofday: %lu\n", tv.tv_sec);
        start_time = BfFitsIO::timeval_2_mjd(&tv);
        printf("is DMJD: %f\n", start_time);

        unsigned long secs = BfFitsIO::dmjd_2_secs(start_time);
        printf("goes back to secs: %lu\n", secs);
    }
    hgetr8(status_buf, "STRTDMJD", &start_time);
    unsigned long secs = BfFitsIO::dmjd_2_secs(start_time);
    fitsio->set_startTime(start_time);

    // Starttime & DATADIR must be set as it is used to determine the file name
    // for data FITS files. The path becomes something like this:
    // sprintf(fullpath, "%s/%s/VEGAS/%s%c.fits", DATADIR, PROJID, start_time, BANK)
    // example:
    // DATADIR=/lustre
    // PROJID=project_1
    // start_time=2013_10_01_12:00:00
    // BANK=A
    // would result in a file: /lustre/project_1/VEGAS/2013_10_01_12:00:00A.fits
    //open FITS file
    fitsio->open();
    printf("fitsio opened\n");
    if(fitsio->getStatus() != 0)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "line %d, getStatus() is %d\n", __LINE__, fitsio->getStatus());
        vegas_error("BfFitsThread", buf);
        pthread_exit(0);
    }

    int block = 0,num_iter=0;
    char scan_status[96];
    int rx_some_data = 0;
    scan_finished = 0;

    int rowsWritten = 0;
    uint64_t total_loop_time = 0;
    uint64_t total_write_time = 0;

    signal(SIGINT, stop_thread);
    signal(SIGHUP, stop_thread);
    signal(SIGTERM, stop_thread);
    signal(SIGKILL, stop_thread);

    vegas_status_lock_safe(&st);
    hputi4(st.buf, "DSKBLKIN", block);
    vegas_status_unlock_safe(&st);
    int scanLen;
     
    hgeti4(status_buf,"SCANLEN",&scanLen);
    //ensure we have the correct scan length TODO: remove
    printf("SCANLEN: %d\n",scanLen);
    //enter loop until scan is finished. 
    while(!scan_finished && ::run)
    {
        
        clock_gettime(CLOCK_MONOTONIC, &loop_start);
        // Wait for a data buffer from the HPC program
        if (databuf_wait_filled(semid, block))
        {
            //printf("Timed out\n");
            // Waiting timed out - check the scan status
            vegas_status_lock_safe(&st);
            hgets(st.buf, "SCANSTAT", sizeof(scan_status), scan_status);
            vegas_status_unlock_safe(&st);
            /*change process status to waiting*/            
            vegas_status_lock_safe(&st);
            hputs(st.buf, STATUS_KEYW, "Waiting");
            vegas_status_unlock_safe(&st);
            continue;
        }
        rx_some_data = 1;
        /*change process status to waiting*/
        vegas_status_lock_safe(&st);
        hputs(st.buf, STATUS_KEYW, "Writing");
        vegas_status_unlock_safe(&st);

        
        // Start the timer for how long it takes to write to FITS
        // We are now starting this timer before the data is copied
        //   from the gpu table to the fits table
        clock_gettime(CLOCK_MONOTONIC, &fits_start);

        // collect some mode dependent info about the databuffer blocks
        uint64_t mcnt,n_block;
        int64_t gd;
        float *data;
        if (cov_mode1) {
            mcnt = ((bf_databuf *)gdb)->block[block].header.mcnt;
            n_block = ((bf_databuf *)gdb)->header.n_block;
            gd = ((bf_databuf *)gdb)->block[block].header.good_data;
            data = ((bf_databuf *)gdb)->block[block].data;
            fitsio->write_HI(mcnt, gd, data);
	    printf("mcnt: %llu\n",(long long unsigned int) mcnt);
        }

        else if (cov_mode2) {
            mcnt = ((bfpaf_databuf *)gdb)->block[block].header.mcnt;
            n_block = ((bfpaf_databuf *)gdb)->header.n_block;
            gd = ((bf_databuf *)gdb)->block[block].header.good_data;
            data = ((bfpaf_databuf *)gdb)->block[block].data;
            fitsio->write_PAF(mcnt, gd, data);
	    printf("mcnt: %llu\n",(long long unsigned int) mcnt);
        }

        else if (cov_mode3){
            mcnt = ((bffrb_databuf *)gdb)->block[block].header.mcnt;
            n_block = ((bffrb_databuf *)gdb)->header.n_block;
            gd = ((bf_databuf *)gdb)->block[block].header.good_data;
            data = ((bffrb_databuf *)gdb)->block[block].data;
            fitsio->write_FRB(mcnt, gd, data);
            printf("mcnt: %llu\n",(long long unsigned int) mcnt);
        }
        else {
            mcnt = ((bfp_databuf *)gdb)->block[block].header.mcnt;
            n_block = ((bfp_databuf *)gdb)->header.n_block;
            gd = ((bf_databuf *)gdb)->block[block].header.good_data;
            data = ((bfp_databuf *)gdb)->block[block].data;
            printf("mcnt: %llu\n",(long long unsigned int) mcnt);
            num_iter++;
            fitsio->write_RTBF(mcnt, gd, data);
        }    
        clock_gettime(CLOCK_MONOTONIC, &fits_stop);
        total_write_time += ELAPSED_NS(fits_start, fits_stop);
        
        rowsWritten++;

        // Free the datablock for the HPC program
        if(databuf_set_free(semid, block))
        {
            vegas_warn("BfFitsThread::run", "failed to set block free");
            printf("block=%d\n", block);
        }

        block = (block + 1) % n_block;

        
        // Scan completed (We have more than SCANLEN of data)
        if (fitsio->is_scan_complete(mcnt) || scan_finished ==1)
        {
            printf("Ending fits writer because scan is complete\n");
            scan_finished = 1;
            databuf_set_free(semid, block);
        }

        // Check for a thread cancellation
        pthread_testcancel();

        clock_gettime(CLOCK_MONOTONIC, &loop_stop);
        total_loop_time += ELAPSED_NS(loop_start, loop_stop);
    }
    printf("BfFitsThread::run exiting with scan_finished=%d run=%d\n", scan_finished, ::run);
    printf("\tWe wrote %d lines\n", rowsWritten);
    printf("\tIt took an average of %.2f µs to complete each loop\n", total_loop_time / (double)rowsWritten / 1000);
    printf("\tIt took an average of %.2f µs to write each row to FITS\n", total_write_time / (double)rowsWritten / 1000);

    fitsio->close();

    // Set our process status to exiting
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEYW, "Exiting");
    vegas_status_unlock_safe(&st);

    // cleanup on exit
    // TBF: is this the correct number of pops?
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    return 0;
}

void
BfFitsThread::set_finished(struct vegas_thread_args *args)
{
vegas_thread_set_finished(args);
}


void
BfFitsThread::status_detach(vegas_status *st)
{
    vegas_status_detach(st);
}

void
BfFitsThread::setExitStatus(vegas_status *st)
{
    vegas_status_lock(st);
    hputs(st->buf, STATUS_KEYW, "exiting");
    vegas_status_unlock(st);
}

void
BfFitsThread::databuf_detach(void *db)
{
    databuf_detach(db);
}

void
BfFitsThread::databuf_detach(bf_databuf *db)
{
    bf_databuf_detach(db);
}

void
BfFitsThread::free_sdfits(vegas_status *st)
{
    // no-op
}

void
BfFitsThread::close(BfFitsIO *f)
{
    f->close();
}


