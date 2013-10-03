/*******************************************************************
 *  data_plot.cc - Implements a nice little strip-chart using the qwt
 *  library.
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
 *  $Id: data_plot.cc,v 1.2 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#include <stdlib.h>
#include <qwt_painter.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_scale_widget.h>
#include <qwt_legend.h>
#include <qwt_scale_draw.h>
#include <qwt_math.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QGroupBox>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>

#include <algorithm>
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "data_plot.h"
#include "strip_options.h"

using namespace std;
using namespace boost;

DataPlot::DataPlot(string device, ConfigData &d, vector<Qt::GlobalColor> *data_colors, QWidget *parent):
    QwtPlot(parent),
    _lines(d.lines),
    _plot_size(d.seconds),
    _auto_scale(true),
    _min_y(0.0),
    _max_y(0.0),
    _actual_min_y(0.0),
    _actual_max_y(0.0),
    _mask(0ul),
    _display_source(device),
    _curve(_lines),
    _measpwr(&_curve[0]),
    _new_curve(_lines, true),
    _x(_plot_size, 0.0),
    _y(_lines),
    _conf(d)
{
    int i;
    Qt::GlobalColor color;

    // We don't need the cache here
    canvas()->setPaintAttribute(QwtPlotCanvas::BackingStore, false);
    canvas()->setPaintAttribute(QwtPlotCanvas::BackingStore, false);

#if QT_VERSION >= 0x040000
#ifdef Q_WS_X11
    /*
       Qt::WA_PaintOnScreen is only supported for X11, but leads
       to substantial bugs with Qt 4.2.x/Windows
     */
    canvas()->setAttribute(Qt::WA_PaintOnScreen, true);
#endif
#endif

    _align_scales();

    //  Initialize data
    vector<vector<double> >::iterator vi;
    vector<circular_buffer<double> >::iterator cbi;
    vector<string>::iterator vs;

    // keep history in circular buffers, one per line
    _y_history = vector<circular_buffer<double> >(_lines);

    for (i = 0; i < _plot_size; i++)
    {
        _x[i] = (static_cast<double>(_plot_size) /
                 static_cast<double>(_plot_size)) * i;     // time axis
    }

    for (vi = _y.begin(); vi != _y.end(); ++vi)
    {
        vi->resize(_plot_size, 0.0);
    }

    for (cbi = _y_history.begin(); cbi != _y_history.end(); ++cbi)
    {
        cbi->set_capacity(_plot_size);
    }

    // Assign a title
    // setTitle("A Test for High Refresh Rates");
    // insertLegend(new QwtLegend(), QwtPlot::BottomLegend);

    for (i = 0; i < _lines; ++i)
    {
        color = data_colors ? (*data_colors)[i % data_colors->size()] : Qt::black;
        _measpwr[i] = new QwtPlotCurve(QString(""));
        _measpwr[i]->attach(this);
        _measpwr[i]->setPen(QPen(color));
        _measpwr[i]->setRawSamples(_x.data(), _y[i].data(), _plot_size);
        _mask[i] = 1;
    }

#if 0
    //  Insert zero line at y = 0
    QwtPlotMarker *mY = new QwtPlotMarker();
    mY->setLabelAlignment(Qt::AlignRight|Qt::AlignTop);
    mY->setLineStyle(QwtPlotMarker::HLine);
    mY->setYValue(0.0);
    mY->attach(this);
#endif

    // Axis
    setAxisTitle(QwtPlot::xBottom, "Time/seconds");
    setAxisScale(QwtPlot::xBottom, 0, _plot_size);

    setAxisTitle(QwtPlot::yLeft, "dBm");
    setAxisScale(QwtPlot::yLeft, _min_y, _max_y);

}

DataPlot::~DataPlot()
{
    int i;


    for (i = 0; i < _lines; ++i)
    {
	delete _measpwr[i];
    }
}

//
//  Set a plain canvas frame and align the scales to it
//
void DataPlot::_align_scales()
{
    // The code below shows how to align the scales to
    // the canvas frame, but is also a good example demonstrating
    // why the spreaded API needs polishing.

    canvas()->setFrameStyle(QFrame::Box | QFrame::Plain );
    canvas()->setLineWidth(1);

    for ( int i = 0; i < QwtPlot::axisCnt; i++ )
    {
        QwtScaleWidget *scaleWidget = (QwtScaleWidget *)axisWidget(i);

        if (scaleWidget)
        {
            scaleWidget->setMargin(0);
        }

        QwtScaleDraw *scaleDraw = (QwtScaleDraw *)axisScaleDraw(i);

        if (scaleDraw)
        {
            scaleDraw->enableComponent(QwtAbstractScaleDraw::Backbone, false);
        }
    }
}

/********************************************************************
 * DataPlot::newData(int line, double dat)
 *
 * Updates the strip chart history buffers with a new data point for
 * line 'line'. If line == _line - 1 (the last one), updates the plot.
 *
 * @param int line: The line to whom this data point belongs to.
 * @param double dat: the data point.
 *
 *******************************************************************/

void DataPlot::newData(std::string *source, std::string *sampler, double dat)
{
    std::vector<std::string> strs;
    boost::split(strs, *source, boost::is_any_of("."));
    std::string subdevice = strs[1];

    if (_display_source == subdevice)  // only interested in our data...
    {
        int line = (*sampler)[sampler->size() - 1] == '1' ? 0 : 1;

        if (line < _lines)
        {
            _y_history[line].push_back(dat);
            _new_curve[line] = false;

            if (line == (_lines - 1))
            {
                _display_data();
            }
        }
    }
}

void DataPlot::toggle_mask_bit(int bit)
{
    std::bitset<32> nm;

    nm = _mask;
    nm[bit] = !nm[bit];
    setMask(nm.to_ulong());
}

void DataPlot::new_scaling(std::string *source, bool autoscale, double min_y, double max_y)
{
    if (_display_source == *source)
    {
        autoScale(autoscale);

        if (!autoscale)
        {
            yMin(min_y);
            yMax(max_y);
        }
    }
}

/********************************************************************
 * DataPlot::setMask(uint32_t mask)
 *
 * Sets a new display mask.
 *
 * @param uint32_t mask: the new mask
 *
 *******************************************************************/

void DataPlot::setMask(uint32_t mask)

{
    int i;
    bitset<32> dm(mask);

    for (i = 0; i < _lines; ++i)
    {
        if (dm[i] && !_mask[i])
        {
            _measpwr[i]->attach(this);
        }
        else if (!dm[i] && _mask[i])
        {
            _measpwr[i]->detach();
        }
    }

    _mask = dm;
    _display_data();
}

/********************************************************************
 * DataPlot::_display_data()
 *
 * Displays the data.  This is a separate slot from 'newData' so that
 * the data can be displayed at a rate independent of the data update
 * rate.
 *
 *******************************************************************/

void DataPlot::_display_data()

{
    int i;
    double margin;
    vector<double> min, max;


    for (i = 0; i < _lines; ++i)
    {
        _reload_y(i);

        if (_auto_scale)
        {
            if (_mask[i])
            {
                min.push_back(*min_element(_y[i].begin(), _y[i].end()));
                max.push_back(*max_element(_y[i].begin(), _y[i].end()));
            }
        }
    }

    if (_auto_scale)
    {
        if (_mask.to_ulong() > 0UL)
        {
            _min_y = *min_element(min.begin(), min.end());
            _max_y = *max_element(max.begin(), max.end());

            margin = (_max_y - _min_y) * 0.1;
            _min_y -= margin;
            _max_y += margin;
        }
        else
        {
            _min_y = -1.0;
            _max_y = 1.0;
        }
    }

    setAxisScale(QwtPlot::yLeft, _min_y, _max_y);
    replot();
}

/********************************************************************
 * DataPlot::clear()
 *
 * Wipes out the data buffer, setting all elements to 0.
 *
 *******************************************************************/

void DataPlot::clear()

{
    int j, k;


    for (j = 0; j < _lines; ++j)
    {
        for (k = 0; k < _plot_size; ++k)
        {
            _y_history[j].push_back(0.0);
        }

        _reload_y(j);
        _new_curve[j] = true;
    }

    if (_auto_scale)
    {
        _min_y = -1.0;
        _max_y = 1.0;
        setAxisScale(QwtPlot::yLeft, _min_y, _max_y);
    }

    replot();
}

/********************************************************************
 * DataPlot::autoScale()
 *
 * Returns scaling status
 *
 * @return bool: true if auto scaling, false if manual scaling
 *
 *******************************************************************/

bool DataPlot::autoScale()

{
    return _auto_scale;
}

void DataPlot::autoScale(bool as)
{
    _auto_scale = as;
}

/********************************************************************
 * DataPlot::yMin()
 *
 * returns the current minimum y-axis scale value.
 *
 * @return double, the minimum y-axis scale value.
 *
 *******************************************************************/

double DataPlot::yMin()

{
    return _min_y;
}

void DataPlot::yMin(double v)
{
    _min_y = v;
}

/********************************************************************
 * DataPlot::yMax()
 *
 * returns the current maximum y-axis scale value.
 *
 * @return double, the current maximum y-axis scale value.
 *
 *******************************************************************/

double DataPlot::yMax()

{
    return _max_y;
}

void DataPlot::yMax(double v)
{
    _max_y = v;
}

/********************************************************************
 * DataPlot::seconds()
 *
 * return the number of seconds worth of data that is displayed by the
 * strip chart.
 *
 * @return double, the number of seconds displayed.
 *
 *******************************************************************/

double DataPlot::seconds()

{
    return (_x[1] - _x[0]) * _plot_size;
}

/********************************************************************
 * DataPlot::seconds(double seconds)
 *
 * Called when the user desires different display history
 * parameters, i.e. number of seconds displayed
 *
 * @param double seconds: number of seconds to display.
 *
 *******************************************************************/

void DataPlot::seconds(double seconds)

{
    int i;
    vector<vector<double> >::iterator vi;
    vector<circular_buffer<double> >::iterator cbi;


    _plot_size = static_cast<int>(seconds);

    // resize buffers first...
    _x.resize(_plot_size);

    for (cbi = _y_history.begin(); cbi != _y_history.end(); ++cbi)
    {
        cbi->set_capacity(_plot_size);
    }

    for (vi = _y.begin(); vi != _y.end(); ++vi)
    {
        vi->resize(_plot_size, 0.0);
    }

    // reload the data...
    for (i = 0; i < _plot_size; i++)
    {
        _x[i] = (static_cast<double>(_plot_size) /
                 static_cast<double>(_plot_size)) * i;
    }

    setAxisScale(QwtPlot::xBottom, 0,
                 static_cast<double>(_plot_size));

    // Set the new buffers for the plot
    for (i = 0; i < _lines; ++i)
    {
        _reload_y(i);
        _curve[i]->setRawSamples(_x.data(), _y[i].data(), _plot_size);
    }

    replot();
}

/********************************************************************
 * DataPlot::_reload_y(int line)
 *
 * This helper function transfers data from the Y axis history circular
 * buffers into the actual plot buffers, most recent entry first.
 *
 *******************************************************************/

void DataPlot::_reload_y(int line)

{
    circular_buffer<double>::const_reverse_iterator rcbi;

    _y[line].clear();

    for (rcbi = _y_history[line].rbegin();
         rcbi != _y_history[line].rend(); ++rcbi)
    {
        _y[line].push_back(*rcbi);
    }
}
