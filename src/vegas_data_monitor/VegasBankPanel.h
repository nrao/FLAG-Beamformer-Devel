/*******************************************************************
 *  VegasBankPanel.h - A display for the Vegas Bank adcsnap samplers,
 *  and whatever  else.
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

#if !defined(_VEGAS_BANK_PANEL_H_)
#define _VEGAS_BANK_PANEL_H_

#include <QWidget>
#include <bitset>
#include <string>
#include <map>
#include <boost/shared_ptr.hpp>
#include <stdint.h>

#include "ConfigData.h"
#include "adchist.h"

class QPushButton;
class QString;
class QTabBar;
class QTableWidget;
class QGroupBox;
class QButtonGroup;
class QFrame;
class DataSource;
class DataPlot;
class QVBoxLayout;
class QHBoxLayout;

typedef std::bitset<32> plot_mask;

class VegasBankPanel : public QWidget

{
    Q_OBJECT

  public:

    VegasBankPanel(ConfigData &d, QString source, QWidget *parent = 0);

  public slots:
    void dataSourceError(int, QString *);
    void newTabSelected(int index);
    void strip_chart_options();

  signals:
    void autoScale(std::string *, bool, double = 0.0, double = 0.0);
    void setStatusBar(const QString &, int = 0);
    void clearStatusBar();

  private:

    QGroupBox *_button_box(std::string title, QButtonGroup *&bg, plot_mask &pm);
    QHBoxLayout *_make_strip_chart(std::string device);
    QVBoxLayout *_make_histogram(std::string device, std::string title, const QColor &color);
    QVBoxLayout * _make_backend_layout(std::string backend);

    int _current_column;
    int _update_iteration;
    int _data_rate;
    int _data_rows;

    QTabBar *tab;
    std::string _current_device;
    plot_mask _measpwr_mask;

    boost::shared_ptr<DataSource> _ds;
    std::map<std::string, DataPlot *> _dp;
    std::map<std::string, std::map<std::string, ADCPlot *> > _adc;

    ConfigData _conf;
    std::vector<Qt::GlobalColor> _data_colors;

};

#endif // _MUSTANG_PANEL_H_
