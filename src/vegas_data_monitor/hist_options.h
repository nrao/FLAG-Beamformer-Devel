/*******************************************************************
 *  hist_options.h -- declares a custom dialog class that provides to
 *  the user options for the data monitoring histograms.
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

 #ifndef _HIST_OPTIONS_H_
 #define _HIST_OPTIONS_H_

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QButtonGroup;

class HistOptionsDialog : public QDialog
{
    Q_OBJECT

  public:

    struct options_data
    {
        options_data() :
            x_min(0.0),
            x_max(0.0),
            bin_size(1),
            x_min_changed(false),
            x_max_changed(false),
            bin_size_changed(false)

        {}

        double x_min;
        double x_max;
        int bin_size;
        bool x_min_changed;
        bool x_max_changed;
        bool bin_size_changed;
    };

    HistOptionsDialog(std::string title, QWidget *parent = 0);

    void setData(options_data const &d);
    void getData(options_data &d);

  private:

    QLabel *_x_min_label;
    QLabel *_x_max_label;
    QLabel *_bin_label;
    QLineEdit *_x_min;
    QLineEdit *_x_max;
    QLineEdit *_bin_size;
    QWidget *_extension;
    options_data _dlg_data;
};

#endif // _HIST_OPTIONS_H_
