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

#include "VegasFitsIO.h"

#define THIS_MJD_1970_EPOCH (40587)
double this_timeval_2_mjd(timeval *tv)
{
    double dmjd = tv->tv_sec/86400 + THIS_MJD_1970_EPOCH;
    dmjd += (tv->tv_sec % 86400)/86400.0;
    return dmjd;
}

int mainTest(int argc, char **argv) {

    size_t status_size = 184320; // (2304*80)
    double start_time = 0;
    VegasFitsIO *fitsio;    

    fitsio = new VegasFitsIO("/tmp", false);

    // sim. status memory buffer contents
    void *status = malloc(sizeof(char)*status_size);
    memset(status, ' ', sizeof(char)*status_size);
    printf("status: '%s'\n", (char *)status);
 
    char *p = (char *)status;
    /*
    // This example works
    memcpy(p, "DOG=BOO", sizeof("DOG=BOO"));
    p[7] = ' ';
    memcpy(p+10, "CAT=MAX", sizeof("CAT=MAX"));
    p[17] = ' ';
    memcpy(p+20, "RAT=FAT", sizeof("RAT=FAT"));
    p[27] = ' ';
    memcpy(p+30, "END", sizeof("RAT=FAT"));
    int j= 0;
    for(j=0; j<30; j++)
        printf("j: %d, p[j] = '%c'\n", j, p[j]);

    printf("first 10\n");
    std::string boo = std::string((const char *)status, 0, 10);
    printf("boo: %s, %d\n", boo.c_str(), boo.size());
    printf("next 10\n");
    std::string max = std::string((const char *)status, 10, 10);
    printf("max: %s, %d\n", max.c_str(), max.size());
    printf("next 10\n");
    std::string rat = std::string((const char *)status, 20, 10);
    printf("rat: %s, %d\n", rat.c_str(), rat.size());
    */

    
    // write to the status memory, removing string null terminators
    // TBF: clean this up, and put stuff the FITS writer will look at
    memcpy(p, "DOG=BOO", sizeof("DOG=BOO"));
    p[7] = ' ';
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

    fitsio->write(data);

    fitsio->close();

    return 0;
}
