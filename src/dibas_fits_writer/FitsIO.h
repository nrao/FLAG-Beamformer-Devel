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

#ifndef FitsIO_h
#define FitsIO_h

#include "SwitchingSignals.h"
#include <string.h>
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>


#define DATEBLD_TARGET_STRING "DATEBLD  goes  here"

typedef double TimeStamp;

/// Encapsulates Pence's CFITSIO library to handle telescope data.
/// This abstract base class encapsulates Pence's CFITSIO library
/// for handling telescope data. 
/// Full implementations should extend this class with a class that is knowledgeable of telescope 
/// data writing needs.
class FitsIO
{
public:
    /// The constructor takes:
    // <dl> <i>path_env_variable</i>
    // <dd> the environement variable string as found in <b>system.conf</b>
    // to be passed to <b>getConfigValue</b> to determine the root
    // directory for the FITS file path.
    // <dl> <i>fits_version</i>
    // <dd> the value of the suffix for the FITS keyword FITSVER
    // which tracks the FITS definition for this device.  For consistency
    // this should be specified by the macro FITS_VERSION in the beginning
    // of the file where this base class is initialized.
    // <dl> <i>instrument</i>
    // <dd> FITS standard keyword INSTRUME, identifies the instrument to
    // acquire this data
    // <dl> <i>simulateFlag</i>
    // <dd> the value of the FITS keyword SIMULATE which identifies whether
    // the instrument was running in simulation mode or not when the data
    // was taken
    // </dl>
    // Example:
    // <pre>
    // #define FITS_VERSION 1
    // 
    // ...
    //
    // SpectrometerFitsIO::SpectrometerFitsIO(char *bankName) :
    //                FitsIO("YGOR_DATA",
    //                       FITS_VERSION,
    //                       "Spectrometer",
    // #ifdef SIMULATE
    //                       1
    // #else
    //                       0
    // #endif
    //                 ),
    //                 theTestConfiguration(0),
    //                 samplerA(0),
    //                 samplerB(0),
    //                 ...
    // </pre>
    virtual ~FitsIO();

    FitsIO(const char *path_env_variable,
           int fits_version,
           const char *instrument,
           int simulateFlag);
    FitsIO(const FitsIO& rhs);
    const FitsIO& operator=(const FitsIO& rhs);

    // This method returns the current <b>status</b> where 0
    // indicates success..
    int getStatus();
    void setStatus(int);

    // This method returns the FITS file root directory.
    const char *getRootDirectory() const ;

    // This method sets the FITS file root directory.
    void setRootDirectory(const char *root);
            
    // This method returns a message in <i>errtext</i>.
    void get_errstatus(char *errtext);

    // This method closes the FITS file.
    virtual int close();
    // </group>

    // This method is for filling 
    // the FITS file's path. <b>createDirectoryPath</b>
    // fills <b>path</b> with a directory path made up of
    // <i>args</i> directory names starting at the root.
    // It returns a pointer to the location in the path
    // where the filename would go.  If the address of
    // <i>buf</i> is the data member <i>path</i> then
    // <i>filenamePtr</i> is updated automatically for
    // use with <b>setFilename</b>.
    char *createDirectoryPath(char *buf, size_t buflength, int args, ...);

    // This method returns a pointer to path, i.e., the file currently
    // being written to.
    const char *getPath();

    // <b>setFilename</b> adds or replaces a file name at <i>buf</i>
    // it returns a pointer to the suffix
    // of the file which is ".fits".
    char  *setFilename(char *buf, TimeStamp &ts);

    // This method flushes the FITS file to disk
    void flush();

    // These methods generate headers used across FITS files.  They are
    // declared virtual to allow the additon of keywords and columns.
    // The argument HDU_number specifies the order of the table in the
    // file where HDU_number = 1 implies the primary HDU.
    // <example>
    // void SpectrometerFitsIO::createPrimaryHDU()
    // {
    //     createBasePrimaryHDU();
    // 
    //     fits_update_key_lng(fptr,
    //                         "FFT",
    //                         theFFTFlag,
    //                         "Have the data been Fourier transformed?",
    //                         &status);
    //     fits_update_key_str(fptr,
    //                         "BANK",
    //                         theBank,
    //                         "set of quadrants identifier",
    //                         &status);
    //     fits_update_key_str(fptr,
    //                         "QUADRANT",
    //                         theQuadrants,
    //                         "Quadrants contributing to Bank",
    //                         &status);
    //     fits_update_key_lng(fptr,
    //                         "NLAGS",
    //                         theNumberOfLags,
    //                         "number of lags in correlation function",
    //                         &status);
    //     fits_update_key_str(fptr,
    //                         "MODE",
    //                         theMode,
    //                         "Spectrometer mode",
    //                         &status);
    //     fits_update_key_lng(fptr, 
    //                         "NYQUIST",
    //                         twiceNyquistSampling, 
    //                         "Twice Nyquist Sampling Flag",
    //                         &status);
    //     fits_update_key_lng(fptr,
    //                         "SELFTEST",
    //                         selfTestMode,
    //                         "Is the spectrometer in self-test mode?",
    //                         &status);
    //     if (selfTestMode)
    //     {
    //         // This keyword only has meaning when we're in self test mode.
    //         fits_update_key_str(fptr,
    //                             "TESTCONF",
    //                             theTestConfiguration,
    //                             "Canned setup file",
    //                             &status);
    //     }
    // 
    //     flush();
    // }
    // </example>
    //
    // <example>
    // void SpectrometerFitsIO::createPortTable()
    // {
    //     char *ttypeMode[] = {"ATTENSET", "MEASPWR", "RATIO", "LEVEL",
    //                          "SPEED", "BANDWDTH", "FSTART", "FEND"};
    //     char *tformMode[] = {"1E", "1E", "1E", "1J","1D", "1D", "1D", "1D"};
    //     char *tunitMode[] = {"dBm", "V", "none", "COUNT",
    //                          "Hz", "Hz", "Hz", "Hz"};
    //     float
    //         port_attenuator_settings[SpectrometerId::number_of_input_ports];
    //     float measured_power_levels[SpectrometerId::number_of_input_ports];
    //     float duty_cycle_ratios[SpectrometerId::number_of_input_ports];
    //     int level[SpectrometerId::number_of_input_ports];
    //     double speed[SpectrometerId::number_of_input_ports];
    //     double bandwidth[SpectrometerId::number_of_input_ports];
    //     double fstart[SpectrometerId::number_of_input_ports];
    //     double fend[SpectrometerId::number_of_input_ports];
    // 
    //     int port_count = 0;
    //     for (int i = 0; i < SpectrometerId::number_of_input_ports; ++i)
    //     {
    //         if (theInputsUsed[i] != SpectrometerId::not_used)
    //         {
    //             port_attenuator_settings[port_count] =
    //                                        thePortAttenuatorSettings[i];
    //             measured_power_levels[port_count] =
    //                                        theMeasuredPowerLevels[i];
    //             duty_cycle_ratios[port_count] = theSetDutyCycleRatios[i];
    //             level[port_count] = theLevel;
    //             speed[port_count] = theSamplerSpeed;
    //             bandwidth[port_count] = theBandwidth;
    //             fstart[port_count] = theFstart;
    //             fend[port_count] = theFend;
    //             port_count++;
    //         }
    //     }
    //     set_number_ports(port_count);
    //     set_bank(bank_name, number_ports);
    //     set_port(port, number_ports);
    //
    //     createBasePortTable(2, 8, ttypeMode, tformMode, tunitMode);
    // 
    //     int column_number = get_base_number_port_columns() + 1;
    // 
    //     fits_write_col_flt(fptr, column_number++, 1, 1, number_ports,
    //                        port_attenuator_settings, &status);
    //     fits_write_col_flt(fptr, column_number++, 1, 1, number_ports,
    //                        measured_power_levels, &status);
    //     fits_write_col_flt(fptr, column_number++, 1, 1, number_ports,
    //                        duty_cycle_ratios, &status);
    //     fits_write_col_int(fptr, column_number++, 1, 1, number_ports, 
    //                        level, &status);
    //     fits_write_col_dbl(fptr, column_number++, 1, 1, number_ports, 
    //                        speed, &status);
    //     fits_write_col_dbl(fptr, column_number++, 1, 1, number_ports, 
    //                        bandwidth, &status);
    //     fits_write_col_dbl(fptr, column_number++, 1, 1, number_ports, 
    //                        fstart, &status);
    //     fits_write_col_dbl(fptr, column_number++, 1, 1, number_ports, 
    //                        fend, &status);
    // }
    // </example>
    // <group>
    virtual void createPrimaryHDU();
    virtual void createPortTable();
    virtual void createStateTable();
    virtual void createDataTable();
    // </group>

    // These methods return the number of columns generated in the
    // createBase<Table>Table() methods.
    // <group>
    int get_base_number_state_columns();
    int get_base_number_port_columns();
    int get_base_number_data_columns();
    // </group>

    // Set member functions for the PHDU 
    // <group>
    void set_startTime(const TimeStamp &time);
    void set_source(const char *source);
    void set_projectId(const char *projectId);
    void set_scanNumber(const int scanNumber);
    void set_scanId(const char *scanId);
    // </group>

    // Set member functions for the Port Table.  The call to
    // createPortTable() will attempt to generate <i>number_ports</i>
    // rows therefore number >= number_ports.
    // <group>
    void set_number_ports(const int number_ports);
    void set_bank(const char *bank, int number);
    void set_port(const int *port, int number);
    // </group>

    // Set member functions for the State Table.  The call to
    // createStateTable() will attempt to generate <i>number_phases</i>
    // rows therefore number >= number_phases.
    // <group>
    void set_number_phases(const int number_phases);
    void set_blanking(const double *blanking, int number);
    void set_phase_start(const double *phase_start, int number);
    void set_sig_ref_state(
         const SwitchingSignals::SigRefState *sig_ref_state, int number);
    void set_cal_state(
         const SwitchingSignals::CalState *cal_state, int number);
    void set_switch_period(const double &switch_period);
    void set_switching_signals_master(const char *switching_signals_master);
    // </group>

    static int mkdirp(const char *path, mode_t mode);
    static char *generate_FITS_date_time_string(TimeStamp dmjd, char *buffer);
    static char *generateDataTimeName(char *buffer, TimeStamp time);
protected:

    // Call throughs to cfitsio
    int create_binary_tbl(long naxis2, int tfields, char **ttype,
                          char **tform, char **tunit, char *extnm);
    int create_file(const char *name);
    int create_img(int bitpix, int naxis, long *naxes);
    // mode must be "r" or "rw"
    int insert_rows(long firstrow, long nrows);
    int movabs_hdu(int hdunum, int *exttype);
    int open_file(const char *filename, const char *mode);

    int read_int_key(char *keyname, void *value, char *comm);
    int read_errmsg(char *err_message);
    void report_error(FILE *stream, int stat);
    int update_key_dbl(char *keyname, double value, int decim, char *comment);
    int update_key_flt(char *keyname, float value, int decim, char *comment);
    int update_key_lng(char *keyname, long value, char *comment);
    int update_key_log(char *keyname, int value, char *comment);
    int update_key_str(char *keyname, char *value, char *comment);

    int write_col_byt(int colnum, long firstrow, long firstelem,
                      long nelem, unsigned char *array);
    int write_col_dbl(int  colnum, long  firstrow, long  firstelem,
                      long  nelem, double *array);
    int write_col_flt(int  colnum, long  firstrow, long  firstelem,
                      long  nelem, float *array);
    int write_col_int(int  colnum, long  firstrow, long  firstelem,
                      long  nelem, int *array);
    int write_col_lng(int  colnum, long  firstrow, long  firstelem,
                      long  nelem, long *array);
    int write_col_log(int colnum, long firstrow, long firstelem,
                      long nelem, char *array);
    int write_col_null(int colnum, long firstrow, long firstelem, long nelem);
    int write_col_sht(int colnum, long firstrow, long firstelem,
                      long nelem, short *array);
    int write_col_str(int colnum, long firstrow, long firstelem,
                      long nelem, char **array);
    int write_col_uint(int colnum, long firstrow, long firstelem,
                       long nelem, unsigned int *array);
    int write_comment(const char *comment);
    int write_history(const char *info);
    int write_tdim(int colnum, int naxis, long naxes[]);



    // This method generates the initial portion of the
    // indicated HDU.  The "Base" tables are required for
    // data files and the "Minimum" is required for log files.
    void createMinimumPrimaryHDU();
    void createBasePrimaryHDU();
    void createBasePortTable(int HDU_number,
                             int additional_numberColumns = 0,
                             char **additional_ttypeState = 0,
                             char **additional_tformState = 0,
                             char **additional_tunitState = 0);
    void createBaseStateTable(int HDU_number,
                              int additional_numberColumns = 0,
                              char **additional_ttypeState = 0,
                              char **additional_tformState = 0,
                              char **additional_tunitState = 0);
    void createBaseDataTable(int HDU_number,
                             int additional_numberColumns = 0,
                             char **additional_ttypeState = 0,
                             char **additional_tformState = 0,
                             char **additional_tunitState = 0);
    // <group>
    // </group>

    // These methods write specific primary HDU keywords and values.
    // <group>
    void update_key_date_obs();
    void update_key_telescop();
    void update_key_projid();
    void update_key_scan();
    // </group>

    // This method computes the value of the telescope
    // version keyword (data member telescope_version).
    // The default is to use the leaf directory name
    // of the environment variable YGOR_TELESCOPE.
    virtual void define_keyword_value_telescope_version();

    // This method prints to stderr all of the cfitsio errors
    // accumulated so far and returns the number printed.
    int print_all_error_messages(const char *where);

    enum { pathlength=1024,
           base_number_state_columns = 4,
           base_number_port_columns = 2,
           base_number_data_columns = 1
         };
    char *rootDirectory;

    // Primary Header
    //<group>
    char *dateObsString;
    char path[pathlength];
    char *filenamePtr;
    char *origin;
    char *telescope;
    char *telescope_comment;
    char *telescope_version_keyword;
    char *telescope_version;
    char *fits_version;
    char *instrument;
    int simulateFlag;
    //</group>

    // State Table
    //<group>
    double *blanking;
    double *phase_start;
    SwitchingSignals::SigRefState *sig_ref_state;
    SwitchingSignals::CalState *cal_state;
    int number_phases;
    char *switching_signals_master;
    double switch_period;
    //</group>

    // Port Table
    //<group>
    long number_ports;
    char *bank;
    int *port;
    //</group>

    int hduCount;
    long optimalRows;

    // Increment whenever a version is released with modified output as
    // the result of changes in the FitsIO library and its associated
    // software note 4.
    static const char *fitsvers_string_prefix;

    // Members which hold information obtained in set_<parameter> methods.
    // <group>
    TimeStamp startTime;
    char *source;
    char *projectId;
    long scanNumber;
    char *scanId;
    // </group>


    // This method sets optimalRows, the optimal number of rows
    // to be flushed.
    void setOptimalRows();

private:
    struct fitsio_data;
    fitsio_data *fid;
};

#endif
