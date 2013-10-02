// Copyright (C) 1994 Associated Universities, Inc. Washington DC, USA.
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
//	GBT Operations
//	National Radio Astronomy Observatory
//	P. O. Box 2
//	Green Bank, WV 24944-0002 USA

// $Id: TimeStamp.cc,v 1.36 2007/04/27 17:09:03 jbrandt Exp $


#include "TimeStamp.h"

#if defined(VXWORKS)
#include <semLib.h>
#include "mcTime.h"
#include <fioLib.h>
#include <iomanip.h>
#elif defined(WIN32) || defined(__WIN32__)
#include <time.h>
#include <iostream>
#include <iomanip>
#else
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <iomanip>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(VXWORKS)
using std::ostream;
using std::istream;
using std::ios;
using std::setw;
using std::setfill;
using std::cerr;
using std::endl;
#endif

// extern "C" int xdr_double(...);

// Notational conventions:
// dmjd is a modified julian date with fractional portion of day
// djd is a julian date with fractional portion of day
// mjd is a modified julian date, without fractional portion of day
// 

ostream& operator<<(ostream& os, const TimeStamp& ts)
{
        int hrs, mins, secs;
        ts.HrsMinSec(hrs, mins, secs);
	os << setw(5)
		<< (int)ts.theMJD << " "
		<< setw(2)
		<< hrs << ":"
		<< setw(2) << setfill('0')
		<< mins << ":" 
		<< setw(2) << setfill('0')
		<< secs << setfill(' ') << setw(0);
	return os;
}

istream& operator>>(istream &is, TimeStamp &ts)
{
	int h,m,s;
	int mjd;
	is >> mjd;
	is >> h;
	if (is.get() != ':')
	{
		is.clear(ios::badbit | is.rdstate());
		return is;
	}
	is >> m;
	if (is.get() != ':')
	{
		is.clear(ios::badbit | is.rdstate());
		return is;
	}
	is >> s;
	ts.Sec(3600*h + 60*m + s);
	ts.refFrame = TimeStamp::UTC;
	ts.units = TimeStamp::SECS;
	ts.theMJD = mjd;
	return is;
}

bool_t xdr_TimeStamp(XDR *xdrs, TimeStamp *obj)
{
	if (!xdr_double(xdrs,&obj->theSec))
	{
		cerr << "xdr_TimeStamp: theSec" << endl;
		return(FALSE);
	}
	if (!xdr_int(xdrs,&obj->theMJD))
	{
		cerr << "xdr_TimeStamp: theMJD" << endl;
		return(FALSE);
	}
	if (!xdr_short(xdrs,&obj->flags))
	{
		cerr << "xdr_TimeStamp: flags" << endl;
		return(FALSE);
	}
	if (!xdr_char(xdrs,&obj->refFrame))
	{
		cerr << "xdr_TimeStamp: refFrame" << endl;
		return(FALSE);
	}
	if (!xdr_char(xdrs,&obj->units))
	{
		cerr << "xdr_TimeStamp: units" << endl;
		return(FALSE);
	}
	return(TRUE);
}

// Constructors
// Units of MSEC (milliseconds) and UTC reference are assumed
TimeStamp::TimeStamp() : 
    theSec(0.0), theMJD(0), flags(0), refFrame(UTC), units(SECS)
{
}

TimeStamp::TimeStamp(const int msec) : 
    theSec(0), theMJD(0), flags(0), refFrame(UTC), units(SECS)
{
	Sec(((double)msec)/1000.0);
}

TimeStamp::TimeStamp(const int mjd, const int msec) :
    theSec(0), theMJD(mjd), flags(0), refFrame(UTC), units(SECS)
{
	Sec(((double)msec)/1000.0);
}

TimeStamp::TimeStamp(const double dmjd) :
    theSec(0), theMJD((int)dmjd),
    flags(0), refFrame(UTC), units(SECS)
{
	Sec((dmjd-((int)dmjd)) * SecondsPerDay);
}

TimeStamp::TimeStamp(const int mjd, const double sec) :
    theSec(0.0), theMJD(mjd), flags(0), refFrame(UTC), units(SECS)
{
	Sec(sec);
}

void
TimeStamp::HrsMinSec(int &hrs, int &min, int &sec) const
{
    sec = (int)(fabs(Sec()) + .5);
    hrs = sec/3600;
    sec = sec - hrs*3600;
    min = sec/60;
    sec = sec - min*60;
    if (Sec() < 0)
    {
        hrs = -hrs;
    }
}

void
TimeStamp::HrsMinSec(int &hrs, int &min, double &sec) const
{
    double asec = fabs(Sec());
    int isec = static_cast<int>(asec);
    hrs = isec/3600;
    isec = isec - hrs*3600;
    min = isec/60;
    sec = asec - hrs*3600 - min*60;
    if (Sec() < 0)
    {
        hrs = -hrs;
    }
}

// Methods for parsing into Hrs:Min:Sec
double
TimeStamp::Hrs() const
{
    return(24.0 * theSec/unitsPerDay());
}

double
TimeStamp::Min() const
{
    return(60.0 * Hrs());
}

double
TimeStamp::Sec() const
{
    switch(units) {
    case SECS:
        return(theSec);
    // break;
    case MSEC:
        return(theSec/1000.0);
    // break;
    case USEC:
        return(theSec / 1000000.0);
    // break;
    case NSEC:
        return(theSec / 1000000000.0);
    // break;
    case TURNS:
        return(theSec * SecondsPerDay);
    // break;
    default:
        return(0.0);
    // break;
    }
}

#ifdef VXWORKS
	#define IROUND(x) irint((x))
#elif defined(WIN32) || defined(__WIN32__)
	// This is a TBF. Should use a IEEE rounding function instead.
	#define IROUND(x) ((x))
#else
	#define IROUND(x) rint((x))
#endif

// Methods for access milliseconds
int
TimeStamp::Msec() const
{
    switch(units) {
    case SECS:
        return((int)IROUND(theSec * 1000.0));
    // break;
    case MSEC:
        return((int)IROUND(theSec));
    // break;
    case USEC:
        return((int)IROUND(theSec / 1000.0));
    // break;
    case NSEC:
        return((int)IROUND(theSec / 1000000.0));
    // break;
    case TURNS:
        return((int)IROUND(theSec * MilliSecondsPerDay));
    // break;
    default:
        return(0);
    // break;
    }
}

void
TimeStamp::Msec(const int xx)
{
int x = xx;
    if (x >= MilliSecondsPerDay) {
        int days =  x / int(MilliSecondsPerDay);
        theMJD += days;
        x %= int(MilliSecondsPerDay);
    } 
    
    switch(units) {
    case SECS:
        theSec = x/1000.0;
    break;
    case MSEC:
        theSec = x;
    break;
    case USEC:
        theSec = x*1000.0;
    break;
    case NSEC:
        theSec = x*1000000.0;
    break;
    case TURNS:
        theSec = (double)x/MilliSecondsPerDay;
    break;
    default:
    break;
    }
}

void
TimeStamp::Sec(const double ss)
{
double x = ss;
    if (x >= SecondsPerDay) {
        int days =  int(x) / int(SecondsPerDay);
        theMJD += days;
        x -= days * SecondsPerDay;
    } 

    switch(units) {
    case SECS:
        theSec = x;
    break;
    case MSEC:
        theSec = x*1000.0;
    break;
    case USEC:
        theSec = x*1000000.0;
    break;
    case NSEC:
        theSec = x*1000000000.0;
    break;
    case TURNS:
        theSec = x/SecondsPerDay;
    break;
    default:
    break;
    }
}

// Method for accessing MJD with fractional part of day
double
TimeStamp::DMJD() const
{
    switch(units) {
    case SECS:
        return(theMJD + (theSec / SecondsPerDay));
    // break;
    case MSEC:
        return(theMJD + (theSec / MilliSecondsPerDay));
    // break;
    case USEC:
        return(theMJD + (theSec / MicroSecondsPerDay));
    // break;
    case NSEC:
        return(theMJD + (theSec / NanoSecondsPerDay));
    // break;
    case TURNS:
        return(theMJD + theSec);
    // break;
    default:
        return(0.0);
    // break;
    }
}

double
TimeStamp::fractionOfDay() const
{
    return(theSec/unitsPerDay());
}

// Methods for Setting TimeStamp from a double MJD or JD
void
TimeStamp::DJD(const double fractionalJulianDay)
{
double fmjd = fractionalJulianDay-2400000.5;
    
    theMJD = (int)(fmjd);
    theSec = (fmjd - ((int)theMJD)) * unitsPerDay();
}

void
TimeStamp::MJD(const double dmjd)
{
    theMJD = (int)dmjd;
    theSec = (dmjd - ((int)theMJD)) * unitsPerDay();
}


// addition/subtraction operators
TimeStamp 
operator+(const TimeStamp &A, const TimeStamp &B)
{
TimeStamp C;
double secsum;
int mjd;

    mjd = A.MJD() + B.MJD();
    secsum = A.Sec() + B.Sec();

    if(mjd > 0 && secsum < 0.0) {
        secsum += 86400.0;
        mjd--;
    } else if(mjd < 0 && secsum > 0.0) {
        mjd++;
        secsum -= 86400.0;
    } 

    C.MJD(((int)(secsum/86400.0)) + mjd);
    C.Sec(fmod(secsum, 86400.0));
    return(C);
}

TimeStamp 
operator-(const TimeStamp &A, const TimeStamp &B)
{
TimeStamp C;
double secdif;
int mjd;

    mjd = A.MJD() - B.MJD();
    secdif = A.Sec() - B.Sec();

    if(mjd > 0 && secdif < 0.0) {
        secdif += 86400.0;
        mjd--;
    } else if(mjd < 0 && secdif > 0.0) {
        mjd++;
        secdif -= 86400.0;
    } 
    
    C.MJD(mjd + ((int)(secdif/86400.0)));
    C.Sec(fmod(secdif, 86400.0));
    return(C);
}


// Boolean Operators
int 
TimeStamp::operator==(const TimeStamp &p) const 
{ 
    return(theMJD == p.theMJD && (fabs(Sec() - p.Sec()) < 1E-6));
}

int 
TimeStamp::operator>(const TimeStamp &p)  const 
{ 
    return((theMJD > p.theMJD) || ((theMJD == p.MJD()) && (Msec() > p.Msec()))); 
}

int 
TimeStamp::operator<(const TimeStamp &p)  const 
{ 
    return((theMJD < p.theMJD) || ((theMJD == p.MJD()) && (Msec() < p.Msec()))); 
}

int 
TimeStamp::operator>=(const TimeStamp &p)  const 
{ 
    return(!(*this < p)); 
}

int 
TimeStamp::operator<=(const TimeStamp &p)  const 
{ 
    return(!(*this > p)); 
}

int 
TimeStamp::operator!=(const TimeStamp &p)  const 
{ 
    return(!(*this == p)); 
}

void
TimeStamp::normalize()
{ 
const double unitsperday = unitsPerDay();

    // Note use of Sec() method. do not set theSec directly
    // cause units might be in turns etc..
    // If theMJD and theSec have different signs, the result may not
    // be correct.
    theMJD += (int)(theSec/unitsperday);
    theSec = fmod(theSec, unitsperday);
}

double
TimeStamp::unitsPerDay() const
{
    switch(units) {
    case SECS:
        return(SecondsPerDay);
    // break;
    case MSEC:
        return(MilliSecondsPerDay);
    // break;
    case USEC:
        return(MicroSecondsPerDay);
    // break;
    case NSEC:
        return(NanoSecondsPerDay);
    // break;
    case TURNS:
        return(1.0);
    // break;
    default:
        return(0.0);
    // break;
    }
}

char *TimeStamp::generate_FITS_date_time_string(char *buffer)
{
    int month, day, year, hours, minutes, seconds;
    calendarDate(*this, month, day, year);
    HrsMinSec(hours, minutes, seconds);

    sprintf(buffer,
            "%04d-%02d-%02dT%02d:%02d:%02d",
            year, month, day, hours, minutes, seconds);
    return buffer;
}
