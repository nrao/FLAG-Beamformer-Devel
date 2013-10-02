/*******************************************************************
 *  adchist.cc - implementation of VEGAS ADC snapshot histogram.  Based
 *  on qwt code samples.
 *
 *  Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
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
 *******************************************************************/

#include <stdlib.h>
#include <qpen.h>
#include <qwt_plot_layout.h>
#include <qwt_legend.h>
#include <qwt_legend_item.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_histogram.h>
#include <qwt_column_symbol.h>
#include <qwt_series_data.h>
#include "adchist.h"
#include "hist_options.h"

#include <string>
#include <iostream>
#include <boost/algorithm/string.hpp>


using namespace std;
using namespace boost;

class Histogram: public QwtPlotHistogram
{
public:
    Histogram(const QString &, const QColor &);

    void setColor(const QColor &);
    void setValues(int bin, QwtInterval ragne, uint numValues, const double *values);
};

Histogram::Histogram(const QString &title, const QColor &symbolColor):
    QwtPlotHistogram(title)
{
    setStyle(QwtPlotHistogram::Columns);

    setColor(symbolColor);
}

void Histogram::setColor(const QColor &symbolColor)
{
    QColor color = symbolColor;
    color.setAlpha(180);

    setPen(QPen(Qt::black));
    setBrush(QBrush(color));

    QwtColumnSymbol *symbol = new QwtColumnSymbol(QwtColumnSymbol::Box);
    symbol->setFrameStyle(QwtColumnSymbol::Raised);
    symbol->setLineWidth(2);
    symbol->setPalette(QPalette(color));
    setSymbol(symbol);
}

void Histogram::setValues(int bin, QwtInterval range, uint numValues, const double *values)
{
    // QwtIntervalSample: A sample of the types (x1-x2, y) or (x, y1-y2)
    // The data is given as 256 elements, 0-255. The data will be
    // plotted as -128-to-128, or anything between those numbers.  Thus
    // the right index offset for 'values' must be calculated by
    // adding 128 to 'range.minValue()'.  So, for example, if
    // 'range.minValue()' is -128, adding 128 returns 0, the first
    // element of 'values'.  If 'range.minValue()' is -60, then adding
    // 128 yields the 68th entry in 'values'.  We also check to ensure
    // that 'range' is within the range of 'values'.

    int values_offset = (int)range.minValue() + 128;
    int values_top_limit = values_offset + (int)range.width();
    int sample_size = (int)range.width() / bin;

    // We're trying to accomodate any bin size, not just what is evenly
    // divisible into the range.  So if there is a remainder, add
    // another bin.  It will get shorted in value.
    if ((int)range.width() % bin)
    {
        sample_size++;
    }

    if (values_top_limit > (int)numValues)
    {
        return;
    }

    QVector<QwtIntervalSample> samples(sample_size);

    for (int i = 0; i < sample_size; ++i)
    {
        // 'interval' here represents a specific bin, starting at 'lower',
        // and ending before 'upper'.
        double lower = (double)i * bin + range.minValue();
        double upper = lower + bin;
        QwtInterval interval(lower, upper);
        interval.setBorderFlags(QwtInterval::ExcludeMaximum);

        // So here we set 'samples[i]' to the sum of all the 'values'
        // that fit within the bin.
        double bin_data = 0;

        for (int k = 0; k < bin; ++k)
        {
            int index = i * bin + values_offset + k;

            if (index > values_top_limit)
            {
                break;
            }

            bin_data += values[index];
        }

        samples[i] = QwtIntervalSample(bin_data, interval);
    }

    // The less than useful doxygen documentation: QwtIntervalSeriesData
    // is "Interface for iterating over an array of intervals."
    setData(new QwtIntervalSeriesData(samples));
}

ADCPlot::ADCPlot(string source, string title, const QColor &color, QWidget *parent):
    QwtPlot(parent),
    _color(color),
    _x_min(-128),
    _x_max(128),
    _bin_size(1),
    _bins(256, 0.0),
    _display_source(source),
    _sampler(title)
{
    setTitle(title.c_str());

    setCanvasBackground(QColor(Qt::gray));
    plotLayout()->setAlignCanvasToScales(true);

    setAxisTitle(QwtPlot::yLeft, "counts");
    setAxisTitle(QwtPlot::xBottom, "value");
    populate();
    replot(); // creating the legend items
    setAutoReplot(true);
}

void ADCPlot::populate()
{
    QwtPlotGrid *grid = new QwtPlotGrid;
    grid->enableX(false);
    grid->enableY(true);
    grid->enableXMin(false);
    grid->enableYMin(false);
    grid->setMajPen(QPen(Qt::black, 0, Qt::DotLine));
    grid->attach(this);
    _histogram = new Histogram("adcpwr", _color);
    _histogram->setValues(1, QwtInterval(-128, 128), _bins.size(), _bins.data());
    _histogram->setVisible(true);
    _histogram->attach(this);
}

void ADCPlot::newData(std::string *source, std::string *adc, int sze, char *data)

{
    vector<string> strs;
    split(strs, *source, is_any_of("."));
    string subdevice = strs[1];

    if (subdevice == _display_source && *adc == _sampler)  // ignore if not for us
    {
        QwtInterval interval(_x_min, _x_max);

        for (int i = 0; i < 256; ++i)
        {
            _bins[i] = 0.0;
        }

        for (int i = 0; i < sze; ++i)
        {
            _bins[(int)data[i] + 128] += 1.0;
        }

        _histogram->setValues(_bin_size, interval, _bins.size(), _bins.data());
        replot();
    }
}

double ADCPlot::xMin()
{
    return _x_min;
}

double ADCPlot::xMax()
{
    return _x_max;
}

int ADCPlot::binSize()
{
    return _bin_size;
}

void ADCPlot::xMin(double v)
{
    _x_min = v;
}

void ADCPlot::xMax(double v)
{
    _x_max = v;
}

void ADCPlot::binSize(int v)
{
    _bin_size = v;
}

void ADCPlot::update_histogram_options()

{
    int r;
    HistOptionsDialog dlg(_display_source, this);
    HistOptionsDialog::options_data d;

    d.x_min = xMin();
    d.x_max = xMax();
    d.bin_size = binSize();
    dlg.setData(d);
    r = dlg.exec();

    if (r)
    {
        dlg.getData(d);

        if (d.x_min_changed)
        {
            xMin(d.x_min);
        }

        if (d.x_max_changed)
        {
            xMax(d.x_max);
        }

        if (d.bin_size_changed)
        {
            binSize(d.bin_size);
        }
    }
}
