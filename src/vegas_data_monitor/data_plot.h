/*******************************************************************
 *  data_plot.h - A nice little strip chart using the Qwt library.
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
 *  $Id: data_plot.h,v 1.2 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#ifndef _DATA_PLOT_H
#define _DATA_PLOT_H 1

#include <qwt_plot.h>
#include <vector>
#include <map>
#include <bitset>
#include <boost/circular_buffer.hpp>

#include <stdint.h>

#include "ConfigData.h"


class QwtPlotCurve;

class DataPlot : public QwtPlot
{
    Q_OBJECT

public:
    DataPlot(std::string device, ConfigData &d,
             std::vector<Qt::GlobalColor> *data_colors = NULL, QWidget* = NULL);
    ~DataPlot();

    bool autoScale();
    double yMin();
    double yMax();
    double seconds();
    void autoScale(bool);
    void yMin(double);
    void yMax(double);
    void seconds(double);
    void clear();
    void setMask(uint32_t mask);

public slots:
    void newData(std::string *, std::string *, double);
    void toggle_mask_bit(int);
    void new_scaling(std::string *, bool, double, double);

private:
    void _display_data();
    void _align_scales();
    void _reload_y(int line);

    int    _lines;
    int    _plot_size;
    bool   _auto_scale;
    double _min_y;
    double _max_y;
    double _actual_min_y;
    double _actual_max_y;

    std::bitset<32> _mask;

    std::string _display_source;

    std::vector<QwtPlotCurve *>       _curve;
    QwtPlotCurve                      **_measpwr;
    std::vector<bool>                 _new_curve;
    std::vector<double>               _x;
    std::vector<std::vector<double> > _y;
    std::vector<boost::circular_buffer<double> > _y_history;

    ConfigData _conf;
};

#endif
