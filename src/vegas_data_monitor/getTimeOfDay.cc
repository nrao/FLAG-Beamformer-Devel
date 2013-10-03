// Copyright (C) 2011 Associated Universities, Inc. Washington DC, USA.
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
//       GBT Operations
//       National Radio Astronomy Observatory
//       P. O. Box 2
//       Green Bank, WV 24944-0002 USA

#include "TimeStamp.h"

#ifdef VXWORKS
#include <semLib.h>
#include "mcTime.h"
#include <math.h>
#include <iostream.h>
#include <iomanip.h>
#elif defined(WIN32) || defined(__WIN32__)
#include <time.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#define M_PI            3.14159265358979323846
#define M_PI_2          1.57079632679489661923
#define M_PI_4          0.78539816339744830962
#else
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#endif

#if !defined(VXWORKS)
using std::cout;
using std::endl;
#endif

TimeStamp
getTimeOfDay(int ref)
{
TimeStamp retval;

    switch(ref) {
    // Get Universal Coordinated Time
    case TimeStamp::UTC:
        retval.reference(TimeStamp::UTC);
        retval.unitType(TimeStamp::SECS);
        getUTC(retval);
    break;

    // Get Greenwhich Mean Siderial Time. Note that seconds are in siderial
    // not solar (as in UTC) units.
    // In order to get LMST = GMST - longitude
    // LAST = LMST + equation of the equniox (see sla_eqeqx())
    case TimeStamp::GMST:
        retval.refFrame = TimeStamp::GMST;
        retval.unitType(TimeStamp::TURNS);
        getUTC(retval);
        retval.Sec(lmst(retval.DMJD(), 0.0) * (43200.0/M_PI));
    break;

    default:
        cout << "getTimeOfDay: Reference not recognized" << endl;
    }
    return(retval);
}


void
getUTC(TimeStamp &retval)
{
#ifdef VXWORKS
    retval.MJD(mcJulianDate()-2400001);
    retval.Sec(mcUsSinceMidnight()/1000000.0);
#else
    struct timeval val;

    gettimeofday(&val, NULL);

    getUTC_from_timeval(retval, val);
#endif // VXWORKS
}

#if !defined(VXWORKS)

// Note: The ifdefs below swap names of the two routines below. The
// alternate version is used for comparison in the unit tests.
#define FASTERGETUTC
#define epoch_MJD (40587)
#ifdef FASTERGETUTC
void getUTC_from_timeval(TimeStamp &retval, struct timeval &val)
#else
void alternate_getUTC_from_timeval(TimeStamp &retval, struct timeval &val)
#endif
{
    // A much faster optimized routine.

    unsigned long days;
    unsigned long secsmid;

    // the number of whole days since Jan 1st, 1970
    days = val.tv_sec/86400;
    secsmid = val.tv_sec - days*86400;
    retval.MJD((int)(days + epoch_MJD));
    retval.Sec((double)secsmid + (double)val.tv_usec/1.0E6);
}

#ifdef FASTERGETUTC
void alternate_getUTC_from_timeval(TimeStamp &retval, struct timeval &val)
#else
void getUTC_from_timeval(TimeStamp &retval, struct timeval &val)
#endif
{
    // The slower version which uses gmtime() and jday() functions.
    struct tm *tim;
    int ja,jy,jm;
    int year, month, day;
    int jday;
    double sec;


    tim = gmtime(&val.tv_sec);

    month = tim->tm_mon+1;
    day = tim->tm_mday;
    year = tim->tm_year+1900;

    jy = year;

    if (jy == 0)
        cout << "TimeStamp::getTimeOfDay: there is no year zero." << endl;
    if (jy < 0) 
        ++jy;
    if (month > 2) {
        jm=month+1;
    } else {
        --jy;
        jm=month+13;
    }
    jday = (int)(floor(365.25*jy)+floor(30.6001*jm)+day+1720995);

    // switch over to gregorian calendar 
    if (day+31L*(month+12L*year) >= (15+31L*(10+12L*1582))) { 
        ja=(int)(0.01*jy);
        jday += 2-ja+(int) (0.25*ja);
    }
    double dbl_usec = (double)val.tv_usec;
    sec = ((((tim->tm_hour * 60.0) + tim->tm_min) * 60.0 + tim->tm_sec) + 
           (dbl_usec/1000000.0));
    retval.MJD(jday-2400001);
    retval.Sec(sec);
}
#endif

/*
   lmst()

   Compute Mean Local Siderial Time.
   Given:
      UT1 in MJD format.
      Longitude in Radians
   Return:
      LST in radians.

   Notes:
      Algorithm accuracy depends upon input parameters and estimation of
      the observers Longitude.

      For most purposes UTC can be used.

   Author:
      jbrandt@nrao.edu
*/

  
double // Return local siderial time in radians
lmst(const double dmjd, const double longitude)
{
    double tu;
    double gmst;
    double LST;
    double ut1;
#if defined(VXWORKS)
    int mjd;
    mjd = (int)dmjd;
    ut1 = dmjd - (double)mjd;  /* equiv to fmod(ut1, 1.0) */
#else
    ut1 = fmod ( dmjd, 1.0 );
#endif

    /* Julian centuries from fundamental epoch J2000 to this UT */
    tu = (dmjd - 51544.5) / 36525.0;

    /* GMST at this UT */
    gmst = ( ut1 * (2.0*M_PI) +
             ( 24110.54841 +
             ( 8640184.812866 +
             ( 0.093104 - ( 6.2e-6 * tu ) ) * tu ) * tu ) * M_PI/43200.0 );

    // correct for observer longitude
    LST = gmst - longitude;

    // assure a positive answer
    LST = fmod(LST, 2.0*M_PI);
    if(LST < 0.0) {
        LST += 2.0*M_PI;
    }
    return(LST);
}

