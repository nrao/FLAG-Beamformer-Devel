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

#include "BfCovFitsIO.h"

BfCovFitsIO::BfCovFitsIO(const char *path_prefix, int simulator) : BfFitsIO(path_prefix, simulator)
{
    // What distinquishes modes is their data format
    data_size = FITS_BIN_SIZE * NUM_CHANNELS;
    sprintf(data_form, "%dC", data_size);
}

// example implementation of abstract method
int BfCovFitsIO::myAbstract() {
    return 1;
}    

// covariance data coming out of GPU has tons of zeros and some
// redundant values that need to be purged first
int BfCovFitsIO::write(int mcnt, float *data) {
    // For a matrix of 40x40 there will be 20 redundant values
    float fits_matrix[NUM_CHANNELS * FITS_BIN_SIZE * 2];
    parseGpuCovMatrix(data, fits_matrix);
    writeRow(mcnt, fits_matrix);
    return 1;
}    
