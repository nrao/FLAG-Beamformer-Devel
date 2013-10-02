/*******************************************************************
 *  ConfigData.h - Declares a struct that contains the configuration
 *  data for the Mustand Data Monitor Display.  This data allows the
 *  application to be configured for number (and which) DFBs and Rows
 *  to display.
 *
 *  Copyright (C) <date> Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: ConfigData.h,v 1.1 2009/12/07 14:43:15 rcreager Exp $
 *
 *******************************************************************/

#if !defined(_CONFIGDATA_H_)
#define _CONFIGDATA_H_

#include <string>
#include <vector>


struct ConfigData

{
    std::string source;
    bool manual_scaling;
    double ymin;
    double ymax;
    int seconds;
    int lines;
    std::vector<std::string> devices;
};

#endif // _CONFIGDATA_H_
