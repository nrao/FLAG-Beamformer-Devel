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

#ifndef BFPULSARFITSIO
#define BFPULSARFITSIO

#define NUM_BEAMS  7
#define NUM_PULSAR_CHANNELS 50
#define NUM_STOKES 3

#include "BfFitsIO.h"

class BfPulsarFitsIO : public BfFitsIO
{
public:
    BfPulsarFitsIO(const char *path_prefix, int simulator = 0, int instance_id = 0);
    int write(int mcnt, float *data);
    int write_HI(int mcnt, float *data);
    int write_PAF(int mcnt, float *data);
    int write_FRB(int mcnt, float *data);
    int myAbstract();
    int writeRow(int mcnt, float *data);

};

#endif
