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

#ifndef BFCOVFITSIO
#define BFCOVFITSIO

#include "BfFitsIO.h"

class BfCovFitsIO : public BfFitsIO
{
public:
    BfCovFitsIO(const char *path_prefix, int simulator = 0, int instance_id = 0);
    void parseGpuCovMatrix(float const *const gpu_matrix, float *const fits_matrix);
    void parseGpuCovMatrix(float const *const gpu_matrix, int gpu_size, float *const fits_matrix, int fits_size, int num_channels);
    int write(int mcnt, float *data);
    void testthis(float *const fits_matrix);
    int myAbstract();
    void parseAndReorderGpuCovMatrix(float const *const gpu_matrix, int gpu_corr_num, float *const fits_matrix, int fits_corr_num, int num_channels);
};
#endif
