/*******************************************************************
 ** NumericConversions.h - Routines that convert ASCII to Numeric
 *
 *  Copyright (C) 2004 Associated Universities, Inc. Washington DC, USA.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Correspondence concerning GBT software should be addressed as follows:
 *  GBT Operations
 *  National Radio Astronomy Observatory
 *  P. O. Box 2
 *  Green Bank, WV 24944-0002 USA
 *
 *  $Id: NumericConversions.h,v 1.1 2004/02/02 19:49:33 rcreager Exp $
 *
 *******************************************************************/

#if !defined(_NUMERICCONVERSIONS_H_)
#define _NUMERICCONVERSIONS_H_
#if !defined(VXWORKS)
#if defined(__cplusplus)

/********************************************************************
 * These all convert an ASCII string to the corresponding numeric
 * value.  Set 'base' to the numeric base that the string is encoded in
 * (ie 10 for dec, 16 for hex).  By default 'base' is 0, which means
 * that the base is dependent on the ASCII string prefix: 0x for hex,
 * 0 for octal, none for decimal.
 * The functions will return 'false' if the entire ASCII string was not
 * used in the coversion. If the flag 'f' is set to true, a message
 * to stderr will also be printed in case of failure.
 *
 * Note: These could become templates after migration from VxWorks
 *******************************************************************/

bool convert(char const *str, short &val, int base = 0, bool f = false);
bool convert(char const *str, unsigned short &val, int base = 0, bool f = false);
bool convert(char const *str, unsigned int &val, int base = 0, bool f = false);
bool convert(char const *str, int &val, int base = 0, bool f = false);
bool convert(char const *str, long &val, int base = 0, bool f = false);
bool convert(char const *str, unsigned long &val, int base = 0, bool f = false);
bool convert(char const *str, double &val, bool f = false);

#endif // __cplusplus
#endif // VXWORKS
#endif // _NUMERICCONVERSIONS_H_

