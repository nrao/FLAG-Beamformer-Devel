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
#include <iostream>
#include <iomanip>
#include <algorithm>

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
#include "BfCovFitsIO.h"
#include "BfPulsarFitsIO.h"

int mainTest(bool cov_mode, int argc, char **argv)
{
    printf("Beamformer FITS Festival! Cov. Matrix mode? %d\n", cov_mode);
    int rv = 0;
    if (cov_mode)
        rv = mainTestCov(argc, argv);
    else    
        rv = mainTestPulsar(argc, argv);
    return rv;    
}

// TBF
int mainTestPulsar(int argc, char **argv)
{
    return 0;
}

/* Test basic functionality of the BfCovFitsIO class */
int mainTestCov(int argc, char **argv)
{
    BfCovFitsIO *fitsio = new BfCovFitsIO("/tmp", false);
    fitsio->setBankName('A'); //, 1);

    double start_time = 0;
    timeval tv;
    gettimeofday(&tv, 0);
    // Get the current time as an MJD for use in the FITS file names
    start_time = BfFitsIO::timeval_2_mjd(&tv);

    fitsio->set_startTime(start_time);
    fitsio->open();

    const int num_chans = 128; // number of channels
    const int bin_size = 820; // number of complex pair elements in a bin/channel
    const int chan_size = bin_size * 2; // number of floats in a channel
    const int num_floats = num_chans * chan_size; // total number of floats

    // Create a standard vector that contains the correct number of elements (initialized to 0)
    std::vector<float> fits_data (num_floats, 0);

    // now test out simple writeRow
    fitsio->writeRow(0, fits_data.data());

    // now test out more complicated shit
    // create some fake GPU data: 64 x 64 x 2 matrix in 1-d
    int numChan = 5;
    int M = 40;
    int cmpSz = 2;
    int covDataSz = ((M * (M + 1))/2);
    int gpuDataSz = covDataSz + (M/2);
    int fitsSz = covDataSz * cmpSz * numChan;
    int gpuSz = gpuDataSz  * cmpSz * numChan;
    float gpu_matrix[gpuSz];
    float fits[fitsSz];
    // init the gpu matrix
    int i = 0;
    for (i = 0; i<gpuSz; i++)
        gpu_matrix[i] = (float)i;
    //for (i = 0; i<10; i++)
    //    printf("%f\n", fits_data[i]);
    printf("M: %d, # chans: %d, Gpu Data Size: %d, Fits Data Size: %d\n", M, numChan, gpuDataSz, covDataSz);
    printf("Parse!\n");
    fitsio->parseGpuCovMatrix(gpu_matrix, gpuDataSz, fits, covDataSz, numChan);    
    //for (i = 0; i<fitsSz; i++)
    //    printf("fits[%d] = %f\n", i, fits[i]);
    
    // if that worked, try to write w/ it
    //fitsio->write(1, gpu_matrix);

    // cleanup 
    fitsio->close();
}

int fishFits2CovFitsTest(int argc, char **argv)
{
    fitsfile *fptr;
    const std::string filename("/home/scratch/npingel/FLAG/data/TGBT14B_913_04/PafSoftCorrel/2015_01_26_09:47:21.fits");
    int status = 0;
    int iomode = READONLY;
    fits_open_file(&fptr, filename.c_str(), iomode, &status);
    
    if (status)
        printf("bad status: %d\n", status);

    // Look at the second table (which is the first data table)
    fits_movabs_hdu(fptr, 2, NULL, &status);
     
    if (status)
        printf("bad status: %d\n", status);

    const int num_chans = 128; // number of channels
    const int bin_size = 820; // number of complex pair elements in a bin/channel
    const int chan_size = bin_size * 2; // number of floats in a channel
    const int num_floats = num_chans * chan_size; // total number of floats


    // Create a standard vector that contains the correct number of elements (initialized to 0)
    std::vector<float> fits_data (num_floats, 0);
    // Read the data from the PAF comm. data as floats into fits_data
    // A std::vector has a 'data' segment, but it also has some metadata
    // This means that we can't just send a pointer to the vector; we must
    //   send a pointer to fits_data.data() so that we can put the elements
    //   directly into the vector
    fits_read_col(fptr, TFLOAT, 1, 1, 1, num_floats, NULL, fits_data.data(), NULL, &status);

    if (status)
        printf("bad status: %d\n", status);

    // At this point we should have all of the data we want stored in a vector. Let's check:
    std::cout << "\n\"initial\" fits_data:" << std::endl;
    std::cout << "\tnumber of elements: " << fits_data.size() << std::endl;
    std::cout << "\tnumber of channels: " << fits_data.size() / chan_size << std::endl;

    // Now let's copy over the data that we want into a new vector,
    //   leaving behind the first and last 4 channels
    // Let's set an iterator that "points" to the beginning of the 120 channels we want to keep
    std::vector<float>::const_iterator trunc_begin = fits_data.begin() + (4 * chan_size);
    // Then an iterator that "points" to the end of the 120 channels we want to keep
    std::vector<float>::const_iterator trunc_end = fits_data.end() - (4 * chan_size);
    // Then we copy the data over into a new vector with this constructor call
    std::vector<float> trunc_data(trunc_begin, trunc_end);

    // At this point we should have removed the first and last 4 channels. Let's check:
    std::cout << "\n\"truncated\" fits_data:" << std::endl;
    std::cout << "\tnumber of elements: " << trunc_data.size() << std::endl;
    std::cout << "\tnumber of channels: " << trunc_data.size() / chan_size << std::endl << std::endl;

    // Check to make sure that all of the elements have been copied over correctly
    // Basically we start at the first element of the 5th channel in the fits_data,
    //   which is where we should have started copying the data
    for (auto fits_it = fits_data.begin() + (4 * chan_size), trunc_it = trunc_data.begin();
         trunc_it != trunc_data.end();
         fits_it++, trunc_it++)
    {
        if (*fits_it != *trunc_it)
            std::cout << "Error copying data over" << std::endl;
    }

    // TBF: parse the FISHFITS data to convert to input and frequency space
    // TBF: then convert to the 10 GPU's frequency space and write each to its own FITS file
    
    BfCovFitsIO *fitsio;

    const int num_banks = 10;
    char banks[num_banks];
    // char ch;
    // Give every bank a sequential letter representation
    // Right now this will yield: {A, B, ... , J}
    for (int i = 0, ch = 'A'; i < num_banks; i++, ch++)
    	banks[i] = ch;
    
    double start_time = 0;
    timeval tv;
    gettimeofday(&tv, 0);
    // Get the current time as an MJD for use in the FITS file names
    start_time = BfFitsIO::timeval_2_mjd(&tv);

    for (int i = 0; i < 10; i++)
    {
        // Create a BfFitsIO writer
        fitsio = new BfCovFitsIO("/tmp", false);

        printf("setting bank: %c, %d\n", banks[i], i);
        fitsio->setBankName(banks[i]); //, 1);

        fitsio->set_startTime(start_time);
        fitsio->open();

        printf("Sending pointer to element number %d\n", i * num_floats / 10);
        fitsio->writeRow(0, fits_data.data() + (i * num_floats / 10));
        
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
//     start_time = timeval_2_mjd(&tv);    
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

