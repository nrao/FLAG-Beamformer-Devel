
#ifndef TimeStamp_h
#define TimeStamp_h
//#ident "$Id: TimeStamp.h,v 1.27 2008/09/23 15:30:55 rcreager Exp $"
// ======================================================================
// Copyright (C) 1993 Associated Universities, Inc. Washington DC, USA.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Correspondence concerning GBT software should be addressed as follows:
//  GBT Operations
//  National Radio Astronomy Observatory
//  P. O. Box 2
//  Green Bank, WV 24944-0002 USA

#include "MathExtras.h"
#include <rpc/rpc.h>
#if defined(VXWORKS)
#include <iostream.h>
#define std
#else
#include <iostream>
#endif

#define SecondsPerDay 86400.0
#define MilliSecondsPerDay (SecondsPerDay * 1000.)
#define MicroSecondsPerDay (MilliSecondsPerDay * 1000.)
#define NanoSecondsPerDay  (MicroSecondsPerDay * 1000.)

//<summary>
// A class for producing and manipulating Time reference tags or TimeStamps.
// This includes an operating system interface to acquire the current time,
// on VxWorks SBC's or Various (sun) UNIX systems.
//</summary>

//<example>
// <h3>An example on acquiring the current Time:</h3>
// <srcblock>
// TimeStamp now = getTimeOfDay();
// cout "The current MJD and UTC are: " << now << endl;
// </srcblock>
// <h3>
// An example on converting to (Gregorian) calendar dates from a TimeStamp:
// </h3>
// <srcblock>
// int month, day, year;
// TimeStamp ts = getTimeOfDay();
// calendarDate(ts, month, day, year);
// // month day and year are set to the current calendar date.
// </srcblock>
// <h3>Setting a TimeStamp from a calendar date:</h3>
// <srcblock>
// TimeStamp ts = calendarDate(6, 16, 1995); 
// </srcblock>
// </example>

// <todo>
// </todo>

class TimeStamp;
TimeStamp getTimeOfDay(int ref = 1);
bool_t xdr_TimeStamp(XDR *xdr, TimeStamp *timestamp);

class TimeStamp {
public:

// <h2>Reference Frame enumerations</h2>
// Note: Only UTC and GMST can be computed using this
// library, since the apparent correction uses the Starlink library.
// See gbt_lmst() and sla_eqeqx().
// <dl>
// <dt>NOREF</dt>
// <dd>This represents an unset reference frame.</dd>
// <dt>UTC</dt>
// <dd>This is the default. It refers to the commonly known UTC. This system
//     is corrected for periodically using leap seconds.</dd>
// <dt>UT0</dt>
// <dd>This reference is based upon transits of various sources, and is
//     effected by variations in the earths rotational speed.</dd>
// <dt>UT1</dt>
// <dd>When UT0 is corrected for polar motion is is designated <b>UT1</b>.</dd>
// <dt>UT2</dt>
// <dd>When UT1 is corrected for seasonal inqeualities in the earths
//     rotational rate, it is called <b>UT2</b>. These variations presumably 
//     result from redistributions of air and water on the earths surface.</dd>
// <dt>GAST</dt>
// <dd>Greenwhich Apparent Sidereal Time</dd>
// <dt>GMST</dt>
// <dd>Greenwhich Mean Sidereal Time</dd>
// <dt>LAST</dt>
// <dd>Local Apparent Sidereal Time, based upon internal library longitude
//     for the Greenbank 140' telescope.</dd>
// <dt>LMST</dt>
// <dd>Local Mean Sidereal Time, based upon internal library longitude
//     of the Greenbank 140' telescope.</dd>
// <dt>OFFSET</dt>
// <dd>This refers to a simple offset from some other reference frame.</dd>
// </dl>
    enum RefFrame { NOREF = 0,
                    UTC,
                    UT0,
                    UT1,
                    UT2,
                    GAST,
                    GMST,
                    LAST,
                    LMST,
                    OFFSET };
    
    enum UnitType { NOTSET = 0,
                    SECS,
                    MSEC,
                    USEC,
                    NSEC,
                    TURNS };
    
    // Default Constructor Units are (milliseconds) and reference is UTC
    TimeStamp();
    
    // Construct a TimeStamp, setting only the milliseconds field.
    TimeStamp(const int msec);
                 
    // Construct a TimeStamp, setting both mjd and milliseconds fields.
    TimeStamp(const int mjd, const int msec);

    // Construct a TimeStamp from a fractional MJD.
    TimeStamp(const double ModifiedJulianDate);

    // Construct a TimeStamp, using the internal fields directly
    TimeStamp(const int mjd, const double sec);

    // Return the Fractional <b>Julian</b> Date e.g. (2449238.148252)
    double DJD() const { return(DMJD()+2400000.5); }

    // Set the TimeStamp from a Fractional Julian Date
    void DJD(const double fractionalJulianDay);

    // Set the MJD and milliseconds fields from a fractional MJD,
    // e.g. (49237.648)
    void MJD(const double fractionalModifiedJulianDay);

    // Set the MJD from a integer MJD e.g. (49237)
    void MJD(const int ModifiedJulianDay) { theMJD = ModifiedJulianDay; }

    // Return the integer MJD 
    int MJD() const { return(theMJD); }

    // Return the Fractional Modified Julian Date e.g. (DJD - 2400000.5)
    double DMJD() const;

    // Set the seconds since midnight from integral milliseconds.
    void Msec(const int x);

    // Set the seconds since midnight from fractional seconds.
    void Sec(const double x);

    // Return the portion of a day since midnight in milliseconds.
    int    Msec() const;

    // Return the portion of a day since midnight in seconds.
    double Sec() const;

    // Convert a TimeStamp to hrs,min,sec format.
    // The parameter sec is rounded.
    void HrsMinSec(int &hrs, int &min, double &sec) const;

    // Convert a TimeStamp to hrs,min,sec format (integer seconds)
    void HrsMinSec(int &hrs, int &min, int &sec) const;

    // Return the portion of a day since midnight in minutes.
    double Min() const;

    // Return the portion of a day since midnight in Hours.
    double Hrs() const;

    // Return the portion of a day since midnight in Turns.
    double fractionOfDay() const;

    // Boolean Operators (millisecond resolution only)
    int operator==(const TimeStamp &p) const;

    int operator>(const TimeStamp &p)  const;

    int operator<(const TimeStamp &p)  const;

    int operator>=(const TimeStamp &p)  const;

    int operator<=(const TimeStamp &p)  const;

    int operator!=(const TimeStamp &p)  const;

    // check and adjust for over/underflows
    void normalize();

    // Number of units in a day, based upon units field.
    double unitsPerDay() const;

    // Return the current reference frame enumeration
    RefFrame reference() const { return((RefFrame)refFrame); }

    // Return the current unit enumeration
    UnitType unitType()  const { return((UnitType)units); }

    // Set the reference frame
    void reference(RefFrame newRef)  { refFrame = (char)newRef; }

    // Set the units 
    void unitType(UnitType newUnits) { units = (char)newUnits; }

    // Set the flags field. Note: this field is for use by the Monitor system.
    void setFlags(short value) { flags = value; }

    // Get the flags field. Note: this field is for use by the Monitor system.
    short getFlags() const { return flags; }

    // This method generates a FITS compliant string of the form
    // 2001-02-15T01:01:01
    // in the provided buffer which must be able to hold 21 characters.
    char *generate_FITS_date_time_string(char *buffer);

    // Friend function declarations
    // Get the current Time of day on VXWORKS or UNIX system

//    friend TimeStamp getTimeOfDay(RefFrame ref = TimeStamp::UTC);
    friend TimeStamp getTimeOfDay(int);
    friend std::ostream &operator<<(std::ostream &, TimeStamp const &);
    friend std::istream &operator>>(std::istream &, TimeStamp &);
    friend bool_t xdr_TimeStamp(XDR *xdr, TimeStamp *timestamp);
private:
    double theSec;      /* Units since midnight */
    int theMJD;         /* integral MJD */
    short flags;        /* */
    char refFrame;      /* reference frame used for this TimeStamp */
    char units;         /* units for theSec */
};

// Add two TimeStamps
TimeStamp operator+(const TimeStamp &A, const TimeStamp &B);

// Subtract two TimeStamps
TimeStamp operator-(const TimeStamp &A, const TimeStamp &B);

double lmst(const double dmjd, const double longitude);

// Set a TimeStamp to the current UTC
void getUTC(TimeStamp &);

// Set a TimeStamp from the provided struct timeval
#if !defined(VXWORKS)
void getUTC_from_timeval(TimeStamp &, struct timeval &);
#endif

// Fill-in the month, day, year represented by the TimeStamp
void calendarDate(TimeStamp &, int &month, int &day, int &year);

// Return A TimeStamp representing month/day/year.
TimeStamp calendarDate(int month, int day, int year);


#endif
