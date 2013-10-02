/*******************************************************************
 *  MustangPanel.cc - implementation of the dm application.
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
 *  $Id: MustangPanel.cc,v 1.3 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#include "VegasBankPanel.h"
#include "data_plot.h"
#include "strip_options.h"
#include "simulated_data_source.h"
#include "device_client_data_source.h"
#include "katcp_data_source.h"
#include "DeviceClientMap.h"
#include "hist_options.h"

#include <QPushButton>
#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTabBar>
#include <QTabWidget>
#include <QStringList>
#include <QBrush>
#include <QGroupBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QStatusBar>
#include <QLabel>
#include <QScrollArea>
#include <QSpacerItem>

#include <iostream>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;

VegasBankPanel::VegasBankPanel(ConfigData &d, QString source, QWidget *parent)
    : QWidget(parent),
      _current_column(0),
      _update_iteration(1),
      _measpwr_mask(3),
      _conf(d)
{
    QString title;

    _data_colors.push_back(Qt::darkCyan);
    _data_colors.push_back(Qt::red);

    _current_device = _conf.devices[0];

    setWindowTitle(tr("VEGAS Bank Data Monitor"));

    // Data source:
    if (source == "simulator")
    {
        cout << "Setting up simulated data source" << endl;
        _ds.reset(new SimulatedDataSource(_conf.devices, 1000, this));
    }
    else if (source == "accessor")
    {
        cout << "Setting up GBT M&C Accessor data source" << endl;
        _ds.reset(new DeviceClientDataSource(_conf.devices, this));
    }
    else if (source == "katcp")
    {
        cout << "Setting up KATCP data source" << endl;
        _ds.reset(new KATCPDataSource(_conf.devices, this));
    }

    connect(_ds.get(), SIGNAL(error(int, QString *)), SLOT(dataSourceError(int, QString *)));
    _ds->start();

    // Status bar
    QStatusBar *statusBar = new QStatusBar();
    connect(this, SIGNAL(setStatusBar(const QString &, int)),
            statusBar, SLOT(showMessage(const QString &, int)));
    connect(this, SIGNAL(clearStatusBar()), statusBar, SLOT(clearMessage()));

    //////////////////////////////////////////////////////////////////////
    // Main screen layout:

    // Tabs, to be shown along the top:
    QTabWidget *tab = new QTabWidget();

    // Add a Bank page for each device tab:
    for (vector<string>::iterator i = _conf.devices.begin(); i != _conf.devices.end(); ++i)
    {
        QWidget *page = new QWidget();
        page->setLayout(_make_backend_layout(*i));
        tab->addTab(page, i->c_str());
    }

    // Now create a layout for the "All" tab, and create that tab:
    QGridLayout *all_layout = new QGridLayout;

    for (int i = 0; i != (int)_conf.devices.size(); ++i)
    {
        DataPlot *plot = new DataPlot(_conf.devices[i], _conf, &_data_colors);
        plot->setTitle(tr(_conf.devices[i].c_str()));
        connect(_ds.get(), SIGNAL(new_measpwr_data(std::string *, std::string *, double)),
                plot, SLOT(newData(std::string *, std::string *, double)));
        int col = i % 2;
        int row = i / 2;
        all_layout->addWidget(plot, row, col);
    }

    QWidget *page = new QWidget();
    page->setLayout(all_layout);
    tab->addTab(page, "All measpwr");

    // just so we know when a new tab is selected
    connect(tab, SIGNAL(currentChanged(int)), this, SLOT(newTabSelected(int)));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(tab);
    layout->addWidget(statusBar);
    setLayout(layout);
}

/********************************************************************
 * VegasBankPanel::_make_strip_chart(string device)
 *
 * Lays out a strip chart & controls.
 *
 * @param Name of the device to plot
 *
 * @return QHboxLayout *, a layout of the strip-chart & controls.
 *
 *******************************************************************/

QHBoxLayout *VegasBankPanel::_make_strip_chart(string device)
{
    DataPlot *plot = new DataPlot(device, _conf, &_data_colors);
    connect(_ds.get(), SIGNAL(new_measpwr_data(std::string *, std::string *, double)),
            plot, SLOT(newData(std::string *, std::string *, double)));

    // add checkboxes group
    QButtonGroup *measpwr_group;
    QHBoxLayout *plot_layout = new QHBoxLayout;
    QGroupBox *measpwrButtonBox = _button_box("measpwr", measpwr_group, _measpwr_mask);
    QHBoxLayout *group_box_layout = new QHBoxLayout;
    group_box_layout->addWidget(measpwrButtonBox);
    connect(measpwr_group, SIGNAL(buttonClicked(int)), plot, SLOT(toggle_mask_bit(int)));

    // The strip chart + controls layout
    QVBoxLayout *control_layout = new QVBoxLayout;
    control_layout->addLayout(group_box_layout);

    string pbtitle = "&measpwr options";
    QPushButton *strip_chart_ctrls = new QPushButton(tr(pbtitle.c_str()));
    control_layout->addWidget(strip_chart_ctrls);
    connect(strip_chart_ctrls, SIGNAL(clicked()), this, SLOT(strip_chart_options()));

    plot_layout->addLayout(control_layout);
    plot_layout->addWidget(plot);
    _dp[device] = plot;
    return plot_layout;
}

/********************************************************************
 * VegasBankPanel::_make_backend_layout(std::string device)
 *
 * This creates a layout for a single bank display. This consists of a
 * strip chart and its controls on the top half of the layout, and two
 * histograms and their controls on the bottom half of the layout.
 *
 * @param device: The VEGAS subdevice that this represents.
 *
 * @return QVBoxLayout *: the layout for single bank data.
 *
 *******************************************************************/

QVBoxLayout *VegasBankPanel::_make_backend_layout(std::string device)
{
    QHBoxLayout *plot = _make_strip_chart(device);

    // ADC Histograms
    QVBoxLayout *hist1 = _make_histogram(device, "adcpwr&1", _data_colors[0]);
    QVBoxLayout *hist2 = _make_histogram(device, "adcpwr&2", _data_colors[1]);
    QHBoxLayout *adc_layout = new QHBoxLayout;
    adc_layout->addLayout(hist1);
    adc_layout->addLayout(hist2);

    QVBoxLayout *sublayout = new QVBoxLayout;
    sublayout->addLayout(plot, 4);
    sublayout->addLayout(adc_layout, 5);

    return sublayout;
}

/********************************************************************
 * VegasBankPanel::_button_box(char *title, QButtonGroup *&bg, plot_mask &pm)
 *
 * This helper function sets up the measpwr plot display check-box
 * group.  These checkboxes are used to select which lines to display.
 *
 * @param std::string title: the title of the box.
 * @param QButtonGroup *&bg: the button group, used in the button group slot.
 * @param plot_mask &pm: used to determine initial check state for buttons.
 *
 * @return QGroupBox *: the completed group box.
 *
 *******************************************************************/

QGroupBox *VegasBankPanel::_button_box(string title, QButtonGroup *&bg, plot_mask &pm)

{
    int i;
    QCheckBox *cb;
    QFrame *button_frame = new QFrame(this);
    QVBoxLayout *button_layout = new QVBoxLayout;
    QVBoxLayout *group_layout = new QVBoxLayout;
    QGroupBox *group_box = new QGroupBox(title.c_str(), this);
    QScrollArea *sa = new QScrollArea(group_box);


    bg = new QButtonGroup(this);
    bg->setExclusive(false);

    for (i = 0; i < 2; ++i)
    {
        cb = new QCheckBox(tr("%1").arg(i + 1));

        if (pm[i])
        {
            cb->setCheckState(Qt::Checked);
        }

        bg->addButton(cb, i);
        button_layout->addWidget(cb);
    }

    button_frame->setLayout(button_layout);
    sa->setWidget(button_frame);
    group_layout->addWidget(sa);
    group_box->setLayout(group_layout);
    group_box->setMaximumWidth(sa->width());

    return group_box;
}

QVBoxLayout *VegasBankPanel::_make_histogram(string device, string title, const QColor &color)

{

    QVBoxLayout *vbox = new QVBoxLayout;
    vector<string> strs;
    split(strs, title, is_any_of("&"));
    string chart_title = strs[0] + strs[1];
    string options_title = title + " " + "options...";
    ADCPlot *adc;

    adc = new ADCPlot(device, chart_title, color);
    _adc[device][chart_title] = adc;
    connect(_ds.get(), SIGNAL(new_adc_data(std::string *, std::string *, int, char *)),
            adc, SLOT(newData(std::string *, std::string *, int, char *)));
    QPushButton *hist_opts = new QPushButton(tr(options_title.c_str()));
    hist_opts->setSizePolicy( QSizePolicy(QSizePolicy::Fixed,
                                          QSizePolicy::Fixed));
    connect(hist_opts, SIGNAL(clicked()), adc, SLOT(update_histogram_options()));
    vbox->addWidget(adc);
    vbox->addWidget(hist_opts);
    return vbox;
}

/********************************************************************
 * VegasBankPanel::newTabSelected(int index)
 *
 * A slot for the signal from the tab bar that a new tab has been
 * selected.
 *
 * @param int index: the index of the tab selected (0 based, from
 * left-to-right)
 *
 *******************************************************************/

void VegasBankPanel::newTabSelected(int index)

{
    if (index < (int)_conf.devices.size())
    {
        _current_device = _conf.devices[index];
    }
    else
    {
        _current_device = "All";
    }
}

/********************************************************************
 * VegasBankPanel::dataSourceError(int err, QString *msg)
 *
 * This slot is intended to be connected to the data source's error
 * signal.  It turns around and emits signals that are intended for a
 * text display widget such as a status bar.
 *
 * @param int err: the error number. 0 means no error.
 * @param QString *msg: the error message.  If NULL, or if err = 0,
 *                      there is no error message.
 *
 *******************************************************************/

void VegasBankPanel::dataSourceError(int err, QString *msg)

{
    if (err)
    {
        if (msg)
        {
            emit setStatusBar(*msg);
        }
    }
    else
    {
        emit clearStatusBar();
    }
}

/********************************************************************
 * VegasBankPanel::strip_chart_options()
 *
 * This slot function gets called when the "Strip Chart Options..."
 * pushbutton is pushed.  It opens a dialog box with all the possible
 * options, including seconds buffered, data rate, auto/manual y-axis,
 * and y-axis manual values.
 *
 *******************************************************************/

void VegasBankPanel::strip_chart_options()

{
    int r;
    OptionsDialog dlg(_current_device, this);
    OptionsDialog::options_data d;

    d.manual_scale = !_dp[_current_device]->autoScale();
    d.y_min = _dp[_current_device]->yMin();
    d.y_max = _dp[_current_device]->yMax();
    d.seconds = _dp[_current_device]->seconds();
    dlg.setData(d);
    r = dlg.exec();

    if (r)
    {
        dlg.getData(d);

        if (d.manual_scale_changed)
        {
            _dp[_current_device]->autoScale(!d.manual_scale);
        }

        if (d.y_min_changed)
        {
            _dp[_current_device]->yMin(d.y_min);
        }

        if (d.y_max_changed)
        {
            _dp[_current_device]->yMax(d.y_max);
        }

        if (d.seconds_changed)
        {
            _dp[_current_device]->seconds(d.seconds);
        }
    }
}
