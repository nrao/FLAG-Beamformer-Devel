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
//#	GBT Operations
//#	National Radio Astronomy Observatory
//#	P. O. Box 2
//#	Green Bank, WV 24944-0002 USA

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/resource.h>

extern "C" 
{
#include "vegas_error.h"
#include "vegas_status.h"
#include "vegas_databuf.h"
#include "spead_heap.h"
#include "fitshead.h"
#define STATUS_KEYW "DISKSTAT"
#include "vegas_threads.h"
};

#include "DiskBufferChunk.h"
#include "VegasFitsIO.h"
#include "VegasFitsThread.h"

static int verbose = false;
#define dbprintf if(verbose) printf


// Blasted vegas_status_lock_safe macro in has an incorrect type cast
// for the pthread_cleanup_push function argument. Must redo it here.
// The unlock macro was fine.
#undef vegas_status_lock_safe

#define vegas_status_lock_safe(s) \
    pthread_cleanup_push((void (*)(void*))vegas_status_unlock, s); \
    vegas_status_lock(s)
    
#define MJD_1970_EPOCH (40587)
double timeval_2_mjd(timeval *tv)
{
    double dmjd = tv->tv_sec/86400 + MJD_1970_EPOCH;
    dmjd += (tv->tv_sec % 86400)/86400.0;
    return dmjd;
}

int scan_finished = 0;
static void stop_thread(int sig)
{
    scan_finished = 1;
}

extern "C"
void *runGbtFitsWriter(void *ptr)
{
    struct vegas_thread_args *args = (struct vegas_thread_args *)ptr;
    return VegasFitsThread::run(args);
}

// data_buffer timeouts are .250 sec, so wait up to 2.5 seconds
#define MAX_ACCUM_TIMEOUTS 100

void *
VegasFitsThread::run(struct vegas_thread_args *args)
{
    int rv;
    VegasFitsIO *fitsio;
    
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::set_finished, args);

    /* Set cpu affinity */
    //cpu_set_t cpuset, cpuset_orig;
    //sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    //CPU_ZERO(&cpuset);
    //CPU_SET(6, &cpuset);
    //rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    rv=0;
    if (rv<0) 
    { 
        vegas_error("VegasFitsThread::run", "Error setting cpu affinity.");
        perror("sched_setaffinity");
    }

    /* Set priority */
    // rv = setpriority(PRIO_PROCESS, 0, args->priority);
    if (rv<0) 
    {
        vegas_error("VegasFitsThread::run", "Error setting priority level.");
        perror("set_priority");
    }
    
    /* Attach to status shared mem area */
    struct vegas_status st;
    rv = vegas_status_attach(&st);
    if (rv!=VEGAS_OK) 
    {
        vegas_error("VegasFitsThread::run", 
                    "Error attaching to status shared memory.");
        pthread_exit(NULL);
    }
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::status_detach, &st);
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::setExitStatus, &st);    
    
    const int databufid = 1; // disk buffer
    // Attach to the data buffer shared memory
    vegas_databuf *gdb = vegas_databuf_attach(databufid);
    if(gdb == 0)
    {
        vegas_error("VegasFitsThread::run", "databuffer attach error cannot continue");
        pthread_exit(NULL);
    }    
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::databuf_detach, gdb);    
    
    /* Set the thread  status to init */
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEYW, "init");
    vegas_status_unlock_safe(&st);

    
    /* Initialize some key parameters */
    struct sdfits sf;
    double start_time = 0;
    sf.data_columns.data = NULL;
    sf.filenum = 0;
    sf.new_file = 1; 
    
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::free_sdfits, &sf);
    
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
    // Create a VegasFitsIO writer
    fitsio = new VegasFitsIO(datadir, false);
    pthread_cleanup_push((void (*)(void*))&VegasFitsThread::close, fitsio);
       
    // pass a copy of the status memory to the writer
    fitsio->copyStatusMemory(status_buf);

    printf("status_buf: %s\n", status_buf);
      
    int nsubband;         
    if (hgeti4(status_buf, "NSUBBAND", &nsubband))
    {
        fitsio->setNumberSubBands(nsubband);
    }

    fitsio->setNumberStokes(1);
    
    // I'm assuming STRTDMJD is the starttime in DMJD
    if (hgetr8(status_buf, "STRTDMJD", &start_time) == 0)
    {
        // use the current time
        timeval tv;
        gettimeofday(&tv, 0);
        start_time = timeval_2_mjd(&tv);        
    }    
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
    printf("fitsio opening\n");
    fitsio->open();
    printf("fitsio opened\n");
    if(fitsio->getStatus() != 0)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "line %d, getStatus() is %d\n", __LINE__, fitsio->getStatus());
        vegas_error("VegasFitsThread", buf);
        pthread_exit(0);
    }

    int block = 0;
    int last_int = -2;
    bool data_waiting = false;
    bool new_integration = true;
    int num_accum_timeouts = 0;
    char scan_status[96];
    int rx_some_data = 0;
    scan_finished = 0;
    
    signal(SIGINT, stop_thread);

    vegas_status_lock_safe(&st);
    hputi4(st.buf, "DSKBLKIN", block);
    vegas_status_unlock_safe(&st);

    /* change process status to running*/
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEYW, "running");
    vegas_status_unlock_safe(&st);
           
    while(!scan_finished && ::run)
    {
        // Wait for a data buffer from the HPC program
        if(vegas_databuf_wait_filled(gdb, block))
        {
            printf("Timed out\n");
            // Waiting timed out - check the scan status
            // dbprintf("db not filled\n");
            vegas_status_lock_safe(&st);
            hgets(st.buf, "SCANSTAT", sizeof(scan_status), scan_status);
            vegas_status_unlock_safe(&st);
            // Is the scan still running?
            if (strcmp(scan_status, "running")!=0 && rx_some_data)
            {
                // scan is not running. Wait a few more times to make sure the
                // data memory is fully drained.
                if (++num_accum_timeouts > MAX_ACCUM_TIMEOUTS)
                {
                    printf("Fits Writer detected end of scan\n");
                    scan_finished = 1;
                }
            }
            else
            {
                num_accum_timeouts = 0;
            }           
            continue;
        }
        rx_some_data = 1;
        printf("Got a buffer block=%d, gdb=%p\n", block, gdb);
        //dbprintf("Got a buffer block=%d, gdb=%p\n", block, gdb);


        /*
        char *fits_header = vegas_databuf_header(gdb, block);
        
        databuf_index *index = reinterpret_cast<databuf_index*>(
            vegas_databuf_index(gdb, block));
        dbprintf("datasets=%d\n", index->num_datasets);
        // Now iterate through the block processing each dataset
        for(size_t dataset = 0; dataset < index->num_datasets; ++dataset)
        {
            // Get a chunk
            sdfits_data_columns *data_header = reinterpret_cast<sdfits_data_columns*>(
                vegas_databuf_data(gdb, block) +
                index->disk_buf[dataset].struct_offset);
            float *data = reinterpret_cast<float*>
                (vegas_databuf_data(gdb, block) + index->disk_buf[dataset].array_offset);
            // Create a DiskBufferChunk to process the dataset (organizes and transposes dataset)
            DiskBufferChunk *chunk = new DiskBufferChunk(fits_header, data_header, data);

            // Handle chunk
            int integration = chunk->getIntegrationNumber();
            if (integration < 0)
            {
                fitsio->set_scan_complete();
            }

            // Is the dataset is part of a new integration?
            if(integration != last_int)
            {
                // Does the fits writer have data from a prior integration?
                if(data_waiting)
                {
                    // If so, write the entire integration to the file
                    if (fitsio->write() != 0)
                    {
                        perror("error writing to FITS file");
                        delete chunk;
                        chunk = 0;
                        break;
                    }
                    dbprintf("fitsio->write\n");
                    data_waiting = false;
                }
                if (fitsio->is_scan_complete())
                {
                    // The scan has ended so cleanup chunk and prepare to exit
                    delete chunk;
                    chunk=0;
                    break;
                }                
                new_integration = true;
                last_int = integration;
            }
            // queue this dataset to be written later
            fitsio->bufferedWrite(chunk, new_integration);
            dbprintf("fitsio->bufferedWrite\n");
            new_integration = false;
            data_waiting = true;

            // Note current block 
            vegas_status_lock_safe(&st);
            hputi4(st.buf, "DSKBLKIN", block);
            vegas_status_unlock_safe(&st);
            
            delete chunk;
        }

        */

        // Free the datablock for the HPC program
        if(vegas_databuf_set_free(gdb, block))
        {
            vegas_warn("VegasFitsThread::run", "failed to set block free");
            printf("block=%d\n", block);
        }

        block = (block + 1) % gdb->header.n_block;
        
        // Scan completed (We have more than SCANLEN of data)
        /*
        if (fitsio->is_scan_complete())
        {
            printf("Ending fits writer because scan is complete\n");
            scan_finished = 1;
        }
        */
        // Check for a thread cancellation
        pthread_testcancel();
        
    }
    printf("VegasFitsThread::run exiting with scan_finished=%d run=%d\n", scan_finished, ::run); 
    
    fitsio->close();
    
    // Set our process status to exiting
    vegas_status_lock_safe(&st);
    hputs(st.buf, STATUS_KEYW, "exiting");
    vegas_status_unlock_safe(&st);
    
    vegas_databuf_detach(gdb);
    pthread_cleanup_pop(0); 
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);   
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);   
    pthread_cleanup_pop(0);    
    return 0;
}

void
VegasFitsThread::set_finished(struct vegas_thread_args *args)
{
    // vegas_thread_set_finished(args);
}

void 
VegasFitsThread::status_detach(vegas_status *st)
{
    vegas_status_detach(st);
}

void
VegasFitsThread::setExitStatus(vegas_status *st)
{
    vegas_status_lock(st);
    hputs(st->buf, STATUS_KEYW, "exiting");
    vegas_status_unlock(st);
}

void 
VegasFitsThread::databuf_detach(vegas_databuf *db)
{
    vegas_databuf_detach(db);
}
 
void 
VegasFitsThread::free_sdfits(vegas_status *st)
{
    // no-op
}

void 
VegasFitsThread::close(VegasFitsIO *f)
{
    f->close();
}


