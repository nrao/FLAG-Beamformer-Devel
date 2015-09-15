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

#define ELAPSED_NS(start,stop) \
  (((int64_t)stop.tv_sec-start.tv_sec)*1000*1000*1000+(stop.tv_nsec-start.tv_nsec))

#ifndef BFFITSIO
#define BFFITSIO

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

#define STATUS_MEMSIZE 184320

// Rate (Hz) that the packets arrive from the roach
// Each packet has it's own 'mcnt'.
// 10 Samples in each packet.
#define PACKET_RATE 600
// #define PACKET_RATE 9492
#define N 30
// #define N 4746 // from spreadsheet

// time between dumps from the GPU
// #define INT_TIME (N / PACKET_RATE)

#define MJD_1970_EPOCH (40587)    

#include "FitsIO.h"
#include "SwitchingSignals.h"

extern "C"
{
#include "bf_databuf.h"
}

class DiskBufferChunk;
#include <map>
#include <vector>
#include <string>

#include "Mutex.h"


/// A GBT-like Vegas/spectral-line Fits writing class
class BfFitsIO : public FitsIO
{
public:
    static const int NUMPORTS  = 2;
    static const int MAXSTOKES = 4;
    static const int MAXPHASES = 8;
    static const int MAXSUBBANDS = 8;
    static const int MAXCHANNELS = 32768;
    /// @param path_prefix is the first portion of the
    /// root directory for the FITS file path.
    /// @param simulator is a flag which sets the SIMULATE primary header keyword
    BfFitsIO(const char *path_prefix, int simulator = 0, int instance_id = 0);
    ~BfFitsIO();

    virtual int myAbstract() = 0;

    // This method opens the FITS file for writing.
    // The FitsIO status is returned.
    // <group>
    int open();//const TimeStamp &ts);
    int close();
    // </group>

    // Glean as much info as we can from the status memory area
    // the status_buffer should be a copy, not the real buffer
    void copyStatusMemory(const char *status_buffer);

    // Get the file name, without the datadir prefix, used to
    // open the last file. Returns NULL if no file is open.
    const char *getFilePath();

public:
    //PRIMARY HDU Methods
    void setScanLength(const TimeStamp &t);
    void setBankName(char bank);
    void setMode(const char *mode);
    void setNumberChannels(int numChannels);
    void setSelfTestMode(int selfTestMode);
    // void setBaseBw(float base_bw);
    // void setNoiseSource(int noise_source);
    void setStatusMem(std::map<std::string, std::string> &status);
    // void setBofFile(const char *);

public:
    // Read the primary header information from status shared memory
    bool readPrimaryHeaderKeywords();

public:
    virtual void createPrimaryHDU();
    void createDataTable();

    // int bufferedWrite(DiskBufferChunk *chunk, bool new_integration = false);
    int writeRow(int mcnt, float *data);
    virtual int write(int mcnt, float *data) = 0;
    bool is_scan_complete();
    void set_scan_complete();
    static double timeval_2_mjd(timeval *tv);
    static unsigned long dmjd_2_secs(double dmjd);

protected:
    int openFlag;
    int nrows;
    double dmjd;
    char theProjectId[256];
    char theStartTimeStr[256];
    char theFilePath[256];
    TimeStamp scanLength;
    TimeStamp stopTime;

    //Primary HDU
    char theBank[2];
    char theVEGASMode[256];
    int numberChannels;
    int selfTest;
    float theBaseBw;
    char theNoiseSource[4];

    //DATA Table
    float fpgaClock;
    float requestedIntegrationTime;
    int theSwPerInt;
    int integ_num;
    std::vector<int> accumid;
    std::vector<int> sttspec;
    std::vector<int> stpspec;

    // Pipeline storage
    TimeStamp integration_start_time;
    float *integration_time;
    float *fits_data;
    double utcfrac;
    int current_row;
    bool scan_is_complete;
    Mutex lock_mutex;

    std::map<std::string, std::string> _status_mem;

    char status_buffer[STATUS_MEMSIZE];
    std::vector<std::string> status_mem_keywords;
    int32_t data_hdu;
    double scan_time_clock;

    double calculateBlockTime(int mcnt, double startDMJD);

    struct timespec data_w_start, data_w_stop;

    int data_size;
    char data_form[256];

    int instance_id;
};

#endif
