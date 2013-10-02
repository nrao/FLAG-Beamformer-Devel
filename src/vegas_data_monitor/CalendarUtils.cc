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
#include <math.h>

#if !defined(VXWORKS)
using std::cout;
using std::endl;
#endif

// Calendar to TimeStamp and TimeStamp to Calendar conversion facilities.

static void caldat(long julian, int *mm, int *id, int *iyyy);
static long julday(int mm, int id, int iyyy);

void
calendarDate(TimeStamp &ts, int &month, int &day, int &year)
{
long julian;
int mo, d, yr;

    julian = ts.MJD() + 2400001;
    
    caldat(julian, &mo, &d, &yr);
    month = mo;
    day = d;
    year = yr;
}

TimeStamp
calendarDate(int month, int day, int year)
{
TimeStamp ts;
long julian;

    if(year == 0) {
        cout << "TimeStamp::calendarDate() there is no year zero." << endl;
        return(ts); // mjd = 0, sec = 0.0
    }
    julian = julday(month, day, year);
    if(julian < 0) {
        cout << "TimeStamp::calendarDate() Conversion Error" << endl;
        return(ts); // mjd = 0, sec = 0.0
    }
    ts.MJD((int)julian - 2400001);
    return(ts);
}


#define IGREG 2299161

void caldat(long julian, int *mm, int *id, int *iyyy)
{
	long ja, jb, jc, jd, je;
    double jalpha, djulian = (double)julian;

    /* Lots of ugly casting, but keeps the compiler from complaining */

	if (julian >= IGREG) {
		jalpha = ((djulian - 1867216)- 0.25) / 36524.25;
		ja = static_cast<long>(julian + 1 + static_cast<long>(jalpha) - 
                 static_cast<long>(0.25 * jalpha));
	} else
		ja = julian;
	jb = ja + 1524;
	jc = static_cast<long>(6680.0 + (static_cast<float>(jb - 2439870) - 122.1) / 365.25);
	jd = static_cast<long>(365 * jc + static_cast<long>(0.25 * static_cast<double>(jc)));
	je = static_cast<long>(static_cast<double>(jb - jd) / 30.6001);
	*id = static_cast<int>(jb - jd - 
                           static_cast<long>(30.6001 * static_cast<double>(je)));
	*mm = static_cast<int>(je - 1);
	if (*mm > 12) *mm -= 12;
	*iyyy = static_cast<int>(jc - 4715);
	if (*mm > 2) --(*iyyy);
	if (*iyyy <= 0) --(*iyyy);
}
#undef IGREG

/* (C) Copr. 1986-92 Numerical Recipes Software .i1. */
#define JULK2 (15+31L*(10+12L*1582))

long julday(int mm, int id, int iyyy)
{
	long jul;
	int ja,jy=iyyy,jm;

	if (jy < 0) ++jy;
	if (mm > 2) {
		jm=mm+1;
	} else {
		--jy;
		jm=mm+13;
	}
	jul = (long) (floor(365.25*jy)+floor(30.6001*jm)+id+1720995);
	if (id+31L*(mm+12L*iyyy) >= JULK2) {
		ja=(int)(0.01*jy);
		jul += 2-ja+(int) (0.25*ja);
	}
	return jul;
}
#undef JULK2
/* (C) Copr. 1986-92 Numerical Recipes Software .i1. */
