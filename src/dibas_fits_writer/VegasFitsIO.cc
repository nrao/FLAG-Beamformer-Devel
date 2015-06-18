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

// Local
#include "DiskBufferChunk.h"
#include "VegasFitsIO.h"
// YGOR
#include "FitsIO.h"
// #include "util.h"
// STL
#include <errno.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>

#include "fitshead.h"
#include "vegas_error.h"

using namespace std;

static bool verbose = false;
#define dbprintf if (verbose) printf

Mutex::Mutex()
{
    pthread_mutex_init(&mutex, 0);
}
Mutex::~Mutex()
{
    pthread_mutex_destroy(&mutex);
}

int Mutex::lock()
{
    return pthread_mutex_lock(&mutex);
}

int Mutex::unlock()
{
    return pthread_mutex_unlock(&mutex);
}

#define FITS_VERSION "1.0"
#define MAX_40BITS 0x000000FFFFFFFFFF

VegasFitsIO::fpga_time_counter::fpga_time_counter()
    : msw(0),
      lsw(0),
      last_lsw(0),
      delta(0)
{
}

void VegasFitsIO::fpga_time_counter::add_lsw(uint64_t val)
{
    lsw = val & MAX_40BITS;

    if (lsw > last_lsw)
    {
        delta = lsw - last_lsw;
    }

    // Rollover is detected if the given lsw is less than the last one,
    // and the last one was near the rollover point. This protects
    // against any glitches (which are bugs and should be fixed in any case).
    if (lsw < last_lsw && last_lsw > (MAX_40BITS - delta * 2)) // rolled over
    {
        msw += 0x10000000000;   // lsw is 40 bits, thus we skip the first 40 of msw.
    }

    last_lsw = lsw;
}

uint64_t VegasFitsIO::fpga_time_counter::get_offset()
{
    uint64_t val;

    val = msw | lsw;
    return val;
}

void VegasFitsIO::fpga_time_counter::clear()
{
    msw = lsw = last_lsw = delta = 0;
}

/// path_prefix The environment varable to use which contains
/// the directory prefix for the data files.
/// simulator A boolean value which sets the 'SIMULATE' header keyword.
VegasFitsIO::VegasFitsIO(const char *path_prefix, int simulator)
    :
    FitsIO(path_prefix, 0, "VEGAS", simulator),
    openFlag(0),
    nrows(0),
    dmjd(0),
    scanLength(),
    stopTime(),
    integration_time(0),
    fits_data(0),
    current_row(1),
    accumid_xor_mask(0x0)
{
    strcpy(theVEGASMode, "");
    for(int i = 0; i < NUMPORTS; ++i)
    {
        noiseTone[i] = new char[256];
    }
}

VegasFitsIO::~VegasFitsIO()
{
    for(int i = 0; i < NUMPORTS; ++i)
    {
        delete [] noiseTone[i];
    }
    close();
}

void
VegasFitsIO::copyStatusMemory(const char *status_memory)
{
    size_t key_start;

    memcpy(status_buffer, status_memory, sizeof(status_buffer));
    status_mem_keywords.clear();

    for (key_start = 0; key_start+80 < sizeof(status_buffer); key_start+= 80)
    {
        string line, keyword;
        size_t eq_idx, blk_idx, key_end;
        line = string(status_memory, key_start, key_start+80);
        eq_idx = line.find_first_of('=');
        blk_idx = line.find_first_of(' ');
        key_end = blk_idx < eq_idx ? blk_idx : eq_idx;
        keyword = string(line, 0, key_end);

        dbprintf("key: %s\n", keyword.c_str());
        if (keyword == "END")
            break;

        status_mem_keywords.push_back(keyword);
    }
}

bool
VegasFitsIO::readPrimaryHeaderKeywords()
{
    char value[80];
    int32_t ival;
    double dval;

    if (hgets(status_buffer, "OBJECT", sizeof(value), value) == 0)
    {
        sprintf(value, "unspecified");
    }
    set_source(value);
    if (hgets(status_buffer, "OBSID", sizeof(value), value) == 0)
    {
        sprintf(value, "unknown");
    }
    set_scanId(value);
    if (hgeti4(status_buffer, "SCAN", &ival) == 0)
    {
        ival=1;
    }
    set_scanNumber(ival);
    if (hgeti4(status_buffer, "NCHAN", &ival) == 0)
    {
        printf("NCHAN not set in status memory\n");
        ival=1024;
    }
    setNumberChannels(ival);
    if (hgets(status_buffer, "MODENUM", sizeof(value), value) == 0)
    {
        sprintf(value, "MODE1");
    }
    setMode(value);
    if (hgetr8(status_buffer, "BAS_BW", &dval) == 0)
    {
        dval=0.0;
    }
    setBaseBw((float)dval);
    if (hgets(status_buffer, "NOISESRC", sizeof(value), value) == 0)
    {
        sprintf(value, "OFF");
    }
    ival = strstr("OFF", value) ? SwitchingSignals::on : SwitchingSignals::off;
    setNoiseSource(ival);
    if (hgets(status_buffer, "BOFFILE", sizeof(value), value) == 0)
    {
        sprintf(value, "unspecified");
    }
    setBofFile(value);
    float fpga_clock_frequency;
    if (hgetr4(status_buffer, "FPGACLK", &fpga_clock_frequency) == 0)
    {
        fpga_clock_frequency = 180.0;
    }
    setFpgaClock(fpga_clock_frequency);
    double scanlen;
    if (hgetr8(status_buffer, "SCANLEN", &scanlen) == 0)
    {
        printf("Required keyword SCANLEN not present in status memory\n");
        scanlen=10.0;
    }
    setScanLength(scanlen);

    accumid_xor_mask = 0x0; // No polarity inversion (done in hpc code)

    return true;
}

bool
VegasFitsIO::readStateTableKeywords()
{
    int32_t nphases, calState[16], sigRefState[16];
    bool done=false, all_columns;
    double phase_start[16];
    double blanking[16];
    double switch_period;
    char name[16];


    for (nphases=0; !done && nphases<16;)
    {
        all_columns=true;
        sprintf(name, "_SBLK_%02d", nphases+1);
        if (hgetr8(status_buffer, name, &blanking[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        dbprintf("(%s) blanking[%d]=%f\n", name, nphases+1, blanking[nphases]);
        sprintf(name, "_SPHS_%02d", nphases+1);
        if (hgetr8(status_buffer, name, &phase_start[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        dbprintf("(%s) phase_start[%d]=%f\n", name, nphases+1, phase_start[nphases]);
        sprintf(name, "_SSRF_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &sigRefState[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        dbprintf("(%s) sigRefState[%d]=%d\n", name, nphases+1, sigRefState[nphases]);
        sprintf(name, "_SCAL_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &calState[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        dbprintf("(%s) calState[%d]=%d\n", name, nphases+1, calState[nphases]);
        if (all_columns == true)
            nphases++;
    }

    int32_t numphase_param;
    if (hgeti4(status_buffer, "NUMPHASE", &numphase_param) == 0)
    {
        vegas_error("VegasFitsIO::readStateTableKeywords",
                    "required keyword NUMPHASES not found");
        numphase_param = 0;
    }

    // If we don't find any keywords -- that is bad and we should probably tank.
    // For now we synthesize a signal phase (SIG, nocal) and continue on.
    if (nphases == 0 || numphase_param == 0)
    {
        vegas_error("VegasFitsIO::readStateTableKeywords",
                    "No switching states defined "
                    "defaulting to SIG/NOCAL\n"
                    "NOTICE: Data will not be properly written to disk!");
        return false;
    }
    if (hgetr8(status_buffer, "SWPERIOD", &switch_period) == 0)
    {
        vegas_error("VegasFitsIO::readStateTableKeywords",
                    "Required keyword SWPERIOD not found");
        return false;
    }
    // Use the lesser of NUMPHASE and the number of phase descriptions
    setNumberPhases( min(nphases,  numphase_param) );
    setBlanking(blanking);
    setCalState(calState);
    setPhaseStart(phase_start);
    setSigRefState(sigRefState);
    setSwitchPeriod(switch_period);

    return true;
}

bool
VegasFitsIO::readActStateTableKeywords()
{
    int32_t nphases;
    bool done=false, all_columns;
    int32_t isigref1[16];
    int32_t isigref2[16];
    int32_t ical[16];
    int32_t esigref1[16];
    int32_t esigref2[16];
    int32_t ecal[16];
    char name[16];


    for (nphases=0; !done && nphases<16;)
    {
        all_columns=true;
        sprintf(name, "_AISA_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &isigref1[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        sprintf(name, "_AISB_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &isigref2[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        sprintf(name, "_AICL_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &ical[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        sprintf(name, "_AESA_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &esigref1[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        sprintf(name, "_AESB_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &esigref2[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        sprintf(name, "_AECL_%02d", nphases+1);
        if (hgeti4(status_buffer, name, &ecal[nphases]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        if (all_columns == true)
            nphases++;
    }
    if (nphases == 0)
    {
        return false;
    }

    setEsigref1(esigref1);
    setEsigref2(esigref2);
    setEcal(ecal);
    setIsigref1(isigref1);
    setIsigref2(isigref2);
    setIcal(ical);

    return true;
}

#define NUM_PORTS 2
#define NOISENAMELEN 8

/// Note this method is rather GBT specific, so the keywords are not required
/// to be in status memory and in that case defaults will be used to fill-in
/// the table.
bool
VegasFitsIO::readPortTableKeywords()
{
    int32_t nrows;
    bool all_columns;
    float meas_power[NUM_PORTS];
    char name[16];
    char tone_noise[NUM_PORTS][NOISENAMELEN];

    // BANK name 'BANKNAM' is read in the ::open() method.
    for (nrows=0; nrows<2; ++nrows)
    {
        all_columns=true;
        sprintf(name, "_PPWR_%02d", nrows+1);
        if (hgetr4(status_buffer, name, &meas_power[nrows]) == 0)
        {
            meas_power[nrows] = 0.0;
        }
        sprintf(name, "_PTNS_%02d", nrows+1);
        if (hgets(status_buffer, name, NOISENAMELEN, &tone_noise[nrows][0]) == 0)
        {
            sprintf(&tone_noise[nrows][0], "TONE");
        }
    }
    for (int32_t i=0; i<2; ++i)
    {
        setMeasuredPower(meas_power[i], i);
        setNoiseTone(strncasecmp("TONE", tone_noise[i], 4) == 0 ?
                     SwitchingSignals::tone : SwitchingSignals::noise,
                     i);
    }
    return true;
}

bool
VegasFitsIO::readSamplerTableKeywords()
{
    int32_t nrows;
    bool done=false, all_columns;
    char name[16];
    double crval1[16];
    double chandelta[16];
    double freq_res[16];
    double crpix;
    char polar_name[32];


    for (nrows=0; !done && nrows<16;)
    {
        all_columns=true;
#if 0
        // BANK_A
        sprintf(name, "_MBKA_%02d", nrows+1);
        if (hgets(status_buffer, name, 16, &bank_a[nrows][0]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        // PORT_A
        sprintf(name, "_MPTA_%02d", nrows+1);
        if (hgeti4(status_buffer, name, &port_a[nrows]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        // BANK_B
        sprintf(name, "_MBKB_%02d", nrows+1);
        if (hgets(status_buffer, name, 16, &bank_b[nrows][0]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        // PORT_B
        sprintf(name, "_MPTB_%02d", nrows+1);
        if (hgeti4(status_buffer, name, &port_b[nrows]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        // DATATYPE
        sprintf(name, "_MDTP_%02d", nrows+1);
        if (hgets(status_buffer, name, 16, &datatype[nrows][0]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
#endif
        // SUBBAND
        // subband[nrows] = nrows;
        /*
        sprintf(name, "_MSBD_%02d", nrows+1);
        if (hgeti4(status_buffer, name, &subband[nrows]) == 0)
        {
            done=1;
            all_columns=false;
            continue;
        }
        */
        // CRVAL1
        sprintf(name, "_MCR1_%02d", nrows+1);
        if (hgetr8(status_buffer, name, &crval1[nrows]) == 0)
        {
            done=1;
            // dbprintf("%s not found\n", name);
            all_columns=false;
            continue;
        }
        // CDELT1
        sprintf(name, "_MCDL_%02d", nrows+1);
        if (hgetr8(status_buffer, name, &chandelta[nrows]) == 0)
        {
            done=1;
            // dbprintf("%s not found\n", name);
            all_columns=false;
            continue;
        }
        // FREQRES
        sprintf(name, "_MFQR_%02d", nrows+1);
        if (hgetr8(status_buffer, name, &freq_res[nrows]) == 0)
        {
            done=1;
            // dbprintf("%s not found\n", name);
            all_columns=false;
            continue;
        }

        if (all_columns == true)
            nrows++;
    }
    if (nrows == 0)
    {
        dbprintf("No complete rows found\n");
        return false;
    }
    if (hgets(status_buffer, "POLARIZE", sizeof(polar_name), polar_name) == 0)
    {
        dbprintf("%s not found\n", "POLARIZE");
        return false;
    }
    if (hgetr8(status_buffer, "CRPIX1", &crpix) == 0)
    {
        dbprintf("%s not found\n", "CRPIX1");
        return false;
    }
    setPolarization(polar_name);
    // Logic lifted from manager implementation
    if (strcasecmp(polar_name, "CROSS")==0)
    {
        setNumberStokes(4);
    }
    else if (strcasecmp(polar_name, "SELF1")==0)
    {
        setNumberStokes(1);
    }
    else if (strcasecmp(polar_name, "SELF2")==0)
    {
        setNumberStokes(1);
    }
    else if (strcasecmp(polar_name, "SELF")==0)
    {
        setNumberStokes(2);
    }
    int32_t nsbbands;
    if (hgeti4(status_buffer, "NSUBBAND", &nsbbands) == 0)
    {
        nsbbands=1;
    }

    dbprintf("polar=%s\n", polar_name);
    setPolarization(polar_name);

    setReferenceChannel(crpix);
    setChannelFreqResolution(freq_res);
    setNumberSubBands(nsbbands);
    setChannelCenterFreq(crval1);
    setChannelFreqIncrement(chandelta);

    return true;
}

int VegasFitsIO::open()//const TimeStamp &ts)
{
    char rootpath[256];
    char value[80];
    MutexLock l(lock_mutex);
    // Only write files when scanning
    if(openFlag)
        close();

    scan_time_clock = 0.0;
    scan_is_complete = false;

    readPrimaryHeaderKeywords();

    int32_t next_hdu = 2;

    // If keywords exsist for tables, then write the table, otherwise ignore it
    if (readPortTableKeywords())
    {
        port_hdu = next_hdu++;
    }
    if (readStateTableKeywords())
    {
        state_hdu = next_hdu++;
    }
    if (readSamplerTableKeywords())
    {
        sampler_hdu = next_hdu++;
    }
    if (readActStateTableKeywords())
    {
        actstate_hdu = next_hdu++;
    }

    // reset fpga time counter mirror to 0
    _time_counter.clear();

    // Allocate buffers
    int n = numberPhases * numberSubBands * numberStokes;
    integration_time = new float[n];
    memset(integration_time, 0, n*sizeof(float));
    n *= numberChannels;
    dbprintf("Allocating %d bytes for fits_data\n", n);
    fits_data = new float[n];
    memset(fits_data, 0, n*sizeof(float));

    current_row = 1;
    //startTime = ts;
    if (hgets(status_buffer, "DATADIR", sizeof(rootpath), rootpath) == 0)
    {
        sprintf(rootpath, ".");
    }
    setRootDirectory(rootpath);
    if (hgets(status_buffer, "PROJID", sizeof(value), value) == 0)
    {
        sprintf(value, "JUNK");
    }
    set_projectId(value);
    // create directory path
    char *namePtr = createDirectoryPath(path,pathlength,3,
                                        rootDirectory,
                                        projectId,
                                        "VEGAS");
    // create directory path and check accessibility
    int retval = mkdirp(path,0775);
    // error?
    if(retval == -1 && errno != EEXIST)
    {
        perror(path);
        exit(2);
    }
    // TBF: have to figure out how to fake the status memory buffer for tests
    /*
    if (hgets(status_buffer, "BANKNAM", sizeof(value), value) == 0)
    {
        sprintf(value, "A");
    }
    string bnkstr(value);
    size_t p = bnkstr.find_last_not_of(' ');
    setBankName(bnkstr[p]);
    */

    char *suffix = setFilename(namePtr, startTime);
    strcpy(suffix, theBank);
    strcat(suffix, ".fits");
    suffix += strlen(theBank);

    // fresh start
    setStatus(0);

    // Does this file exist?
    if(access(path,F_OK) == 0)
    {
        cerr << path << " already exists, using " ;
        sprintf(suffix,"_%ld.fits",(long)getpid());
        cerr << path << endl ;
    }

    sprintf(theFilePath, "./%s/%s/%s",
            projectId,
            "VEGAS",
            namePtr);

    printf("Opening file: %s\n", theFilePath);

    // Open the file
    l.lock();
    create_file(path);

    nrows = 1;

    // Always create the primary, with defaults if necessary
    createPrimaryHDU();

    if (port_hdu)
    {
        createPortTable();
    }
    if (state_hdu)
    {
        createStateTable();
    }
    if (sampler_hdu)
    {
        createSamplerTable();
    }
    if (actstate_hdu)
    {
        createActStateTable();
    }

    // Always create the data table
    data_hdu = next_hdu++;
    createDataTable();

    if (numberChannels==0 || numberSubBands==0 || numberStokes==0)
    {
        vegas_error("VegasFitsIO::open",
                    "One of number channels, number subbands or number stokes is zero\n"
                    "NOTICE: Cannot continue!");
        pthread_exit(0);
    }
    openFlag = 1;
    if (getStatus())
    {
        print_all_error_messages("Error opening file: ");
    }

    return getStatus();
}

int VegasFitsIO::close()
{
    MutexLock l(lock_mutex);
    if(openFlag != 0)
    {
        printf("VegasFitsIO::close\n");
        l.lock();
        FitsIO::close();
        setStatus(0);
        openFlag = 0;
        if(fits_data)
        {
            delete [] fits_data;
            fits_data = 0;
        }
        if(integration_time)
        {
            delete [] integration_time;
            integration_time = 0;
        }
        l.unlock();
    }
    return getStatus();
}

void VegasFitsIO::setScanLength(const TimeStamp &len)
{
    scanLength = len;
    stopTime = startTime + scanLength;
    generate_FITS_date_time_string(startTime, theStartTimeStr);
}

void VegasFitsIO::setMode(const char *mode)
{
    if(mode != NULL)
    {
        strcpy(theVEGASMode, mode);
    }
}

void VegasFitsIO::setBankName(const char bank)
{
    theBank[0] = bank;
    theBank[1] = '\0';
}

void VegasFitsIO::setNumberChannels(int numchannels)
{
    numberChannels = numchannels;
}

void VegasFitsIO::setSelfTestMode(int selfTestMode)
{
    selfTest = selfTestMode;
}

void VegasFitsIO::setBaseBw(float base_bw)
{
    theBaseBw = base_bw;
}

void VegasFitsIO::setNoiseSource(int noise_source)
{
    if(noise_source == SwitchingSignals::on)
    {
        strcpy(theNoiseSource, "ON");
    }
    else
    {
        strcpy(theNoiseSource, "OFF");
    }
}

void VegasFitsIO::setMeasuredPower(float mp, int index)
{
    if((index >= 0) && (index <= 1))
    {
        measuredPower[index] = mp;
    }
}

void VegasFitsIO::setNoiseTone(int nt, int index)
{
    if((index >= 0) && (index <= 1))
    {
        if(nt == SwitchingSignals::noise)
        {
            strcpy(noiseTone[index], "NOISE");
        }
        else
        {
            strcpy(noiseTone[index], "TONE");
        }
    }
}

void VegasFitsIO::setPolarization(const char *pol)
{
    if(pol != NULL)
    {
        strcpy(polarization, pol);
    }
}

void VegasFitsIO::setNumberStokes(int stokes)
{
    numberStokes = stokes;
}

void VegasFitsIO::setNumberSubBands(int subbands)
{
    if((subbands > 0) && (subbands <= 8))
    {
        numberSubBands = subbands;
    }
}

void VegasFitsIO::setReferenceChannel(float refchan)
{
    referenceChannel = refchan;
}

void VegasFitsIO::setChannelCenterFreq(const double *values)
{
    memcpy(crval1, values, sizeof(double)*MAXSUBBANDS);
}

void VegasFitsIO::setChannelFreqIncrement(const double *values)
{
    memcpy(cdelt1, values, sizeof(double)*MAXSUBBANDS);
}

void VegasFitsIO::setChannelFreqResolution(const double *values)
{
    memcpy(freqres, values, sizeof(double)*MAXSUBBANDS);
}

void VegasFitsIO::setBlanking(const double *values)
{
    memcpy(blanking, values, sizeof(double)*numberPhases);
}

void VegasFitsIO::setCalState(const int *values)
{
    for(int i = 0; i < numberPhases; ++i)
    {
        calState[i] = values[i] ? SwitchingSignals::Noise : SwitchingSignals::NoNoise;
    }
}

void VegasFitsIO::setPhaseStart(const double *values)
{
    memcpy(phaseStart, values, sizeof(double)*numberPhases);
}

void VegasFitsIO::setSigRefState(const int *values)
{
    for(int i = 0; i < numberPhases; ++i)
    {
        sigRefState[i] = values[i] ? SwitchingSignals::Ref : SwitchingSignals::Sig;
    }
}

void VegasFitsIO::setSwitchPeriod(double switch_period)
{
    switchPeriod = switch_period;
}

// ACT_STATE Table methods
void VegasFitsIO::setEcal(const int *values)
{
    memcpy(ecal, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setEsigref1(const int *values)
{
    memcpy(esigref1, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setEsigref2(const int *values)
{
    memcpy(esigref2, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setIcal(const int *values)
{
    memcpy(ical, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setIsigref1(const int *values)
{
    memcpy(isigref1, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setIsigref2(const int *values)
{
    memcpy(isigref2, values, sizeof(int)*numberPhases);
}

void VegasFitsIO::setNumberPhases(int num_phases)
{
    if((num_phases > 0) && (num_phases <= MAXPHASES))
    {
        numberPhases = num_phases;
        sttspec.resize(numberPhases, 0);
        stpspec.resize(numberPhases, 0);
        accumid.resize(numberPhases, 0);
    }
}

void VegasFitsIO::setSwitchingSource(int source)
{
    if((source == SwitchingSignals::internal) ||
       (source == SwitchingSignals::external))
    {
        switchingSource = source;
    }
}

// DATA Table methods
void VegasFitsIO::setFpgaClock(float fpga_clock)
{
    if(fpga_clock > 0)
    {
        fpgaClock = fpga_clock;
    }
    if (fpga_clock < 50000)
    {
        fpgaClock = fpga_clock * 1E6; // in case specified in MHz, convert to Hz
    }
}

void VegasFitsIO::setRequestedIntegrationTime(float exp)
{
    if(exp > 0)
    {
        requestedIntegrationTime = exp;
    }
}

void VegasFitsIO::setSwPerInt(int sw_per_int)
{
    theSwPerInt = sw_per_int;
}

void VegasFitsIO::setStatusMem(std::map<std::string, std::string> &status)
{
    _status_mem = status;

    // these are not needed or already present in primary HDU:
    _status_mem.erase("DATADIR");
    _status_mem.erase("NCHAN");
    _status_mem.erase("OBSERVER");
    _status_mem.erase("PROJID");
    _status_mem.erase("SRC_NAME");
}

void VegasFitsIO::setBofFile(const char *bof_file)
{
    _bof_file = bof_file;
}

void VegasFitsIO::createPrimaryHDU()
{
    createBasePrimaryHDU();
    update_key_str((char *)"BANK",
                   theBank,
                   (char *)"spectrometer identifier");
    update_key_lng((char *)"NCHAN",
                   numberChannels,
                   (char *)"number of channels in each spectrum");
    update_key_str((char *)"MODE",
                   theVEGASMode,
                   (char *)"VEGAS mode");
    update_key_lng((char *)"SELFTEST",
                   selfTest,
                   (char *)"Is VEGAS in self-test mode?");
    update_key_str((char *)"FITSVER",
                   (char *)FITS_VERSION,
                   (char *)"FITS definition version for this device");
    update_key_flt((char *)"BASE_BW",
                   theBaseBw,
                   1,
                   (char *)"Base band bandwidth in MHz");
    update_key_str((char *)"NOISESRC",
                   theNoiseSource,
                   (char *)"Noise source, ON or OFF");
    update_key_str((char *)"BOFFILE",
                   (char *)_bof_file.c_str(),
                   NULL);

    write_comment((char *)"***");
    write_comment((char *)"The following are VEGAS status shared memory keyword/value pairs");
    write_comment((char *)"***");

    for (vector<string>::iterator i = status_mem_keywords.begin(); i != status_mem_keywords.end(); ++i)
    {
        char value[80];
        if (hgets(status_buffer, i->c_str(), sizeof(value), value))
        {
            update_key_str((char *)i->c_str(), value, NULL);
        }
    }

    flush();
}

void VegasFitsIO::createPortTable()
{
    char const *ttypeMode[] = {"MEASPWR", "T_N_SW"};
    char const *tformMode[] = {"1E", "5A"};
    char const *tunitMode[] = {"dBm", ""};
    int ports[] = {1,2};
    char bank_names[NUMPORTS];

    for(int index = 0; index < NUMPORTS; index++)
    {
        bank_names[index] = theBank[0];
    }

    set_number_ports(NUMPORTS);
    set_bank(bank_names, NUMPORTS);
    set_port(ports, NUMPORTS);

    createBasePortTable(port_hdu, 2, (char **)ttypeMode, (char **)tformMode, (char **)tunitMode);

    int row = 1;
    for(int index = 0; index < NUMPORTS; index++)
    {
        write_col_flt(3,
                      row,
                      1,
                      1,
                      &measuredPower[index]);
        write_col_str(4,
                      row,
                      1,
                      1,
                      &noiseTone[index]);
        row++;
    }

    flush();
}

void VegasFitsIO::createStateTable()
{
    // set_blanking, set_phase_start, set_sig_ref, set_cal must be
    // called before calling createStateTable
    set_blanking(blanking, numberPhases);
    set_phase_start(phaseStart, numberPhases);
    set_sig_ref_state(sigRefState, numberPhases);
    set_cal_state(calState, numberPhases);
    set_switch_period(switchPeriod);
    set_number_phases(numberPhases);
    createBaseStateTable(state_hdu);
    flush();
}

void VegasFitsIO::createSamplerTable()
{
    //A -> character string
    //I -> signed 16-bit integer
    //D -> 64-bit floating point
    //J -> signed 32-bit integer

    char const *ttypes[] = {"BANK_A", "PORT_A", "BANK_B","PORT_B",
                            "DATATYPE", "SUBBAND", "CRVAL1", "CDELT1", "FREQRES"};
    char const *tforms[] = {"1A","1I","1A","1I","4A","1I","1D","1D","1D"};
    char const *tunits[] = {"INDEX", "INDEX", "INDEX", "INDEX",
                            "", "INDEX", "Hz", "Hz", "Hz"};

    struct PORTDATA
    {
        int PortA;
        int PortB;
        char DataType[5];
    };

    PORTDATA port_data[4] = {
        {1, 1, "REAL"},
        {2, 2, "REAL"},
        {1, 2, "REAL"},
        {1, 2, "IMAG"},
    };

    create_binary_tbl(0,        // nrows - initial number of empty rows.
                      9,        // tfields - # of columns (max=999).
                      (char **)ttypes,  // column name(s)
                      (char **)tforms,  // column datatype(s)
                      (char **)tunits,  // column unit(s)
                      (char *)"SAMPLER" // table name
        );

    movabs_hdu(sampler_hdu,0);

    char* bankNamePtr[1] = {theBank};
    int nrow = 1;

    for(int subband = 0; subband < numberSubBands; subband++)
    {
        for(int pol_index = 1; pol_index <= numberStokes; pol_index++)
        {
            int index;

            if (numberStokes == 1 && strcmp(polarization, "SELF2") == 0)
            {
                index = 1;
            }
            else
            {
                index = pol_index - 1;
            }

            write_col_str(1,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          bankNamePtr
                );

            write_col_int(2,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(port_data[index].PortA)
                );

            write_col_str(3,            //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          bankNamePtr
                );

            write_col_int(4,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(port_data[index].PortB)
                );

            char* datatypePtr[1] = {port_data[index].DataType};
            write_col_str(5,            //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          datatypePtr
                );

            write_col_int(6,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(subband)
                );

            write_col_dbl(7,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(crval1[subband])
                );

            write_col_dbl(8,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(cdelt1[subband])
                );

            write_col_dbl(9,           //column
                          nrow,        //firstrow
                          (long int)1, //firstelem
                          (long int)1, //nelements
                          &(freqres[subband])
                );
            nrow++;
        }
    }

    update_key_flt((char *)"CRPIX1",
                   referenceChannel,
                   4,
                   (char *)"Reference Channel");

    update_key_str((char *)"POLARIZE",
                   polarization,
                   (char *)"Which data products are recorded in this file");
    flush();
}

void VegasFitsIO::createActStateTable()
{
    //J -> signed 32-bit integer
    char const *ttypes[] = {"ISIGREF1", "ISIGREF2", "ICAL","ESIGREF1", "ESIGREF2", "ECAL"};
    char const *tforms[] = {"1J","1J","1J","1J","1J","1J"};
    char const *tunits[] = {"T/F","T/F","T/F","T/F","T/F","T/F"};

    create_binary_tbl(0,            // nrows - initial number of empty rows.
                      6,            // tfields - # of columns (max=999).
                      (char **)ttypes,      // column name(s)
                      (char **)tforms,      // column datatype(s)
                      (char **)tunits,      // column unit(s)
                      (char *)"ACT_STATE"   // table name
        );

    movabs_hdu(actstate_hdu,0);

    int zeroes[MAXPHASES] = {0};
    if(switchingSource == SwitchingSignals::internal)
    {
        write_col_int(1, 1, 1, numberPhases, isigref1);
        write_col_int(2, 1, 1, numberPhases, isigref2);
        write_col_int(3, 1, 1, numberPhases, ical);
        write_col_int(4, 1, 1, numberPhases, zeroes);
        write_col_int(5, 1, 1, numberPhases, zeroes);
        write_col_int(6, 1, 1, numberPhases, zeroes);
    }
    else
    {
        write_col_int(1, 1, 1, numberPhases, zeroes);
        write_col_int(2, 1, 1, numberPhases, zeroes);
        write_col_int(3, 1, 1, numberPhases, zeroes);
        write_col_int(4, 1, 1, numberPhases, esigref1);
        write_col_int(5, 1, 1, numberPhases, esigref2);
        write_col_int(6, 1, 1, numberPhases, ecal);
    }

    flush();
}

void VegasFitsIO::createDataTable()
{
/*
    const int DATA_HDU = data_hdu;
    const int DATA_COLS = 8;
    char const *ttypeLags[] = {"INTEGRAT", "DATA", "UTCDELTA", "INTEGNUM",
                               "ACCUMID", "STTSPEC", "STPSPEC", "TIME_CTR"};
    char const *tformLags[] = {0, 0, "1D", "1J", "1J", "1J", "1J", "1K"};
    char const *tunitLags[] = {"sec", "COUNTS", "s", "id", "swsigbit", "SPCOUNT", "SPCOUNT", "FPGACLKS"};
    char integrat_tform[32];
    char data_tform[32];
    char accumid_tform[32];
    char stpspec_tform[32];
    char sttspec_tform[32];

    int num_integrations = numberSubBands * numberStokes * numberPhases;
    int num_data_points = numberChannels * num_integrations;
    snprintf(integrat_tform, 32, "%dE", num_integrations);
    snprintf(data_tform, 32, "%dE", num_data_points);
    snprintf(accumid_tform, 32, "%dJ", numberPhases);
    snprintf(sttspec_tform, 32, "%dJ", numberPhases);
    snprintf(stpspec_tform, 32, "%dJ", numberPhases);
    tformLags[0] = integrat_tform;
    tformLags[1] = data_tform;
    tformLags[4] = accumid_tform;
    tformLags[5] = sttspec_tform;
    tformLags[6] = stpspec_tform;
*/

    // #define NUM_ANTENNAS 40
    // The bin size is the number of elements in the lower trianglular
    //   portion of the covariance matrix
    //   (41 * 20) gives us the number of complex pair elements
    // #define BIN_SIZE (41 * 20)
    // #define BIN_SIZE 4
    // This is the number of frequency channels that we will be correlating
    //   It will be either 5, 50, or 160, and probably should always be a macro
    //   For the purposes of this simulator we don't care about the input to the correlator
    //   except that the number of input channels will indicate the number of output channels
    //   That is, the total number of complex pairs we will be writing to shared memory
    //   is given as: BIN_SIZE * NUM_CHANNELS
    // #define NUM_CHANNELS 5
    // #define TOTAL_DATA_SIZE (BIN_SIZE * NUM_CHANNELS * 2)
    // TOTAL_DATA_SIZE = 41 * 20 * 5 * 2 = 8200

    char data_form[10];
    sprintf(data_form, "%dC", BIN_SIZE * NUM_CHANNELS);
    //debug
    fprintf(stderr, "data_form: %s\n", data_form);

    const int DATA_HDU = data_hdu;
    const int DATA_COLS = 2;
    char const *ttypeLags[] = {"MCNT", "DATA"};
    char const *tformLags[] = {"1J", data_form};
    char const *tunitLags[] = {" ", " "};

    //                  HDU#, addtnl cols, ttypeState, tformState, tunitState
    createBaseDataTable(DATA_HDU, DATA_COLS, (char **)ttypeLags, (char **)tformLags, (char **)tunitLags);

    /*
    long intShape[2];
    intShape[0] = numberSubBands * numberStokes;
    intShape[1] = numberPhases;
    long datShape[3];
    datShape[0] = numberChannels;
    datShape[1] = intShape[0];
    datShape[2] = intShape[1];

    int column_number = get_base_number_data_columns() + 1;
    write_tdim(column_number++, 2, intShape);
    update_key_str((char *)"TDESC2", (char *)"SAMPLER,ACT_STATE", (char *)"definition of axes");

    write_tdim(column_number++, 3, datShape);
    update_key_str((char *)"TDESC3", (char *)"CHAN,SAMPLER,ACT_STATE", (char *)"definition of axes");
    long startTime_MJD = static_cast<long>(startTime);
    double startTime_Sec = (startTime-floor(startTime))*86400.0;

    update_key_lng((char *)"SWPERINT", theSwPerInt,
                   (char *)"Number of switching periods per integration");
    update_key_dbl((char *)"UTCSTART", startTime_Sec, 16,
                   (char *)"Actual start time in seconds since midnight");
    update_key_lng((char *)"UTDSTART", startTime_MJD,
                   (char *)"Actual start time in MJD");
    update_key_flt((char *)"DURATION", requestedIntegrationTime, 7,
                   (char *)"Length of one integration, seconds");
    */

    flush();
}

#define CAL_BIT    0x1
#define SIGREFBIT0 0x2
#define SIGREFBIT1 0x4
/// Buffer a portion of an integration to be written later
int
VegasFitsIO::bufferedWrite(DiskBufferChunk *chunk, bool new_integration)
{
    int accum;

    // If this a new integration, handle the integration start time counter
    if (new_integration)
    {
        integration_start_time = chunk->getIntegrationStart();
        integ_num = chunk->getIntegrationNumber();
    }

    // The accum (accumulation identifier) represents the state of the
    // switching signals at the time this sub-integration was taken, and
    // also which 'bin' the data was placed in.
    accum = chunk->getAccumulationId();
    // The correct order of bits is as follows, per 06/02/2013 12:51 PM
    // email from Mark Wagner:
    // 3: blank
    // 2: sr1
    // 1: sr0
    // 0: cal
    //
    // Use the accumid_xor_mask to invert the accumid to a sense
    // which matches the state table convention:
    int switch_state  = (accum ^ accumid_xor_mask);
    int accum_sig_state  = switch_state & SIGREFBIT0 ? false : true;
    int accum_cal_state  = switch_state & CAL_BIT    ? false : true;
    int state_offset;

    // Search the knowns switching states and match it against this accum
    for(state_offset = 0; state_offset < numberPhases; ++state_offset)
    {
        if ((calState[state_offset]    == accum_cal_state) &&
            (sigRefState[state_offset] == accum_sig_state))
        {
            sttspec[state_offset] = chunk->getSpectrumCountStart();
            stpspec[state_offset] = chunk->getSpectrumCountStop();
            accumid[state_offset] = accum; // Left un-inverted
            break;
        }
    }

    // If we couldn't find the right state, bail out with messages
    if(state_offset >= numberPhases)
    {
        std::cout << "Could not find state: "
                  << " accum_cal_state=" << accum_cal_state
                  << " accum_sig_state=" << accum_sig_state
                  << std::endl;
        int i;
        std::cout << "Known states are:" << endl;
        for (i=0; i<numberPhases; ++i)
        {
            std::cout << "\t cal="     << calState[i]
                      << "\t sig_ref=" << sigRefState[i]
                      << endl;
        }
        return 0;
    }
    // If this is the first phase (sometimes out of order in the buffer)
    // then record the integrations start time.
    if (state_offset == 0)
    {
        time_ctr_40bits = chunk->getIntegrationOffset();
        _time_counter.add_lsw(time_ctr_40bits);
        utcfrac = (double)_time_counter.get_offset() / (double)fpgaClock;
        // printf("new record t=%f %d\n", utcfrac, chunk->getIntegrationNumber());
    }

    // We found the state, now calculate the offset into the data for this accum
    int num_ints = numberSubBands * numberStokes;
    state_offset *= num_ints;

    // INTEGRAT column
    for(int i = state_offset; i < (num_ints+state_offset); ++i)
    {
        integration_time[i] = chunk->getExposure();
    }

    // DATA column
    state_offset *= numberChannels;

    for(int subband = 0; subband < numberSubBands; ++subband)
    {
        int fits_offset = state_offset + subband * numberStokes * numberChannels;
        int data_offset = subband * MAXSTOKES * numberChannels;
        if(strcmp(polarization, "SELF2") == 0)
        {
            data_offset += numberChannels;
        }
        float *data = chunk->getData();
        // copy the data into the integration buffer at the proper offset
        memcpy(&fits_data[fits_offset], &data[data_offset],
               numberStokes * numberChannels * sizeof(float));
    }

    return 1;
}

// We calculate all timestamps from the known start time and each mcnt ('packet counter')
double VegasFitsIO::calculateBlockTime(int mcnt, double startDMJD) {
    scan_time_clock = (double)((double)mcnt/(double)(PACKET_RATE)); 
    printf("elapsed secs: %f\n", scan_time_clock);
    return (startDMJD + (double)((double)scan_time_clock/(double)(24*60*60)));
}

/// Writes a full integration of data to a row in the FITS file.
int
VegasFitsIO::write(vegas_databuf_block_t *block)
{
    int column = 1;
    MutexLock l(lock_mutex);
    l.lock();

    // DMJD
    double dmjd = calculateBlockTime(block->header.mcnt, startTime);
    printf("dmjd: %f\n", dmjd);

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
                  &(block->header.mcnt));

    // DATA column
    // 41 * 20 * 5 = 4100
    // data will be 8200 elements long
    // TODO: This should not be hardcoded
    write_col_cmp(column++,
                  current_row,
                  1,
                  BIN_SIZE * NUM_CHANNELS,
                  block->data);
/*

    // DMJD column
    double dmjd = integration_start_time;
    write_col_dbl(column++,
                  current_row,
                  1,
                  1,
                  &dmjd);

    // INTEGRAT column
    write_col_flt(column++,
                  current_row,
                  1,
                  numberPhases * numberSubBands * numberStokes,
                  integration_time);

    // DATA column
    write_col_flt(column++,
                  current_row,
                  1,
                  numberPhases * numberSubBands * numberStokes * numberChannels,
                  fits_data);

    // DMJDFRAC column
    write_col_dbl(column++,
                  current_row,
                  1,
                  1,
                  &utcfrac);
    // cache a copy of the scan time
    scan_time_clock = utcfrac;

    // INTEGNUM column
    write_col_int(column++,
                  current_row,
                  1,
                  1,
                  &integ_num);

    // ACCUMID column
    write_col_int(column++,
                  current_row,
                  1,
                  numberPhases,
                  accumid.data());

    // STTSPEC column
    write_col_int(column++,
                  current_row,
                  1,
                  numberPhases,
                  sttspec.data());

    // STPSPEC column
    write_col_int(column++,
                  current_row,
                  1,
                  numberPhases,
                  stpspec.data());

    // TIME_CTR column
    write_col_lng(column++,
                  current_row,
                  1,
                  1,
                  (long int *)&time_ctr_40bits);

     */

    ++current_row;

    l.unlock();
    report_error(stderr, getStatus());
    return getStatus();
}

/// Check to see if the scan end time has been seen in the data
bool
VegasFitsIO::is_scan_complete()
{
    //bool has_ended = scan_time_clock > scanLength || scan_is_complete;
    bool has_ended = scan_time_clock >= (scanLength - INT_TIME) || scan_is_complete;
    if (has_ended)
    {
        printf("Scan ended clock=%f, scanlen=%f\n", scan_time_clock, scanLength);
    }
    return has_ended;
}

void VegasFitsIO::set_scan_complete()
{
    scan_is_complete = true;
}

