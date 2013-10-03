/*******************************************************************
 *  Histogram class to display VEGAS adc snapshots.  Based on qwt
 *  example code.
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


#ifndef _ADC_PLOT_H_

#include <qwt_plot.h>
#include <string>
#include <vector>

class Histogram;

class ADCPlot: public QwtPlot
{
    Q_OBJECT

public:
    ADCPlot(std::string source, std::string title, const QColor &, QWidget * = NULL);
    double xMin();
    double xMax();
    int binSize();
    void xMin(double);
    void xMax(double);
    void binSize(int);

public slots:

    void newData(std::string *, std::string *, int, char *);
    void update_histogram_options();

private:
    void populate();

    QColor _color;
    Histogram *_histogram;
    int _adc_id;
    double _x_min;
    double _x_max;
    int _bin_size;
    std::vector<double> _bins;
    std::string _display_source;
    std::string _sampler;
};

#endif
