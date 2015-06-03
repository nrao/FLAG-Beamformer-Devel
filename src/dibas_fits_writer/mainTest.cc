#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

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

#include "mainTest.h"
#include "VegasFitsIO.h"

#define THIS_MJD_1970_EPOCH (40587)
double this_timeval_2_mjd(timeval *tv)
{
    double dmjd = tv->tv_sec/86400 + THIS_MJD_1970_EPOCH;
    dmjd += (tv->tv_sec % 86400)/86400.0;
    return dmjd;
}

int mainTest(int argc, char **argv)
{

    timeval start, end;
    gettimeofday(&start, 0);
    
    size_t status_size = 184320; // (2304*80)
    double start_time = 0;

    VegasFitsIO *fitsio;
    // Create a VegasFitsIO writer
    fitsio = new VegasFitsIO("/tmp", false);

    printf("mainTest.c\n");

    // sim. status memory buffer contents
    void *status = malloc(sizeof(char)*status_size);
    memset(status, ' ', sizeof(char)*status_size);
    printf("status: '%s'\n", (char *)status);
 
    char *p = (char *)status;
    // write to the status memory, removing string null terminators
    // TBF: clean this up, and put stuff the FITS writer will look at
    memcpy(p, "DOG=BOO", sizeof("DOG=BOO"));
    p[7] = ' ';
    //memcpy(p+(1*80), "DATADIR=/tmp", sizeof("DATADIR=/tmp"));
    memcpy(p+(1*80), "CAT=FOO", sizeof("CAT=FOO"));
    p[(1*80)+7] = ' ';
    memcpy(p+(2*80), "END", sizeof("END"));
    p[(2*80)+3] = ' ';


    fitsio->copyStatusMemory((const char *)status);

    // Min. values to set 
    fitsio->setNumberStokes(1);
    fitsio->setNumberSubBands(2);
    fitsio->setNumberChannels(256);

    // use the current time
    timeval tv;
    gettimeofday(&tv, 0);
    start_time = this_timeval_2_mjd(&tv);    
    printf("start: %f\n", start_time);
    fitsio->set_startTime(start_time);

    fitsio->open();

    // create fake data (ramp)
    int i, dataSize = 40;
    int data[40];
    for (i=0; i<dataSize; i++)
        data[i] = i;

    // TBF: use ramp
    vegas_databuf *db;
    db = new vegas_databuf;
    db->header.shmid = 3;
    db->block[0].header.mcnt = 1;
    db->block[0].data[1] = 3.1;

    int numWrites = 10000;
    for (i=0; i<numWrites; i++)
    {
        fitsio->write(&(db->block[0]));
        db->block[0].header.mcnt += 1;
    }    

    fitsio->close();

    // calculate elapsed time
    gettimeofday(&end, 0);

    printf("start seconds: %d, micro: %d\n", start.tv_sec, start.tv_usec);
    printf("end seconds: %d, micro: %d\n", end.tv_sec, end.tv_usec);
    printf("elapsed seconds: %d\n", end.tv_sec - start.tv_sec);
    return (0);
}

