/*******************************************************************
 *  strip_options.h -- declares a custom dialog class that provides to
 *  the user options for the data monitoring stripchart.
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
 *  $Id: strip_options.h,v 1.1 2009/02/27 19:38:04 rcreager Exp $
 *
 *******************************************************************/

 #ifndef _STRIP_OPTIONS_H_
 #define _STRIP_OPTIONS_H_

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QButtonGroup;

class OptionsDialog : public QDialog
{
    Q_OBJECT

  public:

    struct options_data
    {
        options_data() :
            manual_scale(false),
            manual_scale_changed(false),
            y_min(0.0),
            y_min_changed(false),
            y_max(0.0),
            y_max_changed(false),
            seconds(0.0),
            seconds_changed(false)
        {}

        bool manual_scale;
        bool manual_scale_changed;
        double y_min;
        bool y_min_changed;
        double y_max;
        bool y_max_changed;
        double seconds;
        bool seconds_changed;
//        bool per_second_update;
    };

    OptionsDialog(std::string title, QWidget *parent = 0);

    void setData(options_data const &d);
    void getData(options_data &d);

  public slots:

    void scale_mode_changed(int);
//    void update_button_clicked(int);

  private:

    QLabel *_y_min_label;
    QLabel *_y_max_label;
    QLabel *_seconds_label;
    QLineEdit *_y_min;
    QLineEdit *_y_max;
    QLineEdit *_seconds;
    QCheckBox *_manual_check_box;
    QDialogButtonBox *_button_box;
//    QButtonGroup *_bg;
    QWidget *_extension;
    options_data _dlg_data;
    std::string _title;
};

#endif // _STRIP_OPTIONS_H_
