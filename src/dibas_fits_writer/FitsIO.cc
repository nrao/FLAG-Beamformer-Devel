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

#include "FitsIO.h"
#include "fitsio.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <new>
#include <ctime>


/// A data structure to store the CFITSIO library handle and the current status.
struct FitsIO::fitsio_data
{
    fitsfile *fptr;
    int status;
};

char const *datestring = DATEBLD_TARGET_STRING;

#define fptr fid->fptr
#define status fid->status

const char *FitsIO::fitsvers_string_prefix = "2.";

const char *getConfigValue(const char *deflt, const char *keyword)
{
    const char *p = getenv(keyword);
    if (p == 0)
        return deflt;
    else
        return p;
}

FitsIO::FitsIO(const char *path_env_variable,
               int fits_version_arg,
               const char *instrument_arg,
               int simulateFlag_arg
              ) :
        filenamePtr(0),
        origin(0),
        telescope(0),
        telescope_comment(0),
        telescope_version_keyword(0),
        simulateFlag(simulateFlag_arg),
        blanking(0),
        phase_start(0),
        sig_ref_state(0),
        cal_state(0),
        number_phases(0),
        switching_signals_master(0),
        switch_period(0.0),
        number_ports(0),
        bank(0),
        port(0),
        hduCount(0),
        optimalRows(1),
        startTime(0),
        scanNumber(0),
        fid(0)
{
    fid = new fitsio_data;
    fptr = 0;
    status = 0;

    // FitsIO initializes the Fits file structure
    rootDirectory = strdup(getConfigValue("/lustre/dibas", path_env_variable));

    dateObsString = new char[FLEN_VALUE];
    dateObsString[0] = '\0';

    telescope_version = new char[FLEN_VALUE];
    telescope_version[0] = '\0';

    fits_version = new char[FLEN_VALUE];
    fits_version[0] = '\0';

    instrument = new char[FLEN_VALUE];
    instrument[0] = '\0';

    source = new char[FLEN_VALUE];
    source[0] = '\0';

    projectId = new char[FLEN_VALUE];
    projectId[0] = '\0';

    scanId = new char[FLEN_VALUE];
    scanId[0] = '\0';

    path[0] = '\0';

    origin = strdup("GBTG"); //getConfigValue(0,"ORIGIN");

    telescope = strdup("SHAO 65meter"); // getConfigValue(0,"TELESCOPE");

    telescope_comment = strdup("SHAO "); // getConfigValue(0,"TELESCOPE_COMMENT");

    telescope_version_keyword = strdup("V0.1"); // getConfigValue(0,"VERSION_KEYWORD");

    define_keyword_value_telescope_version();

    sprintf(fits_version, "%s%d", fitsvers_string_prefix, fits_version_arg);

    strncpy(instrument,instrument_arg, FLEN_VALUE - 1);
    instrument[FLEN_VALUE - 1] = '\0';

    source[0] = '\0';

    projectId[0] = '\0';

    scanId[0] = '\0';
}

template <typename T>
T *duplicate(const T *source, int n)
{
    if (0 == source)
    {
        return 0;
    }

    T *result = new T[n];

    for (int i = 0; i < n; ++i)
    {
        result[i] = source[i];
    }

    return result;
}

char* safecopy(const char* source)
{
    char *result = 0;

    if (0 != source)
    {
        result = new char [strlen(source) + 1];
        strcpy(result, source);
    }

    return result;
}

FitsIO::FitsIO(const FitsIO& rhs)
{
    memcpy(this, &rhs, sizeof(FitsIO));

    fid = new fitsio_data;
    memcpy(fid, rhs.fid, sizeof(fitsio_data));

    dateObsString             = safecopy(rhs.dateObsString);
    telescope_version         = safecopy(rhs.telescope_version);
    fits_version              = safecopy(rhs.fits_version);
    instrument                = safecopy(rhs.instrument);
    rootDirectory             = safecopy(rhs.rootDirectory);
    filenamePtr               = safecopy(rhs.filenamePtr);
    origin                    = safecopy(rhs.origin);
    telescope                 = safecopy(rhs.telescope);
    telescope_comment         = safecopy(rhs.telescope_comment);
    telescope_version_keyword = safecopy(rhs.telescope_version_keyword);
    switching_signals_master  = safecopy(rhs.switching_signals_master);
    source                    = safecopy(rhs.source);
    projectId                 = safecopy(rhs.projectId);
    scanId                    = safecopy(rhs.scanId);

    blanking      = duplicate(rhs.blanking, number_phases);
    phase_start   = duplicate(rhs.phase_start, number_phases);
    sig_ref_state = duplicate(rhs.sig_ref_state, number_phases);
    cal_state     = duplicate(rhs.cal_state, number_phases);
 }

const FitsIO& FitsIO::operator=(const FitsIO& rhs)
{
    this->FitsIO::~FitsIO();
    new (this) FitsIO(rhs);
    return *this;
}

void FitsIO::define_keyword_value_telescope_version()
{
    strcpy(telescope_version, "UNKNOWN");
#if 0
    char *ygor_path = 0;
    char bin_path[256];
    char true_path[256];
    char *version_pointer;

    if ((ygor_path = getenv("YGOR_TELESCOPE")) == NULL)
    {
        perror("getenv");
        return;
    }
    strcpy(bin_path, ygor_path);
    strcat(bin_path, "/exec");
    memset(true_path, 0, sizeof(true_path));
    if (readlink(bin_path, true_path, sizeof(true_path)) < 1)
    {
        perror("readlink");
        return;
    }
    if ((version_pointer = strrchr(true_path, '/')) == NULL)
    {
        perror("strrchr");
        return;
    }
    *version_pointer = '\0';
    if ((version_pointer = strrchr(true_path, '/')) == NULL)
    {
        perror("strrchr");
        return;
    }
    version_pointer++;

    strncpy(telescope_version, version_pointer, FLEN_VALUE);
    telescope_version[FLEN_VALUE - 1] = '\0';
#endif
}

FitsIO::~FitsIO()
{
    delete [] dateObsString;
    delete [] telescope_version;
    delete [] fits_version;
    delete [] instrument;
    delete [] rootDirectory;
    delete [] origin;
    delete [] telescope;
    delete [] telescope_comment;
    delete [] telescope_version_keyword;
    delete [] blanking;
    delete [] phase_start;
    delete [] sig_ref_state;
    delete [] cal_state;
    delete [] switching_signals_master;
    delete [] source;
    delete [] projectId;
    delete [] scanId;
    delete [] bank;
    delete [] port;

    delete fid;
}

int FitsIO::close()
{
    if (fptr != 0)
    {
        fits_close_file(fptr,&status);
        return status;
    }

    return 0;
}

char  *FitsIO::setFilename(char *buf, TimeStamp &ts)
{   // setFileName() generates the standard GBT output name
    // the input string must be large to accomdate a long path name
    // a string pointer to the suffix of the path (.fits) is returned
    // The suffix points to a location within buf so will be invalid if
    // if "buf" is de-allocated

    generateDataTimeName(buf,ts);

    char *suffix = buf + strlen(buf);

    strcat(buf,".fits");

    // initialize time identifier
    generate_FITS_date_time_string(ts, dateObsString);

    return suffix;
}

char *FitsIO::createDirectoryPath(char *buf, size_t buflength, int args, ...)
{
    va_list ap;
    size_t n = 0;
    const char *cp;
    va_start(ap, args);

    // Copy the root
    cp = va_arg(ap,const char*);
    if ((n += strlen(cp)+1) > buflength)
    {
        va_end(ap);
        return 0;
    }
    strcpy(buf,cp);

    // Append the rest of the path
    for (int  i=1; i<args; i++)
    {
        cp = va_arg(ap,char*);
        if (cp != 0)
        {
            if ((n += strlen(cp)+1) > buflength)
            {
                va_end(ap);
                return 0;
            }
            strcat(buf,"/");
            strcat(buf,cp);
        }
    }
    strcat(buf,"/");
    va_end(ap);

    char *retval = buf + strlen(buf);

    // Remember where the filename starts if the buf is
    // the data member path
    if (buf == path)
        filenamePtr = retval;

    // Is there room for the filename?
    assert((n+24) < buflength);

    return retval;
}

void FitsIO::createPrimaryHDU()
{
    createBasePrimaryHDU();
}

void FitsIO::createBasePrimaryHDU()
{
    char keyname[10];
    char comment[30];
    createMinimumPrimaryHDU();

    update_key_date_obs();
    update_key_telescop();
    strcpy(keyname, "OBJECT");
    strcpy(comment, "Manager parameter source");
    fits_update_key_str(fptr,
                        keyname,
                        source,
                        comment,
                        &status);
    update_key_projid();
    strcpy(keyname, "OBSID");
    strcpy(comment, "Manager parameter scanId");
    fits_update_key_str(fptr,
                        keyname,
                        scanId,
                        comment,
                        &status);
    update_key_scan();
    flush();
}

void FitsIO::createPortTable()
{
    createBasePortTable(2);
}

void FitsIO::createStateTable()
{
    createBaseStateTable(3);
}

void FitsIO::createDataTable()
{
    createBaseDataTable(4);
}


void FitsIO::createMinimumPrimaryHDU()
{
    char keyname[10];
    char comment[64];
    // Initialize primary header
    fits_create_img(fptr, 8, 0, 0, &status);

    strcpy(keyname, "ORIGIN");
    strcpy(comment, " ");
    fits_update_key_str(fptr,
                        keyname,
                        origin,
                        comment,
                        &status);
    strcpy(keyname, "INSTRUME");
    strcpy(comment, "device or program of origin");
    fits_update_key_str(fptr,
                        keyname,
                        instrument,
                        comment,
                        &status);
    strcpy(comment, "telescope control software release");
    fits_update_key_str(fptr,
                        telescope_version_keyword,
                        telescope_version,
                        comment,
                        &status);
    strcpy(keyname, "FITSVER");
    strcpy(comment, "FITS definition version for this device");
    fits_update_key_str(fptr,
                        keyname,
                        fits_version,
                        comment,
                        &status);
    strcpy(keyname, "DATEBLD");
    strcpy(comment, "time program was linked");
    fits_update_key_str(fptr,
                        keyname,
                        // the following string should be replaced
                        // by the actual day and time, e.g.,
                        // "Thu Jun  7 20:15:13 GMT 2001"
                        // in the executable
                        (char *)datestring,
                        comment,
                        &status);
    strcpy(keyname, "SIMULATE");
    strcpy(comment, "Is the instrument in simulate mode?");
    fits_update_key_lng(fptr,
                        keyname,
                        simulateFlag,
                        comment,
                        &status);
}

void FitsIO::createBaseStateTable(int HDU_number,
                                  int additional_numberColumns,
                                  char **additional_ttypeState,
                                  char **additional_tformState,
                                  char **additional_tunitState)
{
    char name[10];
    char comment[64];

    // Create State Table and fill in its HDU information.
    int base_numberColumns = base_number_state_columns;
    char *base_ttypeState[] =
        {(char *)"BLANKTIM", (char *)"PHSESTRT", (char *)"SIGREF", (char *)"CAL"};
    char *base_tformState[] =
        {(char *)"1D", (char *)"1D", (char *)"1J", (char *)"1J"};
    char *base_tunitState[] =
        {(char *)"SECONDS", (char *)"NONE", (char *)"T/F", (char *)"T/F"};

    // Combine base and additional column descriptions
    int numberColumns = base_numberColumns + additional_numberColumns;
    char **ttypeState = new char * [numberColumns];
    char **tformState = new char * [numberColumns];
    char **tunitState = new char * [numberColumns];
    int i = 0;
    for (i = 0; i < base_numberColumns; ++i)
    {
        ttypeState[i] = base_ttypeState[i];
        tformState[i] = base_tformState[i];
        tunitState[i] = base_tunitState[i];
    }
    for (i = base_numberColumns; i < numberColumns; ++i)
    {
        ttypeState[i] = additional_ttypeState[i-base_numberColumns];
        tformState[i] = additional_tformState[i-base_numberColumns];
        tunitState[i] = additional_tunitState[i-base_numberColumns];
    }

    strcpy(name, "STATE");
    fits_create_tbl(fptr,
                    BINARY_TBL,
                    number_phases,
                    numberColumns,
                    ttypeState,
                    tformState,
                    tunitState,
                    name,
                    &status);

    delete [] ttypeState;
    delete [] tformState;
    delete [] tunitState;

    fits_movabs_hdu(fptr, HDU_number, 0, &status);

    fits_write_col_dbl(fptr, 1,1,1, number_phases, blanking, &status);
    fits_write_col_dbl(fptr, 2,1,1, number_phases, phase_start, &status);
    fits_write_col_int(fptr, 3,1,1, number_phases,
                       (int*)sig_ref_state, &status);
    fits_write_col_int(fptr, 4,1,1, number_phases, (int*)cal_state, &status);

    strcpy(name, "NUMPHASE");
    strcpy(comment, "Number of Phases if only Internal Switching Sig");
    fits_update_key_lng(fptr,
                        name,
                        number_phases,
                        comment,
                        &status);
    strcpy(name, "SWPERIOD");
    strcpy(comment, "Switching period");
    fits_update_key_dbl(fptr,
                        name,
                        switch_period,
                        3,
                        comment,
                        &status);
    strcpy(name, "MASTER");
    strcpy(comment, "Switching Signals Master");
    fits_update_key_str(fptr,
                        name,
                        switching_signals_master,
                        comment,
                        &status);
}

void FitsIO::createBasePortTable(int HDU_number,
                                 int additional_numberColumns,
                                 char **additional_ttypeState,
                                 char **additional_tformState,
                                 char **additional_tunitState)
{
    // Create Port Table and fill in its HDU information.
    int base_numberColumns = base_number_port_columns;
    char *base_ttypeState[] =
        {(char *)"BANK", (char *)"PORT"};
    char *base_tformState[] =
        {(char *)"1A", (char *)"1I"};
    char *base_tunitState[] =
        {(char *)"INDEX", (char *)"INDEX"};

    // Combine base and additional column descriptions
    int numberColumns = base_numberColumns + additional_numberColumns;
    char **ttypeState = new char * [numberColumns];
    char **tformState = new char * [numberColumns];
    char **tunitState = new char * [numberColumns];
    int i = 0;
    for (i = 0; i < base_numberColumns; ++i)
    {
        ttypeState[i] = base_ttypeState[i];
        tformState[i] = base_tformState[i];
        tunitState[i] = base_tunitState[i];
    }
    for (i = base_numberColumns; i < numberColumns; ++i)
    {
        ttypeState[i] = additional_ttypeState[i-base_numberColumns];
        tformState[i] = additional_tformState[i-base_numberColumns];
        tunitState[i] = additional_tunitState[i-base_numberColumns];
    }

    char extname[] = "PORT";
    fits_create_tbl(fptr,
                    BINARY_TBL,
                    number_ports,
                    numberColumns,
                    ttypeState,
                    tformState,
                    tunitState,
                    extname,
                    &status);

    delete [] ttypeState;
    delete [] tformState;
    delete [] tunitState;

    fits_movabs_hdu(fptr, HDU_number, 0, &status);

    fits_write_col_byt(fptr, 1,1,1, number_ports,
                       (unsigned char *)bank, &status);
    fits_write_col_int(fptr, 2,1,1, number_ports, port, &status);
}

void FitsIO::createBaseDataTable(int HDU_number,
                                 int additional_numberColumns,
                                 char **additional_ttypeState,
                                 char **additional_tformState,
                                 char **additional_tunitState)
{
    // Create Data Table and fill in its HDU information.
    int base_numberColumns = base_number_data_columns;
    char *base_ttypeState[] =
        {(char *)"DMJD"};
    char *base_tformState[] =
        {(char *)"1D"};
    char *base_tunitState[] =
        {(char *)"d"};

    // Combine base and additional column descriptions
    int numberColumns = base_numberColumns + additional_numberColumns;
    char **ttypeState = new char * [numberColumns];
    char **tformState = new char * [numberColumns];
    char **tunitState = new char * [numberColumns];
    int i = 0;
    for (i = 0; i < base_numberColumns; ++i)
    {
        ttypeState[i] = base_ttypeState[i];
        tformState[i] = base_tformState[i];
        tunitState[i] = base_tunitState[i];
    }
    for (i = base_numberColumns; i < numberColumns; ++i)
    {
        ttypeState[i] = additional_ttypeState[i-base_numberColumns];
        tformState[i] = additional_tformState[i-base_numberColumns];
        tunitState[i] = additional_tunitState[i-base_numberColumns];
    }

    char extname[] = "DATA";
    fits_create_tbl(fptr,
                    BINARY_TBL,
                    0,
                    numberColumns,
                    ttypeState,
                    tformState,
                    tunitState,
                    extname,
                    &status);

    delete [] ttypeState;
    delete [] tformState;
    delete [] tunitState;

    fits_movabs_hdu(fptr, HDU_number, 0, &status);
}

void FitsIO::set_switching_signals_master(
    const char *switching_signals_master_arg)
{
    delete [] switching_signals_master;
    switching_signals_master =
        new char[strlen(switching_signals_master_arg) + 1];
    strcpy(switching_signals_master,switching_signals_master_arg);
}

int FitsIO::print_all_error_messages(const char *where)
{
    char error_message[FLEN_ERRMSG];
    int count = 0;
    int number_of_messages = 1;

    while (number_of_messages != 0)
    {
        number_of_messages = fits_read_errmsg(error_message);

        if (error_message[0] != '\0')
        {
            fprintf(stderr, "Error in %s - %s\n", where, error_message);
            count++;
        }
    }
    return count;
}

int FitsIO::getStatus()
{
    return status;
}

void FitsIO::setStatus(int s)
{
    status = s;
}

void FitsIO::get_errstatus(char *errtext)
{
    fits_get_errstatus(status,errtext);
}

const char *FitsIO::getRootDirectory() const
{
    return rootDirectory;
}

void FitsIO::setRootDirectory(const char *dir)
{
    delete [] rootDirectory;
    rootDirectory = new char[strlen(dir)+1];
    strcpy(rootDirectory, dir);
}

void FitsIO::flush()
{
    if (fptr != 0)
    {
        fits_flush_file(fptr, &status);
    }
}

void FitsIO::setOptimalRows()
{
    fits_get_rowsize(fptr,&optimalRows,&status);
}

const char *FitsIO::getPath()
{
    return path;
}

int FitsIO::mkdirp(const char *path, mode_t mode)
{
    char mypath[1024];
    char newpath[1024];
    struct stat statbuf;
    char *plast = 0;
    char *ptr;
    memset(mypath, 0, sizeof(mypath));
    memset(newpath, 0, sizeof(newpath));
    strncpy(mypath, path, sizeof(mypath));
    ptr = strtok_r(mypath, "/", &plast);
    ptr = mypath;
    do
    {
        strcat(newpath, ptr);
        if(stat(newpath, &statbuf) == -1)
        {
            if(mkdir(newpath, mode) == -1)
                return(-1);
        }
        // printf("%s\n", newpath ? newpath : "(null)");
        strcat(newpath, "/");
    }
    while((ptr = strtok_r(0, "/", &plast)));
    return(0);
}

#define MJD_1970_EPOCH (40587)
char *FitsIO::generate_FITS_date_time_string(TimeStamp dmjd, char *buffer)
{
    int month, day, year, hours, minutes, seconds;
    
    struct tm caldattime;
    time_t cur_unix_time = static_cast<time_t>((dmjd - MJD_1970_EPOCH)*86400);

    gmtime_r(&cur_unix_time, &caldattime);
    month=caldattime.tm_mon + 1;
    day=caldattime.tm_mday;
    year=caldattime.tm_year+1900;
    hours=caldattime.tm_hour;
    minutes=caldattime.tm_min;
    seconds=caldattime.tm_sec;

    sprintf(buffer,
            "%04d-%02d-%02dT%02d:%02d:%02d",
            year, month, day, hours, minutes, seconds);
    return buffer;
}

char *FitsIO::generateDataTimeName(char *buffer, TimeStamp dmjd)
{
    int month, day, year, hours, minutes, seconds;
    
    struct tm caldattime;
    time_t cur_unix_time = static_cast<time_t>((dmjd - MJD_1970_EPOCH)*86400);

    gmtime_r(&cur_unix_time, &caldattime);
    month=caldattime.tm_mon + 1;
    day=caldattime.tm_mday;
    year=caldattime.tm_year+1900;
    hours=caldattime.tm_hour;
    minutes=caldattime.tm_min;
    seconds=caldattime.tm_sec;

    sprintf(buffer,
            "%04d_%02d_%02d_%02d:%02d:%02d",
            year, month, day, hours, minutes, seconds);
    return buffer;
}

void FitsIO::set_startTime(const TimeStamp &time)
{
    startTime = time;
    generate_FITS_date_time_string(startTime, dateObsString);
}

int FitsIO::get_base_number_state_columns()
{
    return base_number_state_columns;
}

int FitsIO::get_base_number_port_columns()
{
    return base_number_port_columns;
}

int FitsIO::get_base_number_data_columns()
{
    return base_number_data_columns;
}


void FitsIO::set_source(const char *source_arg)
{
    strncpy(source, source_arg, FLEN_VALUE);
}


void FitsIO::set_projectId(const char *projectId_arg)
{
    strncpy(projectId, projectId_arg, FLEN_VALUE);
}


void FitsIO::set_scanNumber(const int scanNumber_arg)
{
    scanNumber = static_cast<long>(scanNumber_arg);
}


void FitsIO::set_scanId(const char *scanId_arg)
{
    strncpy(scanId, scanId_arg, FLEN_VALUE);
}

void FitsIO::update_key_date_obs()
{
    char keyname[10];
    char comment[64];
    strcpy(keyname, "DATE-OBS");
    strcpy(comment, "Manager parameter startTime");
    fits_update_key_str(fptr, 
                        keyname,
                        dateObsString, 
                        comment,
                        &status);
    strcpy(keyname, "TIMESYS");
    strcpy(comment, "time scale specification for DATE-OBS");
    char value[] = "UTC";
    fits_update_key_str(fptr, 
                        keyname,
                        value,
                        comment,
                        &status);
}

void FitsIO::update_key_telescop()
{
    char keyname[] = "TELESCOP";
    fits_update_key_str(fptr, 
                        keyname,
                        telescope,
                        telescope_comment,
                        &status);
}

void FitsIO::update_key_projid()
{
    char keyname[] = "PROJID";
    char comment[] = "Manager parameter projectId";
    fits_update_key_str(fptr, 
                        keyname,
                        projectId, 
                        comment,
                        &status);
}

void FitsIO::update_key_scan()
{
    char keyname[] = "SCAN";
    char comment[] = "Manager parameter scanNumber";
    fits_update_key_lng(fptr, 
                        keyname,
                        scanNumber,
                        comment,
                        &status);
}

void FitsIO::set_blanking(const double *blanking_arg, int number)
{
    delete [] blanking;
    blanking = new double[number];
    for (int i=0; i<number; i++)
        blanking[i] = blanking_arg[i];
}

void FitsIO::set_phase_start(const double *phase_start_arg, int number)
{
    delete [] phase_start;
    phase_start = new double[number];
    for (int i=0; i<number; i++)
        phase_start[i] = phase_start_arg[i];
}

void FitsIO::set_sig_ref_state(
     const SwitchingSignals::SigRefState *sig_ref_state_arg, int number)
{
    delete [] sig_ref_state;
    sig_ref_state = new SwitchingSignals::SigRefState[number];
    for (int i=0; i<number; i++)
        sig_ref_state[i] = sig_ref_state_arg[i];
}

void FitsIO::set_cal_state(
     const SwitchingSignals::CalState *cal_state_arg, int number)
{
    delete [] cal_state;
    cal_state = new SwitchingSignals::CalState[number];
    for (int i=0; i<number; i++)
        cal_state[i] = cal_state_arg[i];
}

void FitsIO::set_number_phases(const int number_phases_arg)
{
     number_phases = number_phases_arg;
}

void FitsIO::set_switch_period(const double &switch_period_arg)
{
    switch_period = switch_period_arg;
}

void FitsIO::set_number_ports(const int number_ports_arg)
{
     number_ports = number_ports_arg;
}

void FitsIO::set_bank(const char *bank_arg, int number)
{
    delete [] bank;
    bank = new char[number];
    for (int i=0; i<number; i++)
        bank[i] = bank_arg[i];
}

void FitsIO::set_port(const int *port_arg, int number)
{
    delete [] port;
    port = new int[number];
    for (int i=0; i<number; i++)
        port[i] = port_arg[i];
}

int FitsIO::create_file(const char *name)
{
    return fits_create_file(&fptr, name, &status);
}

int FitsIO::insert_rows(long firstrow, long nrows)
{
    return fits_insert_rows(fptr, firstrow, nrows, &status);
}

int FitsIO::write_col_byt(int  colnum, long  firstrow, long  firstelem,
                          long  nelem, unsigned char *array)
{
    return fits_write_col_byt(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_dbl(int  colnum, long  firstrow, long  firstelem,
                          long  nelem, double *array)
{
    return fits_write_col_dbl(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_flt(int  colnum, long  firstrow, long  firstelem,
                          long  nelem, float *array)
{
    return fits_write_col_flt(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_int(int  colnum, long  firstrow, long  firstelem,
                          long  nelem, int *array)
{
    return fits_write_col_int(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_null(int  colnum, long  firstrow, long  firstelem,
                          long  nelem)
{
    return fits_write_col_null(fptr, colnum, firstrow, firstelem,
                              nelem, &status);
}

int FitsIO::update_key_dbl(char *keyname, double value,
                           int decim, char *comment)
{
    return fits_update_key_dbl(fptr, keyname, value, decim, comment, &status);
}

int FitsIO::update_key_flt(char *keyname, float value,
                           int decim, char *comment)
{
    return fits_update_key_flt(fptr, keyname, value, decim, comment, &status);
}

int FitsIO::update_key_lng(char *keyname, long value, char *comment)
{
    return fits_update_key_lng(fptr, keyname, value, comment, &status);
}

int FitsIO::update_key_str(char *keyname, char *value, char *comment)
{
    return fits_update_key_str(fptr, keyname, value, comment, &status);
}

int FitsIO::write_comment(const char *comment)
{
    return fits_write_comment(fptr, comment, &status);
}

int FitsIO::write_history(const char *info)
{
    return fits_write_history(fptr, info, &status);
}

int FitsIO::create_binary_tbl(long naxis2, int tfields, char **ttype,
                              char **tform, char **tunit, char *extnm)
{
    return fits_create_tbl(fptr, BINARY_TBL, naxis2, tfields, ttype,
                           tform, tunit, extnm, &status);
}

int FitsIO::movabs_hdu(int hdunum, int *exttype)

{
    return fits_movabs_hdu(fptr, hdunum, exttype, &status);
}

int FitsIO::write_tdim(int colnum, int naxis, long naxes[])

{
    return fits_write_tdim(fptr, colnum, naxis, naxes, &status);
}

int FitsIO::update_key_log(char *keyname, int value, char *comment)

{
    return fits_update_key_log(fptr, keyname, value, comment, &status);
}

int FitsIO::open_file(const char *filename, const char *mode)
{
    int iomode = 0;
    if (strcmp(mode, "rw") == 0)
    {
        iomode = READWRITE;
    }
    else if (strcmp(mode, "r") == 0)
    {
        iomode = READONLY;
    }
    else
    {
        return -666;
    }
    return fits_open_file(&fptr, filename, iomode, &status);
}

int FitsIO::read_int_key(char *keyname, void *value, char *comm)
{
    return fits_read_key(fptr, TINT, keyname, value, comm, &status);
}

int FitsIO::write_col_str(int colnum, long firstrow, long firstelem,
                          long nelem, char **array)
{
    return fits_write_col_str(fptr, colnum, firstrow,
                              firstelem, nelem, array, &status);
}

int FitsIO::write_col_lng(int  colnum, long  firstrow, long  firstelem,
                          long  nelem, long *array)
{
    return fits_write_col_lng(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_log(int colnum, long firstrow, long firstelem,
                          long nelem, char *array)
{
    return fits_write_col_log(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_sht(int colnum, long firstrow, long firstelem,
                          long nelem, short *array)
{
    return fits_write_col_sht(fptr, colnum, firstrow, firstelem,
                              nelem, array, &status);
}

int FitsIO::write_col_uint(int colnum, long firstrow, long firstelem,
                           long nelem, unsigned int *array)
{
    return fits_write_col_uint(fptr, colnum, firstrow, firstelem,
                               nelem, array, &status);
}

int FitsIO::read_errmsg(char *err_message)

{
    return fits_read_errmsg(err_message);
}

int FitsIO::create_img(int bitpix, int naxis, long *naxes)

{
    return fits_create_img(fptr, bitpix, naxis, naxes, &status);
}

void FitsIO::report_error(FILE *stream, int stat)  // don't use status, a macro!

{
    fits_report_error(stream, stat);
} 
