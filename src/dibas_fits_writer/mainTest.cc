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
    printf("Beamformer FITS Festival!");

    fitsfile *fptr;
    char *filename = "/home/scratch/npingel/FLAG/data/TGBT14B_913_04/PafSoftCorrel/2015_01_26_09:47:21.fits";
    int status = 0;
    int iomode = READONLY;
    fits_open_file(&fptr, filename, iomode, &status);
    
    printf("status: %d\n", status);
    fits_movabs_hdu(fptr, 2, NULL, &status);
     
    printf("status: %d\n", status);
    double data[209920];

    fits_read_col(fptr, TDOUBLE, 1, 1, 1, 209920, NULL, data, NULL, &status);

    printf("status: %d\n", status);

    // TBF: parse the FISHFITS data to convert to input and frequency space
    // TBF: then convert to the 10 GPU's frequency space and write each to it's own FITS file
    
    VegasFitsIO *fitsio;
    char fitsfile[256];
    char banks[10][2] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};
    int i = 0;

    // use the current time
    double start_time = 0;
    timeval tv;
    gettimeofday(&tv, 0);
    start_time = this_timeval_2_mjd(&tv);    

    for (i=0; i<10; i++)
    {
        //sprintf(fitsfile, "/tmp/2015_01_26_09:47:21%s.fits" , banks[0]);
        //printf("fitsfile: %s\n", fitsfile);

        // Create a VegasFitsIO writer
        fitsio = new VegasFitsIO("/tmp", false);

        printf("setting bank: %s, %d\n", banks[i], i);
        fitsio->setBankName(banks[i][0]); //, 1);

        // Min. values to set 
        fitsio->setNumberStokes(1);
        fitsio->setNumberSubBands(2);
        fitsio->setNumberChannels(256);
        

        fitsio->set_startTime(start_time);
        fitsio->open();
        
        //fitsio->write(&(db->block[0]));
        
        fitsio->close();
        delete fitsio;
    }


}

int mainTest2(int argc, char **argv)
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
    //printf("status: '%s'\n", (char *)status);
 
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

    int numWrites = 100;
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

