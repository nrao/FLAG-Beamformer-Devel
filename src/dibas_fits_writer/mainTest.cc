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
#include "bf_databuf.h"
#include "spead_heap.h"
#include "fitshead.h"
#define STATUS_KEYW "DISKSTAT"
#include "vegas_threads.h"
};

#include "mainTest.h"
#include "BfFitsIO.h"

#define THIS_MJD_1970_EPOCH (40587)
double this_timeval_2_mjd(timeval *tv)
{
    double dmjd = tv->tv_sec/86400 + THIS_MJD_1970_EPOCH;
    dmjd += (tv->tv_sec % 86400)/86400.0;
    return dmjd;
}

int mainTest(int argc, char **argv)
{
    printf("Beamformer FITS Festival!\n");

    fitsfile *fptr;
    std::string filename("/home/scratch/npingel/FLAG/data/TGBT14B_913_04/PafSoftCorrel/2015_01_26_09:47:21.fits");
    int status = 0;
    int iomode = READONLY;
    fits_open_file(&fptr, filename.c_str(), iomode, &status);
    
    printf("status: %d\n", status);
    fits_movabs_hdu(fptr, 2, NULL, &status);
     
    printf("status: %d\n", status);

    const int num_chans = 128;
    const int bin_size = 820;
    const int sizeof_chan = 820 * 2;
    const int num_els = num_chans * bin_size * 2;


    double data[num_els];
    float trunc_data[(num_chans - 8) * bin_size * 2];

    fits_read_col(fptr, TDOUBLE, 1, 1, 1, num_els, NULL, data, NULL, &status);

    printf("status: %d\n", status);

    // Now we need to convert all of these doubles to floats because that's how they're handled in the BF system currently
    // We are also chopping off the first and last 4 channels here, by starting/stopping the loop 4 channels late/early
    int i, j;
    for (i = 0, j = 4 * sizeof_chan; j < num_els - (4 * sizeof_chan); i++, j++)
    {
    	trunc_data[i] = (float)data[j];
    }



    // No longer relevant; it appears that we are not losing any precision
    // // Now let's compare the values to see how badly we've messed them up
    // for (i = 0; i < 10; i++)
    // {
    // 	printf("double: data[%d] =       %f\n", i, data[i]);
    // 	printf("float:  trunc_data[%d] = %f\n", i, trunc_data[i]);
    // }

    // TBF: parse the FISHFITS data to convert to input and frequency space
    // TBF: then convert to the 10 GPU's frequency space and write each to it's own FITS file
    
    BfFitsIO *fitsio;
    // char fitsfile[256];
    // char banks[10] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'};

    const int num_banks = 10;
    char banks[num_banks];
    char c;
    // Give every bank a sequential letter representation
    for (i = 0, c = 'A'; i < num_banks; i++, c++)
    	banks[i] = c;
    
    // Get the current time as an MJD for use in the FITS file names
    double start_time = 0;
    timeval tv;
    gettimeofday(&tv, 0);
    start_time = this_timeval_2_mjd(&tv);



    for (i=0; i<10; i++)
    {
        //sprintf(fitsfile, "/tmp/2015_01_26_09:47:21%s.fits" , banks[0]);
        //printf("fitsfile: %s\n", fitsfile);

        // Create a BfFitsIO writer
        fitsio = new BfFitsIO("/tmp", false);

        printf("setting bank: %c, %d\n", banks[i], i);
        fitsio->setBankName(banks[i]); //, 1);

        // Min. values to set 
        // fitsio->setNumberStokes(1);
        // fitsio->setNumberSubBands(2);
        // fitsio->setNumberChannels(256);
        

        fitsio->set_startTime(start_time);
        fitsio->open();


        
        printf("Sending pointer to element number %d\n", i * num_els / 10);
        fitsio->write(0, trunc_data + (i * num_els / 10));
        
        fitsio->close();
        delete fitsio;
    }

    return EXIT_SUCCESS;
}

// int mainTest2(int argc, char **argv)
// {

//     timeval start, end;
//     gettimeofday(&start, 0);
    
//     size_t status_size = 184320; // (2304*80)
//     double start_time = 0;

//     BfFitsIO *fitsio;
//     // Create a BfFitsIO writer
//     fitsio = new BfFitsIO("/tmp", false);

//     printf("mainTest.c\n");

//     // sim. status memory buffer contents
//     void *status = malloc(sizeof(char)*status_size);
//     memset(status, ' ', sizeof(char)*status_size);
//     //printf("status: '%s'\n", (char *)status);
 
//     char *p = (char *)status;
//     // write to the status memory, removing string null terminators
//     // TBF: clean this up, and put stuff the FITS writer will look at
//     memcpy(p, "DOG=BOO", sizeof("DOG=BOO"));
//     p[7] = ' ';
//     //memcpy(p+(1*80), "DATADIR=/tmp", sizeof("DATADIR=/tmp"));
//     memcpy(p+(1*80), "CAT=FOO", sizeof("CAT=FOO"));
//     p[(1*80)+7] = ' ';
//     memcpy(p+(2*80), "END", sizeof("END"));
//     p[(2*80)+3] = ' ';


//     fitsio->copyStatusMemory((const char *)status);

//     // Min. values to set 
//     fitsio->setNumberStokes(1);
//     fitsio->setNumberSubBands(2);
//     fitsio->setNumberChannels(256);

//     // use the current time
//     timeval tv;
//     gettimeofday(&tv, 0);
//     start_time = this_timeval_2_mjd(&tv);    
//     printf("start: %f\n", start_time);
//     fitsio->set_startTime(start_time);

//     fitsio->open();

//     // create fake data (ramp)
//     int i, dataSize = 40;
//     int data[40];
//     for (i=0; i<dataSize; i++)
//         data[i] = i;

//     // TBF: use ramp
//     bf_databuf *db;
//     db = new bf_databuf;
//     db->header.shmid = 3;
//     db->block[0].header.mcnt = 1;
//     db->block[0].data[1] = 3.1;

//     int numWrites = 100;
//     for (i=0; i<numWrites; i++)
//     {
//         fitsio->write(&(db->block[0]));
//         db->block[0].header.mcnt += 1;
//     }    

//     fitsio->close();

//     // calculate elapsed time
//     gettimeofday(&end, 0);

//     printf("start seconds: %d, micro: %d\n", start.tv_sec, start.tv_usec);
//     printf("end seconds: %d, micro: %d\n", end.tv_sec, end.tv_usec);
//     printf("elapsed seconds: %d\n", end.tv_sec - start.tv_sec);
//     return (0);
// }

