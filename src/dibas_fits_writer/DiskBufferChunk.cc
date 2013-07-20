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

static char rcs_id[] =  "$Id$";

// Parent
#include "DiskBufferChunk.h"
// Local
#include "fitshead.h"
#include "sdfits.h"
#include "transpose.h"
// STL
#include <cstring>
#include <sstream>
#include <string>
#include <iostream>


DiskBufferChunk::DiskBufferChunk(const char *fits_header,
                                 const sdfits_data_columns *data_header,
                                 float *in_data)
{
    if((data_header == 0) || (in_data == 0))
    {
        return;
    }
    time            = data_header->time;
    // only first 40 bits of time_counter are significant.
    time_counter    = data_header->time_counter & 0x000000FFFFFFFFFF;
    integration     = data_header->integ_num;
    exposure        = data_header->exposure;
    strncpy(object, data_header->object, STRING_LENGTH-1);
    azimuth         = data_header->azimuth;
    elevation       = data_header->elevation;
    bmaj            = data_header->bmaj;
    bmin            = data_header->bmin;
    bpa             = data_header->bpa;
    accumid         = data_header->accumid;
    sttspec         = data_header->sttspec;
    stpspec         = data_header->stpspec;
    center_freq_idx = data_header->centre_freq_idx;
    // Center frequencies
    for(int i = 0; i < MAX_SUBBANDS; ++i)
    {
        center_freq[i] = data_header->centre_freq[i];
    }
    ra              = data_header->ra;
    dec             = data_header->dec;
    // We will transpose the data, so go ahead and swap the dims as well
    hgeti4(fits_header, "NSUBBAND", &data_dims[0]);
    hgeti4(fits_header, "ONLY_I", &data_dims[1]);
    hgeti4(fits_header, "NCHAN", &data_dims[2]);
    if(data_dims[1] == 0) data_dims[1] = 4;
    // Data length
    int array_len = data_dims[0] * 4 * data_dims[2];
    data_len = array_len * (int)sizeof(float);
    // Fill out data array while doing transpose, keep dims straight!
    data = new float[array_len];
    transpose(in_data, data, data_dims[0], data_dims[2]); //, data_dims[1]);
}

DiskBufferChunk::~DiskBufferChunk()
{
    delete [] data;
}

double
DiskBufferChunk::getIntegrationStart()
{
    return time;
}

uint64_t
DiskBufferChunk::getIntegrationOffset()
{
    return time_counter;
}

int
DiskBufferChunk::getIntegrationNumber()
{
    return integration;
}

float
DiskBufferChunk::getExposure()
{
    return exposure;
}

std::string
DiskBufferChunk::getObject()
{
    return std::string(object);
}

float
DiskBufferChunk::getAzimuth()
{
    return azimuth;
}

float
DiskBufferChunk::getElevation()
{
    return elevation;
}

float
DiskBufferChunk::getBeamMajorLength()
{
    return bmaj;
}

float
DiskBufferChunk::getBeamMinorLength()
{
    return bmin;
}

float
DiskBufferChunk::getBeamPositionAngle()
{
    return bpa;
}

int
DiskBufferChunk::getAccumulationId()
{
    return accumid;
}

int
DiskBufferChunk::getSpectrumCountStart()
{
    return sttspec;
}

int
DiskBufferChunk::getSpectrumCountStop()
{
    return stpspec;
}

float
DiskBufferChunk::getCenterFrequencyIndex()
{
    return center_freq_idx;
}

double
DiskBufferChunk::getCenterFrequency(int subband)
{
    return center_freq[subband];
}

double
DiskBufferChunk::getRa()
{
    return ra;
}

double
DiskBufferChunk::getDec()
{
    return dec;
}

int
DiskBufferChunk::getDataLength()
{
    return data_len;
}

int*
DiskBufferChunk::getDataDimensions()
{
    return data_dims;
}

float*
DiskBufferChunk::getData()
{
    return data;
}

float*
DiskBufferChunk::getData(int subband)
{
    return data + subband*data_dims[1]*data_dims[2];
}

float*
DiskBufferChunk::getData(int subband, int stokes)
{
    return data + subband*data_dims[1]*data_dims[2] + stokes*data_dims[2];
}
