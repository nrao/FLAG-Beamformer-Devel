/*******************************************************************
 *  katcp_data_source.cc - implementation of a monitor data
 *  source based on the Grail DeviceClient library.
 *
 *  This class functions by spawning off a thread to handle the RPC.
 *  This thread will also handle the callback from the monitor source,
 *  explode the data into its pieces and post them to a data fifo.
 *  The GUI thread reads the data fifo.  This read is performed by a
 *  Qt timer.  This way the data is provided to the rest of the GUI,
 *  through an 'emit', in the same GUI thread that runs everything
 *  else.
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
 *  $Id: katcp_data_source.cc,v 1.4 2009/12/07 23:00:32 rcreager Exp $
 *
 *******************************************************************/


#include "katcp_data_source.h"

// KATCP
#include "RoachInterface.h"

#include "TCondition.h"
#include "Thread.h"
#include "tsemfifo.h"
#include "EventDispatcher.h"
#include "ThreadLock.h"

#include <QTimerEvent>

#include <algorithm>
#include <vector>
#include <map>
#include <iostream>
#include <unistd.h>
#include <math.h>

using namespace std;

static const size_t ADC_DATA_COUNT = 16384;

//----------------//
// dBm conversion //
//----------------//
double
powerToDbm(double p)
{
    return 10 * log10(p) - 39;
}

//-----------------//
// Sample Variance //
//-----------------//
double sampleVariance(vector<int8_t> &values)
{
    double mean = 0;
    int n = values.size();

    for (vector<int8_t>::iterator i = values.begin(); i != values.end(); ++i)
    {
        mean += *i;
    }

    mean /= n;
    double variance = 0;

    for (int32_t i = 0; i < n; ++i)
    {
        double diff = values[i] - mean;
        variance += diff * diff;
    }

    return variance / n;
}

/********************************************************************
 * This is the implementation data for class KATCPDataSource.
 * KATCPDataSource relies extensively on the Ygor library of
 * classes (see #include list).  In particular the Grail headers
 * 'Paramter.h' and 'DeviceClientMap.h' extensively include from the
 * Ygor source headers.  Because of this the class uses the 'pimpl'
 * idiom (see 'Exceptional C++ by Herb Sutter) to reduce compilation
 * dependencies.
 *
 *******************************************************************/

struct KATCPDataSource::ds_impl
{
    ds_impl(KATCPDataSource *);

    struct adc_data
    {
        adc_data() :
            adcpwr(ADC_DATA_COUNT)
        {}

        std::string device;
        std::string name;
        std::vector<char> adcpwr;
    };

    struct measpwr_data
    {
        std::string device;
        std::string name;
        double val;
    };

    int d_interval;
    int d_timerId;
    bool katcp_done;
    TCondition<bool> katcp_task_created;
    Mutex mtx;
    tsemfifo<adc_data> adc_fifo;
    tsemfifo<measpwr_data> measpwr_fifo;
    Thread<KATCPDataSource> task;
};

KATCPDataSource::ds_impl::ds_impl(KATCPDataSource *ds)
    :
    d_interval(1000),
    d_timerId(-1),
    katcp_done(false),
    katcp_task_created(false),
    adc_fifo(200),
    measpwr_fifo(200),
    task(ds, &KATCPDataSource::_katcp_task)
{
}

/********************************************************************
 * KATCPDataSource::KATCPDataSource(QObject *parent)
 *
 * Constructor.  Sets up the implementation data and creates a
 * DeviceClient for Mustang.
 *
 * @param QObject *parent: the parent Qt object.  Needed because this
 * is also a Qt object.
 *
 *******************************************************************/

KATCPDataSource::KATCPDataSource(vector<string> subdev, QObject *parent)
    : DataSource(parent),
      roach_name(subdev)
{
    _impl = new ds_impl(this);
    _set_timer_interval(_impl->d_interval);
}

/********************************************************************
 * KATCPDataSource::~KATCPDataSource()
 *
 * Destructor.  Stops the RPC task that listens for monitor data, then
 * cleans up the implementation data.
 *
 *******************************************************************/

KATCPDataSource::~KATCPDataSource()

{
    terminate();
    delete _impl;
}

/********************************************************************
 * KATCPDataSource::start()
 *
 * Starts the data flowing.
 *
 *******************************************************************/

void KATCPDataSource::start()

{
    _start_katcp_task();
}

/********************************************************************
 * KATCPDataSource::terminate()
 *
 * This is a slot that terminates the KATCP thread.
 *
 *******************************************************************/

void KATCPDataSource::terminate()

{
    _end_katcp_task();
    assert_error();
}

/********************************************************************
 * KATCPDataSource::timerEvent(QTimerEvent *)
 *
 * Called every time the timer fires. Reads the fifos and empties them
 * if needed.
 *
 * @param QTimerEvent *:
 *
 *******************************************************************/

void KATCPDataSource::timerEvent(QTimerEvent *event)

{
    ds_impl::measpwr_data mp;
    ds_impl::adc_data adc;

    if (event->timerId() == _impl->d_timerId)
    {
        while (_impl->measpwr_fifo.try_get(mp))
        {
            emit new_measpwr_data(&mp.device, &mp.name, mp.val);
        }

        while (_impl->adc_fifo.try_get(adc))
        {
            // cout << adc.device << " " << adc.name << " ";

            // for (int i = 0; i < 10; ++i)
            // {
            //     cout << (int)adc.adcpwr[i] << " ";
            // }

            // cout << endl;

            emit new_adc_data(&adc.device, &adc.name, adc.adcpwr.size(), adc.adcpwr.data());
        }
    }
}

/********************************************************************
 * KATCPDataSource::_katcp_task()
 *
 ** Runs a bunch of KATCP clients.  In Linux, anything that runs in this
 *  polling loop thread must also have been created in this thread,
 *  and must also be destroyed in this thread.  This means the
 *  DeviceClientMap and all DeviceClients.
 *
 * @param void *p: The data for the task, the simulate flag.
 *
 *******************************************************************/

void KATCPDataSource::_katcp_task()

{
    _impl->katcp_task_created.signal(true);
    map<string, RoachInterface *> roach_interface;
    map<string, bool> katcp_ok;
    time_t next_error_reset, now;


    for (vector<string>::iterator i = roach_name.begin(); i != roach_name.end(); ++i)
    {
        RoachInterface *p = new RoachInterface(i->c_str(), 7147);
        roach_interface[*i] = p;
        katcp_ok[*i] = true;
    }

    time(&now);
    next_error_reset = now + 60;

    while (!_impl->katcp_done)
    {
        vector<int8_t> adcpwr1_value(ADC_DATA_COUNT), adcpwr2_value(ADC_DATA_COUNT);

        // If a query to any of the roaches fail, katcp_ok['roach_name']
        // will be set to false and that roach will subsequently be
        // ignored. Here we periodically reset that error so that the
        // roach is retried every so often, but not every time through,
        // because select() time out would slow down the polling.

        if (time(&now) >= next_error_reset)
        {
            for (vector<string>::iterator i = roach_name.begin(); i != roach_name.end(); ++i)
            {
                katcp_ok[*i] = true;
            }

            next_error_reset = now + 60;
        }

        for (vector<string>::iterator i = roach_name.begin(); i != roach_name.end(); ++i)
        {
            // ignore any roach that is giving trouble.
            if (!katcp_ok[*i])
            {
                continue;
            }

            RoachInterface *p = roach_interface[*i];

            bool snap =
                p->setValue("trig", 0) &&
                p->setValue("adcsnap0_ctrl", 0) &&
                p->setValue("adcsnap1_ctrl", 0) &&
                p->setValue("adcsnap0_ctrl", 5) &&
                p->setValue("adcsnap1_ctrl", 5) &&
                p->setValue("trig", 1);

            if (!snap)
            {
                cerr << "Snap failed for " << *i << "." << endl;
                katcp_ok[*i] = false;
                continue;
            }

            bool r1 = p->getValue("adcsnap0_bram",
                                  (int8_t *)adcpwr1_value.data(), ADC_DATA_COUNT);

            if (!r1)
            {
                cerr << "adcsnap0_bram failed for " << *i << "." << endl;
                katcp_ok[*i] = false;
                continue;
            }

            bool r2 = p->getValue("adcsnap1_bram",
                                  (int8_t *)adcpwr2_value.data(), ADC_DATA_COUNT);

            if (!r2)
            {
                cerr << "adcsnap1_bram failed for " << *i << "." << endl;
                katcp_ok[*i] = false;
                continue;
            }

            KATCPDataSource::ds_impl::adc_data adcdata;
            adcdata.name = "adcpwr1";
            adcdata.device = string("VEGAS.") + *i;
            adcdata.adcpwr.resize(ADC_DATA_COUNT);
            memcpy(adcdata.adcpwr.data(), adcpwr1_value.data(), ADC_DATA_COUNT);

            if (!_impl->adc_fifo.try_put(adcdata))
            {
                cerr << "sampler data fifo is full!" << endl;
            }

            adcdata.name = "adcpwr2";
            adcdata.adcpwr.resize(ADC_DATA_COUNT);
            memcpy(adcdata.adcpwr.data(), adcpwr2_value.data(), ADC_DATA_COUNT);

            if (!_impl->adc_fifo.try_put(adcdata))
            {
                cerr << "sampler data fifo is full!" << endl;
            }

            KATCPDataSource::ds_impl::measpwr_data mpdata;
            double measpwr1_value, measpwr2_value;

            measpwr1_value = powerToDbm(sampleVariance(adcpwr1_value));
            measpwr2_value = powerToDbm(sampleVariance(adcpwr2_value));
            mpdata.name = "measpwr1";
            mpdata.device = string("VEGAS.") + *i;
            mpdata.val = measpwr1_value;

            if (!_impl->measpwr_fifo.try_put(mpdata))
            {
                cerr << "Sampler " << mpdata.name << " fifo is full!" << endl;
            }

            mpdata.name = "measpwr2";
            mpdata.val = measpwr2_value;

            if (!_impl->measpwr_fifo.try_put(mpdata))
            {
                cerr << "Sampler " << mpdata.name << " fifo is full!" << endl;
            }
        }

        sleep(1);
    }

    for (vector<string>::iterator i = roach_name.begin(); i != roach_name.end(); ++i)
    {
        delete roach_interface[*i];
    }

    _impl->katcp_task_created.signal(true);
}

/********************************************************************
 * KATCPDataSource::_set_timer_interval(int ms)
 *
 * Sets up a timer that will fire an event every 'ms' milliseconds.
 * The event is handled by KATCPDataSource::timerEvent().  Taken
 * from the Qwt examples.
 *
 * @param int ms: the timer interval, in milliseconds.
 *
 *******************************************************************/

void KATCPDataSource::_set_timer_interval(int ms)

{
    _impl->d_interval = ms;

    if ( _impl->d_timerId >= 0 )
    {
        killTimer(_impl->d_timerId);
        _impl->d_timerId = -1;
    }

    if (_impl->d_interval >= 0 )
    {
        _impl->d_timerId = startTimer(_impl->d_interval);
    }
}

/********************************************************************
 * KATCPDataSource::_start_katcp_task()
 *
 ** Sets up and starts the katcp task.
 *
 *******************************************************************/

bool KATCPDataSource::_start_katcp_task()

{
    _impl->katcp_task_created.set_value(false);
    cout << "Starting KATCP task" << endl;
    _impl->task.start();

    if (!_impl->katcp_task_created.wait(true, 10000000L))
    {
        cerr << "Timed out starting KATCP clients task" << endl;
        return false;
    }

    cout << "KATCP task started." << endl;
    return true;
}

/********************************************************************
 * KATCPDataSource::_end_katcp_task()
 *
 ** Ends the KATCP task by setting the '_katcp_done' flag to true.  This
 *  will cause that task to exit next time through its loop.
 *
 *******************************************************************/

void KATCPDataSource::_end_katcp_task()

{
    _impl->katcp_task_created.set_value(false);
    _impl->katcp_done = true;

    if (!_impl->katcp_task_created.wait(true, 10000000L))
    {
        cerr << "Timed out waiting for KATCP task to end" << endl;
    }

    cout << "KATCP task ended" << endl;
}
