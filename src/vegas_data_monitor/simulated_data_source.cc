/*******************************************************************
 *  simulated_data_source.cc -- A simulated data source class for the
 *  Mustang Data Monitoring application.  Sets up a timer and starts
 *  pumping out data.
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
 *  $Id: simulated_data_source.cc,v 1.2 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#include "simulated_data_source.h"

#include <QTimerEvent>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <string>
#include <sstream>

#include <boost/random.hpp>
#include <boost/random/normal_distribution.hpp>

using namespace std;
using namespace boost;

static const int DATA_COUNT = 16384;

/********************************************************************
 * SimulatedDataSource(QObject *parent)
 *
 * Constructor. Sets up the base class to simulate data.
 *
 * @param QObject *parent
 *
 *******************************************************************/

SimulatedDataSource::SimulatedDataSource(vector<string> subdev, int ms, QObject *parent)
    : DataSource(parent),
      d_interval(ms),
      d_timerId(-1),
      subdevice(subdev)
{
}

void SimulatedDataSource::start()

{
    setTimerInterval(d_interval);
}


void SimulatedDataSource::terminate()

{
    cout << "SimulatedDataSource::terminate() called" << endl;
}

/********************************************************************
 * SimulatedDataSource::timerEvent(QTimerEvent *)
 *
 * Called every time the timer fires.  This function simulates data
 * for testing purposes and is not used during normal operations.  It
 * provides the chart with data in the same way that the actual data
 * provider will, by emitting a signal that can be connected to the
 * plot and grid.
 *
 * @param QTimerEvent *:
 *
 *******************************************************************/

void SimulatedDataSource::timerEvent(QTimerEvent *event)

{
    char dat[DATA_COUNT];
    int j;


    if (event->timerId() != d_timerId)
    {
        return;
    }

    // do this for each device
    for (vector<string>::iterator s = subdevice.begin(); s != subdevice.end(); ++s)
    {
        // and each sampler in the device
        for (int i = 0; i < 2; ++i)
        {
            // fake adc data
            for (j = 0; j < DATA_COUNT; ++j)
            {
                dat[j] = (char)(rand() % 256) - 127;
            }

            stringstream adc;
            adc << "adcpwr" << i << ends;
            string adcpwr = adc.str().c_str();
            string device = string("VEGAS.") + *s;
            emit new_adc_data(&device, &adcpwr, DATA_COUNT, dat);

            // fake measpwr data
            int r = rand() % 4000 - 2000; //this produces numbers between -2000 - +2000
            double x = -29.5 + ((double)r) / 10000.0;
            stringstream meas;
            meas << "measpwr" << i << ends;
            string measpwr = meas.str().c_str();
            emit new_measpwr_data(&device, &measpwr, x);
        }
    }
}

/********************************************************************
 * SimulatedDataSource::setTimerInterval(int ms)
 *
 * Sets up a timer that will fire an event every 'ms' milliseconds.
 * The event is handled by SimulatedDataSource::timerEvent().  Taken
 * from the Qwt examples.
 *
 * @param int ms: the timer interval, in milliseconds.
 *
 *******************************************************************/

void SimulatedDataSource::setTimerInterval(int ms)

{
    d_interval = ms;

    if ( d_timerId >= 0 )
    {
        killTimer(d_timerId);
        d_timerId = -1;
    }

    if (d_interval >= 0 )
    {
        d_timerId = startTimer(d_interval);
    }
}
