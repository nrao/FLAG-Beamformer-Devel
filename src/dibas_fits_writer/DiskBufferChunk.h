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

//# $Id$

#ifndef DISK_BUFFER_CHUNK_H
#define DISK_BUFFER_CHUNK_H

#include <string>
#include <stdint.h>

#define STRING_LENGTH 16
#define MAX_SUBBANDS   8
#define DATA_DIMS      3

struct sdfits_data_columns;

/// DiskBufferChunk.h
/// A class to process a dataset from a shared memory data block.
class DiskBufferChunk
{
public:
    
    /// @param fits_header The header for this dataset
    /// @param data_header column header data
    /// @param in_data a pointer to the dataset in the current data block
    DiskBufferChunk(const char *fits_header,
                    const sdfits_data_columns *data_header,
                    float *in_data);
    ~DiskBufferChunk();

    double getIntegrationStart();  // MJD at start of integration (system time)
    uint64_t getIntegrationOffset(); // FPGA time counter at start of integration
    int    getIntegrationNumber();
    float  getExposure();
    std::string getObject();
    float  getAzimuth();
    float  getElevation();
    float  getBeamMajorLength();
    float  getBeamMinorLength();
    float  getBeamPositionAngle();
    int    getAccumulationId();
    int    getSpectrumCountStart();
    int    getSpectrumCountStop();
    float  getCenterFrequencyIndex();
    double getCenterFrequency(int subband);
    double getRa();
    double getDec();
    int    getDataLength();
    int   *getDataDimensions();
    float *getData();
    float *getData(int subband);
    float *getData(int subband, int stokes);

    // Data Handling
    int getSize() const;
    int serialize(char *buffer);
    int parse(const char *buffer);

private:
    double time;
    uint64_t time_counter;
    int integration;
    float exposure;
    char object[STRING_LENGTH];
    float azimuth;
    float elevation;
    float bmaj;
    float bmin;
    float bpa;
    int accumid;
    int sttspec;
    int stpspec;
    float center_freq_idx;
    double center_freq[MAX_SUBBANDS];
    double ra;
    double dec;
    int data_len;
    int data_dims[DATA_DIMS];
    float *data;
};

#endif//DISK_BUFFER_CHUNK_H
