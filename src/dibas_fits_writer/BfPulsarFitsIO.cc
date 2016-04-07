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

#include <stdio.h>
#include "BfPulsarFitsIO.h"

BfPulsarFitsIO::BfPulsarFitsIO(const char *path_prefix, int simulator, int instance_id) : BfFitsIO(path_prefix, simulator, instance_id)
{
    // What distinquishes modes is their data format
    data_size = NUM_BEAMS * NUM_PULSAR_CHANNELS*NUM_STOKES;
    printf("THE DATA SIZE IS %d\n",data_size);
    sprintf(data_form, "%dE", data_size);
}

int
BfPulsarFitsIO::writeRow(int mcnt, float *data)
{
    int column = 1;
    MutexLock l(lock_mutex);
    l.lock();

    // TODO: Make current_row a local variable or come up with a 
    // reason it shouldn't be

    // DMJD column
    double dmjd = calculateBlockTime(mcnt, startTime);
    write_col_dbl(column++,
                  current_row,
                  1,
                  1,
                  &dmjd);

    // MCNT column
    write_col_int(column++,
                  current_row,
                  1,
                  1,
                  &mcnt);

    clock_gettime(CLOCK_MONOTONIC, &data_w_start);
    // DATA column
    write_col_flt(column++,
                  current_row,
                  1,
                  data_size, //FITS_BIN_SIZE * NUM_CHANNELS,
                  data);
     clock_gettime(CLOCK_MONOTONIC, &data_w_stop);

    ++current_row;

    l.unlock();
    report_error(stderr, getStatus());
    return getStatus();
 }

int BfPulsarFitsIO::write(int mcnt, float *data) {
    
    printf("about to parse data\n");
    printf("writing data\n");    
    writeRow(mcnt, data);
    printf("Done writing data\n");
    return 1;
}


int BfPulsarFitsIO::write_FRB(int mcnt, float *data) {
        return 1;
}

int BfPulsarFitsIO::write_PAF(int mcnt, float *data) {
        return 1;
}

int BfPulsarFitsIO::write_HI(int mcnt, float *data) {
        return 1;
}

// example implementation of abstract method
int BfPulsarFitsIO::myAbstract() {
    return 0;
}
