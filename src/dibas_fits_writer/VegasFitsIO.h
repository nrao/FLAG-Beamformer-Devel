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

#ifndef VEGASFITSIO
#define VEGASFITSIO

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
#define PACKET_RATE 303000
#define N 303

// time between dumps from the GPU
// #define INT_TIME (N / PACKET_RATE)

#include "FitsIO.h"
#include "SwitchingSignals.h"

extern "C"
{
#include "vegas_databuf.h"
}

class DiskBufferChunk;
#include <map>
#include <vector>
#include <string>

#include "Mutex.h"


/// A GBT-like Vegas/spectral-line Fits writing class
class VegasFitsIO : public FitsIO
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
    VegasFitsIO(const char *path_prefix, int simulator = 0);
    ~VegasFitsIO();

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
    void setBaseBw(float base_bw);
    void setNoiseSource(int noise_source);
    void setStatusMem(std::map<std::string, std::string> &status);
    void setBofFile(const char *);

public:
    /// Read the primary header information from status shared memory
    bool readPrimaryHeaderKeywords();
    /// Read the switching signal configuration information from status shared memory
    bool readStateTableKeywords();
    /// Read the low-level switching signal configuration information from status memory
    bool readActStateTableKeywords();
    /// Read the port table information from status memory
    bool readPortTableKeywords();
    /// Read the sampler table information from status memory
    bool readSamplerTableKeywords();

public:
    //PORT Table methods
    //The Bank Name is set in Primary HDU.
    //The PORT table for VEGAS will always contain 2 rows.
    void setMeasuredPower(float measpwr, int index);
    void setNoiseTone(int noiseTone, int index);

public:
    ///@{
    /// STATE Table methods
    void setBlanking(const double *blanking);
    void setCalState(const int *cal_state);
    void setPhaseStart(const double *phase_start);
    void setSigRefState(const int *sig_ref_state);
    void setSwitchPeriod(double switch_period);
    ///@}

public:
    /// SAMPLER Table methods
    void setPolarization(const char *pol);
    void setNumberStokes(int stokes);
    void setNumberSubBands(int subbands);
    void setReferenceChannel(float refchan);
    void setChannelCenterFreq(const double*);
    void setChannelFreqIncrement(const double*);
    void setChannelFreqResolution(const double*);
    //@}

public:
    /// ACT_STATE Table methods
    void setEcal(const int *ecal);
    void setEsigref1(const int *esr1);
    void setEsigref2(const int *esr2);
    void setIcal(const int *ical);
    void setIsigref1(const int *isr1);
    void setIsigref2(const int *isr2);
    void setNumberPhases(int num_phases);
    void setSwitchingSource(int source);

public:
    /// DATA Table methods
    void setFpgaClock(float fpga_clock);
    void setRequestedIntegrationTime(float exposure);
    void setSwPerInt(int sw_per_int);

public:
    virtual void createPrimaryHDU();
    virtual void createPortTable();
    virtual void createStateTable();
    void createSamplerTable();
    void createActStateTable();
    void createDataTable();

    int bufferedWrite(DiskBufferChunk *chunk, bool new_integration = false);
    int write(int mcnt, float *data);
    bool is_scan_complete();
    void set_scan_complete();

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
    //<group>
    char theBank[2];
    char theVEGASMode[256];
    int numberChannels;
    int selfTest;
    float theBaseBw;
    char theNoiseSource[4];
    //</group>

    //PORT Table
    //<group>
    float measuredPower[NUMPORTS];
    char  *noiseTone[NUMPORTS];
    //</group>

    //STATE Table
    double blanking[MAXPHASES];
    double phaseStart[MAXPHASES];
    enum SwitchingSignals::CalState calState[MAXPHASES];
    enum SwitchingSignals::SigRefState sigRefState[MAXPHASES];
    double switchPeriod;

    //SAMPLER Table
    //<group>
    char polarization[256];
    int numberStokes;
    int numberSubBands;
    float referenceChannel;
    double crval1[MAXSUBBANDS];
    double cdelt1[MAXSUBBANDS];
    double freqres[MAXSUBBANDS];
    //</group>

    //ACT_STATE Table
    int ecal[MAXPHASES];
    int esigref1[MAXPHASES];
    int esigref2[MAXPHASES];
    int ical[MAXPHASES];
    int isigref1[MAXPHASES];
    int isigref2[MAXPHASES];
    int numberPhases;
    int switchingSource;

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

    /// A class to track the fpga time_counter.
    /// The counter is only 40 bits in length, but this tracks roll-over extending the
    /// count to a full 64 bits.
    struct fpga_time_counter
    {
        fpga_time_counter();
        void add_lsw(uint64_t val);
        uint64_t get_offset();
        void clear();

        uint64_t msw;
        uint64_t lsw;
        uint64_t last_lsw;
        uint64_t delta;
    };

    fpga_time_counter _time_counter;
    uint64_t time_ctr_40bits;
    std::map<std::string, std::string> _status_mem;
    std::string _bof_file;

    char status_buffer[STATUS_MEMSIZE];
    std::vector<std::string> status_mem_keywords;
    int32_t port_hdu;
    int32_t state_hdu;
    int32_t sampler_hdu;
    int32_t actstate_hdu;
    int32_t data_hdu;
    double scan_time_clock;
    int32_t accumid_xor_mask;

    double calculateBlockTime(int mcnt, double startDMJD);
};

#endif
