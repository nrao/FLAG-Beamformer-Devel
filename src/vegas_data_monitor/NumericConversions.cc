/*******************************************************************
 ** NumericConversions.cc - C++ routines to convert numbers encoded
 *  as ASCII strings to numeric values.
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
 *  $Id: NumericConversions.cc,v 1.1 2004/02/02 19:49:33 rcreager Exp $
 *
 *******************************************************************/

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/********************************************************************
 * convert(char const *str, short &val, int base, bool f)
 *
 ** Converts a string to an short.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param short &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, short &val, int base, bool f)

{
    char *endp;


    val = static_cast<short>(strtol(str, &endp, base));

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, unsigned short &val, int base, bool f)
 *
 ** Converts a string to an unsigned short.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param unsigned short &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, unsigned short &val, int base, bool f)

{
    char *endp;


    val = static_cast<unsigned short>(strtoul(str, &endp, base));

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, int &val, int base, bool f)
 *
 ** Converts a string to a int.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param int &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, int &val, int base, bool f)

{
    char *endp;


    val = static_cast<int>(strtol(str, &endp, base));

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, unsigned int &val, int base, bool f)
 *
 ** Converts a string to an unsigned int.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param unsigned int &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, unsigned int &val, int base, bool f)

{
    char *endp;


    val = static_cast<unsigned int>(strtoul(str, &endp, base));

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, long &val, int base, bool f)
 *
 ** Converts a string to a long.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param long &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, long &val, int base, bool f)

{
    char *endp;


    val = strtol(str, &endp, base);

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, unsigned long &val, int base, bool f)
 *
 ** Converts a string to an unsigned long.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param unsigned long &val: the location to store the conversion
 *  @parm int base: numeric base, between 2 and 32 inclusive, or 0.  If
 *        0, 10 is assumed unless the number is prefixed with 0x, in
 *        which case 16 is assumed.
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, unsigned long &val, int base, bool f)

{
    char *endp;


    val = strtoul(str, &endp, base);

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

/********************************************************************
 * convert(char const *str, double &val, bool f)
 *
 ** Converts a string to a double.
 *
 *  @param const char *str: The ASCII encoded numeric value
 *  @param double &val: the location to store the conversion
 *  @parm bool f: if 'true', prints error message on failure.
 *
 * @return Returns 'true' if the input string consisted of entirely
 *         numeric values and the conversion succeeded, 'false'
 *         if otherwise.
 *
 *******************************************************************/

bool convert(char const *str, double &val, bool f)

{
    char *endp;


    val = strtod(str, &endp);

    if (*endp != '\0' && !isspace(*endp))
    {
        if (f)
        {
            fprintf(stderr, "parameter %s is not a numeric value\n", str);
        }

        return false;
    }

    return true;

}

