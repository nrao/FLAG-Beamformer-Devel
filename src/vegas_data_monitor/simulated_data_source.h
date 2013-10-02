/*******************************************************************
 *  simulated_data_source.h - Provides simulated data to the Data
 *  Monitoring Application.
 *
 *  Copyright (C) 2009 Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: simulated_data_source.h,v 1.2 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#if !defined(_SIMULATED_DATA_SOURCE_H_)
#define _SIMULATED_DATA_SOURCE_H_

#include "data_source.h"

#include <string>
#include <vector>

class SimulatedDataSource : public DataSource

{
    Q_OBJECT

  public:

    SimulatedDataSource(std::vector<std::string> subdev, int ms, QObject *parent = 0);

  public slots:

    virtual void start();
    virtual void terminate();

  protected:

    virtual void timerEvent(QTimerEvent *event);

  private:

    void setTimerInterval(int ms);

    int d_interval; // timer in ms
    int d_timerId;
    std::vector<std::string> subdevice;
};

#endif // _SIMULATED_DATA_SOURCE_H_
