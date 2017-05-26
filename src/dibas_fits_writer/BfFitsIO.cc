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
#include "BfFitsIO.h"
// YGOR
#include "FitsIO.h"
// #include "util.h"
// STL
#include <errno.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <cassert>
#include <time.h>
#include "fitshead.h"
#include "vegas_error.h"

using namespace std;

static bool verbose = false;
#define dbprintf if (verbose) printf
// #define DEBUG

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

/// path_prefix The environment varable to use which contains
/// the directory prefix for the data files.
/// simulator A boolean value which sets the 'SIMULATE' header keyword.
BfFitsIO::BfFitsIO(const char *path_prefix, int simulator, int instance_id, int cov_mode)
    :
    FitsIO(path_prefix, 0, "BF", simulator),
    openFlag(0),
    nrows(0),
    dmjd(0),
    scanLength(),
    stopTime(),
    current_row(1),
    instance_id(instance_id)
  {
      if (cov_mode == 0)
      { 
        data_size = GPU_BIN_SIZE * NUM_CHANNELS; 
        sprintf(data_form, "%dC", data_size);
      }
      else if (cov_mode == 1)
      { 
        data_size = GPU_BIN_SIZE * NUM_CHANNELS_PAF;
        sprintf(data_form, "%dC", data_size); 
      }
      else if (cov_mode == 2)
      {
        data_size = GPU_BIN_SIZE * NUM_CHANNELS_FRB;
        sprintf(data_form, "%dC", data_size);
      }
      else
      {
        data_size = NUM_BEAMS * NUM_PULSAR_CHANNELS*4*100;
        sprintf(data_form, "%dE", data_size);
      }

      strcpy(theVEGASMode, "");
      setBankName(inst2bank(instance_id));
}

BfFitsIO::~BfFitsIO()
{
    close();
}

// brute force method of maping instance_ids to bank names
// TBF: kind of wierd since the top-level has the bank name,
// converts this to an instance id, then passes this on to 
// this FITS writer.
char
BfFitsIO::inst2bank(int instance_id) {
    assert (instance_id >= 0);
    assert (instance_id <= 9);
    char banks[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T'};
    return banks[instance_id];
}

void
BfFitsIO::copyStatusMemory(const char *status_memory)
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
BfFitsIO::readPrimaryHeaderKeywords()
{
    char value[80];
    int32_t ival;

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
        ival=5;
    }
    setNumberChannels(ival);
    if (hgets(status_buffer, "MODENUM", sizeof(value), value) == 0)
    {
        sprintf(value, "MODE1");
    }
    setMode(value);
    double scanlen;
    if (hgetr8(status_buffer, "SCANLEN", &scanlen) == 0)
    {
        printf("Required keyword SCANLEN not present in status memory\n");
        scanlen=10.0;
    }
    setScanLength(scanlen);

    return true;
}

// This opens a FITS file for writing
int BfFitsIO::open()
{
  char rootpath[256];
  char value[80];
  MutexLock l(lock_mutex);
  // Only write files when scanning
  if(openFlag)
  {
    close();
  }
  scan_time_clock = 0.0;
  scan_is_complete = false;

  readPrimaryHeaderKeywords();
  int32_t next_hdu = 2;
  current_row = 1;
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
    
  //setting scan length
  float keyval;
  if (hgetr4(status_buffer,"SCANLEN",&keyval) == 0 )
  {
    keyval=0;
  }
  set_scanLength(keyval);
  // Setting integation length
  if (hgetr4(status_buffer,"REQSTI",&keyval) == 0)
  {
    keyval =0;
  }
  set_intLength(keyval);
  integration_time = keyval;
  int xid;
  // Setting Xid
  if (hgeti4(status_buffer,"XID",&xid) == 0)
  {
    xid=0;
  }
  set_xid(xid);
   
  // create directory path
  char *namePtr = createDirectoryPath(path,pathlength,3,
                                        rootDirectory,
                                        projectId,
                                        "BF");
  // create directory path and check accessibility
  int retval = mkdirp(path,0775);
  // error?
  if(retval == -1 && errno != EEXIST)
  {
    perror(path);
    exit(2);
  }
  if (hgets(status_buffer, "BANKNAM", sizeof(value), value) == 0)
  {
    sprintf(value, "A");
  }
  string bnkstr(value);
  //size_t p = bnkstr.find_last_not_of(' ');
  // We disable this to allow us to set our own bank name directly
  //char *suffix = setFilename(namePtr, startTime);
  //strcpy(suffix, theBank);
  //strcat(suffix, ".fits");
  //suffix += strlen(theBank);
  
  //set up FITS filename format
  char byu_filename[24];
  hgets(status_buffer, "TSTAMP", 24, byu_filename);
  printf("FITS: Received TSTAMP = %s\n", byu_filename); 
  strcat(path, byu_filename);
  printf("FITS: Filename Stage 1: %s\n", path); 
  strcat(path, theBank);
  printf("FITS: Filename Stage 2: %s\n", path);
  strcat(path, ".fits");
  printf("FITS: Filename Stage 3: %s\n", path);

  
  // fresh start
  setStatus(0);

  // Does this file exist?
  if(access(path,F_OK) == 0)
  {
    cerr << path << " already exists, using " ;
    //sprintf(suffix,"_%ld.fits",(long)getpid());
    sprintf(path, "%s_%1d", path, (long)getpid());
    cerr << path << endl ;
  }

  sprintf(theFilePath, "%s/%s/%s/%s", 
            rootpath,
            projectId,
            "BF",
            namePtr);

  printf("Opening file: %s\n", theFilePath);

  // Open the file
  l.lock();
  printf("%s\n", path);
  if (getStatus())
  {
    print_all_error_messages("Error opening file: ");
  }

  create_file(path);

  nrows = 1;

  // Always create the primary, with defaults if necessary
  createPrimaryHDU();
  //if (getStatus())
  //{
  //  print_all_error_messages("Error opening file: ");
  //}

  data_hdu = next_hdu++;
  createDataTable();

  openFlag = 1;
  //if (getStatus())
  //{
  //  print_all_error_messages("Error opening file: ");
  //}

  return getStatus();
}

int BfFitsIO::close()
{
  MutexLock l(lock_mutex);
  if(openFlag != 0)
  {
    printf("BfFitsIO::close\n");
    l.lock();
    FitsIO::close();
    setStatus(0);
    openFlag = 0;
    l.unlock();
  }
  return getStatus();
}

void BfFitsIO::setScanLength(const TimeStamp &len)
{
  scanLength = len;
  stopTime = startTime + scanLength;
  generate_FITS_date_time_string(startTime, theStartTimeStr);
}

void BfFitsIO::setMode(const char *mode)
{
  if(mode != NULL)
  {
    strcpy(theVEGASMode, mode);
  }
}

void BfFitsIO::setBankName(const char bank)
{
    printf("Setting bank name to %c\n", bank);
    theBank[0] = bank;
    theBank[1] = '\0';
}

void BfFitsIO::setNumberChannels(int numchannels)
{
  numberChannels = numchannels;
}

void BfFitsIO::setSelfTestMode(int selfTestMode)
{
  selfTest = selfTestMode;
}

void BfFitsIO::setStatusMem(std::map<std::string, std::string> &status)
{
  _status_mem = status;

  // these are not needed or already present in primary HDU:
  _status_mem.erase("DATADIR");
  _status_mem.erase("NCHAN");
  _status_mem.erase("OBSERVER");
  _status_mem.erase("PROJID");
  _status_mem.erase("SRC_NAME");
}

void BfFitsIO::createPrimaryHDU()
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

void BfFitsIO::createDataTable()
{
  
  fprintf(stderr, "data_form: %s\n", data_form);

  const int DATA_HDU = data_hdu;
  const int DATA_COLS = 3;
  char const *ttypeLags[] = {"MCNT","GOOD_DATA","DATA"};
  char const *tformLags[] = {"1J","1L", data_form};
  char const *tunitLags[] = {" ", " ", " "};

    //                  HDU#, addtnl cols, ttypeState, tformState, tunitState
  createBaseDataTable(DATA_HDU, DATA_COLS, (char **)ttypeLags, (char **)tformLags, (char **)tunitLags);
  flush();
}

// We calculate all timestamps from the known start time and each mcnt ('packet counter')
double BfFitsIO::calculateBlockTime(int mcnt, double startDMJD) 
{
  scan_time_clock = (double)((double)mcnt)/((double)MCNT_RATE);
#ifdef DEBUG
  printf("elapsed secs: %f\n", scan_time_clock);
#endif
  return (startDMJD + (double)((double)scan_time_clock/(double)(24*60*60)));
}


/// Writes a full integration of data to a row in the FITS file.
int BfFitsIO::writeRow(int mcnt,int good_data, float *data, bool cmp)
{
  int column = 1;
  MutexLock l(lock_mutex);
  l.lock();

  // DMJD
  double dmjd = calculateBlockTime(mcnt, startTime);
  
  //DMJD
  //time_t numSec = time(NULL);
  //float elapsedDMJD = numSec / (float)86400;
  //double dmjd = 40587 + elapsedDMJD;  
   
//DMJD column
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

  write_col_int(column++,
                 current_row,
                  1,
                  1,
                 &good_data);

  clock_gettime(CLOCK_MONOTONIC, &data_w_start);
  // DATA column
  if (cmp){
      write_col_cmp(column++,
                  current_row,
                  1,
                  data_size, //FITS_BIN_SIZE * NUM_CHANNELS,
                  data);
   } 
   else
   {
       write_col_flt(column++,
 	           current_row,
                   1, 
                   data_size,
                   data);
   }
clock_gettime(CLOCK_MONOTONIC, &data_w_stop);
  ++current_row;
  l.unlock();
  report_error(stderr, getStatus());
  return getStatus();
}

// This checks to see if we have reached the desired scan time
bool BfFitsIO::is_scan_complete(int mcnt)
{
  float last_mcnt = scanLength*200*PACKET_RATE;
  //bool has_ended = scantime > scanLength || scan_is_complete;
  //bool has_ended = mcnt >= last_mcnt || mcnt >= last_mcnt-(200*PACKET_RATE) || scan_is_complete;
  bool has_ended = scan_is_complete;
#ifdef DEBUG
  printf("int time: %f\n", (float)N / (float)PACKET_RATE);
#endif
  if (has_ended)
  {
    printf("Scan ended clock=%f, scanlen=%f\n", scan_time_clock, scanLength);
  }
  return has_ended;
}

void BfFitsIO::set_scan_complete()
{
  scan_is_complete = true;
}

double BfFitsIO::timeval_2_mjd(timeval *tv)
{
  double dmjd = tv->tv_sec/86400 + MJD_1970_EPOCH;
  dmjd += (tv->tv_sec % 86400)/86400.0;
  return dmjd;
}

// python:
// d, mjd = math.modf(dmjd)
// return (86400 * (mjd - 40587)) + (86400 * d)
unsigned long BfFitsIO::dmjd_2_secs(double dmjd)
{
  unsigned long mjd = (unsigned long)dmjd;
  double d = dmjd - mjd;
  unsigned long secs = 86400 * (mjd - MJD_1970_EPOCH);
  double others = d * 86400.0;
  unsigned long total = secs + others;
  return total;
}

//function for writing HI data
int BfFitsIO::write_HI(int mcnt,int good_data, float *data) 
{
  writeRow(mcnt,good_data, data, true);
  return 1;
}

//function for writing PAF calibration data
int BfFitsIO::write_PAF(int mcnt,int good_data, float *data) 
{
  writeRow(mcnt,good_data, data, true);
  return 1;
}
//funciton for wrting FRB data
int BfFitsIO::write_FRB(int mcnt,int good_data, float *data) 
{
  writeRow(mcnt,good_data, data, true);
  return 1;
}

//function for writing Real-Time beamforming data
int BfFitsIO::write_RTBF(int mcnt,int good_data, float *data) {
  writeRow(mcnt,good_data, data, false);
  return 1;
}


