/*
Copyright (C) 2011 Associated Universities, Inc. Washington DC, USA.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

Correspondence concerning GBT software should be addressed as follows:
      GBT Operations
      National Radio Astronomy Observatory
      P. O. Box 2
      Green Bank, WV 24944-0002 USA
*/


#ifndef Math_h
#define Math_h

/*
 * $Log: MathExtras.h,v $
 * Revision 1.3  2011/06/14 17:45:31  mclark
 * added copyright header to overlooked files
 *
 * Revision 1.2  2004/05/28 14:46:07  jbrandt
 * Added temperature conversions.
 *
 * Revision 1.1  1993/06/30 22:02:33  jbrandt
 * Initial revision
 *
 */


#if defined(VXWORKS)
#include <math.h>
    #ifndef M_PI
        #define M_PI PI
    #endif
#include <vxWorks.h>
#include <types.h>
    // This avoids many error messages about unused variables.
    // Should probably be const variables anyway.
#define static const
#include <private/trigP.h>
#undef static
#include <sys/times.h>
#else
#include <math.h>
#endif

const double DegToRadians = M_PI/180.0;
const double RadToDegrees = 180.0/M_PI;

// Utility Functions
inline double degtorad(double x) { return(x*DegToRadians); }
inline double radtodeg(double x) { return(x*RadToDegrees); }
inline double hrtorad(double hr) { return(hr/12.0 * M_PI); }
inline double radtohr(double rad) { return(rad/M_PI * 12.0); }
inline double CELSIUS_to_KELVIN(double deg_C) { return(deg_C+273.15); }
inline double KELVIN_to_CELSIUS(double deg_K) { return(deg_K-273.15); }

#endif
