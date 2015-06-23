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

#define THIS_MJD_1970_EPOCH (40587)
double this_timeval_2_mjd(timeval *tv)
{
    double dmjd = tv->tv_sec/86400 + THIS_MJD_1970_EPOCH;
    dmjd += (tv->tv_sec % 86400)/86400.0;
    return dmjd;
}

int mainTest(int argc, char **argv)
{

    // std::vector<float> test;
    // for (int i = 0; i < 10; i++)
    //     test.push_back(i);
    // std::vector<float> other_test (test.begin() + 4, test.end() - 4);

    // std::cout << "test" << std::endl;
    // for (auto it = test.begin(); it != test.end(); ++it)
    //     std::cout << *it << std::endl;

    // std::cout << "other test" << std::endl;
    // for (auto it = other_test.begin(); it != other_test.end(); ++it)
    //     std::cout << *it << std::endl;

    // std::cout << "things and toehr thaingasd gjas " << std::endl;
    // std::cout << other_test.back() << std::endl;
    // std::cout << *(test.end() - 4 - 1) << std::endl;

    // return 0;


    printf("Beamformer FITS Festival!\n");

    fitsfile *fptr;
    std::string filename("/home/scratch/npingel/FLAG/data/TGBT14B_913_04/PafSoftCorrel/2015_01_26_09:47:21.fits");
    int status = 0;
    int iomode = READONLY;
    fits_open_file(&fptr, filename.c_str(), iomode, &status);
    
    if (status)
        printf("bad status: %d\n", status);

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
    
    // float test[num_floats];
    // fits_read_col(fptr, TFLOAT, 1, 1, 1, num_floats, NULL, test, NULL, &status);

    // for (int i = 0; i < num_floats; i++)
    // {
    //     if (fits_data[i] != test[i])
    //         std::cout << "wut" << std::endl;
    // }

    // return 0;


    if (status)
        printf("bad status: %d\n", status);

    // At this point we should have all of the data we want stored in a vector. Let's check:
    std::cout << "\n\"initial\" fits_data:" << std::endl;
    std::cout << "\tnumber of elements: " << fits_data.size() << std::endl;
    std::cout << "\tnumber of channels: " << fits_data.size() / chan_size << std::endl;

    // Now let's copy over the data that we want into a new vector,
    //   leaving behind the first and last 4 channels
    // Let's set an iterator that "points" to the beginning of the 120 channels we want to keep
    // std::vector<float>::const_iterator trunc_begin = fits_data.begin() + (4 * chan_size);
    // // Then an iterator that "points" to the end of the 120 channels we want to keep
    // std::vector<float>::const_iterator trunc_end = trunc_begin + num_floats;
    // // Then we copy the data over into a new vector with this constructor call
    // std::vector<float> trunc_data(trunc_begin, trunc_end);

    std::vector<float> trunc_data = fits_data;

    // Erase the first 4 channels
    trunc_data.erase(trunc_data.begin(), trunc_data.begin() + (4 * chan_size));
    // Erase the last 4 channels
    trunc_data.erase(trunc_data.end() - (4 * chan_size), trunc_data.end());

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
    
    BfFitsIO *fitsio;
    // char fitsfile[256];
    // char banks[10] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'};

    const int num_banks = 10;
    char banks[num_banks];
    char ch;
    // Give every bank a sequential letter representation
    for (int i = 0, ch = 'A'; i < num_banks; i++, ch++)
    	banks[i] = ch;
    
    // Get the current time as an MJD for use in the FITS file names
    double start_time = 0;
    timeval tv;
    gettimeofday(&tv, 0);
    start_time = this_timeval_2_mjd(&tv);

    for (int i = 0; i < 10; i++)
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


        
        printf("Sending pointer to element number %d\n", i * num_floats / 10);
        fitsio->write(0, fits_data.data() + (i * num_floats / 10));
        
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

