/*******************************************************************
 *  strip_options.cc -- implements a stripchart options dialog box
 *  that includes things like y-axis options, data buffer length, etc.
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
 *  $Id: strip_options.cc,v 1.1 2009/02/27 19:38:03 rcreager Exp $
 *
 *******************************************************************/

#include "strip_options.h"

#include <QtGui>

#include <iostream>

using namespace std;



OptionsDialog::OptionsDialog(string title, QWidget *parent)
    : QDialog(parent)
{
    _y_min_label = new QLabel(tr("Y m&in val:"));
    _y_max_label = new QLabel(tr("Y m&ax val:"));
    _seconds_label = new QLabel(tr("&Seconds:"));
    _y_min = new QLineEdit;
    _y_max = new QLineEdit;
    _seconds = new QLineEdit;

    _y_min_label->setBuddy(_y_min);
    _y_max_label->setBuddy(_y_max);
    _seconds_label->setBuddy(_seconds);

    _manual_check_box = new QCheckBox(tr("&Manual Scaling"));
    connect(_manual_check_box, SIGNAL(stateChanged(int)), this, SLOT(scale_mode_changed(int)));

    QGridLayout *y_range_layout = new QGridLayout;
    y_range_layout->addWidget(_manual_check_box, 0, 0);
    y_range_layout->addWidget(_y_min_label, 1, 0);
    y_range_layout->addWidget(_y_min, 1, 1);
    y_range_layout->addWidget(_y_max_label, 2, 0);
    y_range_layout->addWidget(_y_max, 2, 1);
    QGroupBox *y_box = new QGroupBox("Y-Axis");
    y_box->setLayout(y_range_layout);

    QHBoxLayout *time_range_layout = new QHBoxLayout;
    time_range_layout->addWidget(_seconds_label);
    time_range_layout->addWidget(_seconds);
    QGroupBox *time_box = new QGroupBox("X-Axis");
    time_box->setLayout(time_range_layout);

    _button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(_button_box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *main_layout = new QVBoxLayout;
    main_layout->addWidget(y_box);
    main_layout->addWidget(time_box);
    main_layout->addWidget(_button_box);

    setLayout(main_layout);
    setWindowTitle(tr(title.c_str()));
}

/********************************************************************
 * OptionsDialog::setData(OptionsDialog::options_data const &d)
 *
 * Sets the data to be used in the dialog box.
 *
 * @param OptionsDialog::options_data const &d: A reference to
 * existing data used to populate the controls.
 *
 *******************************************************************/

void OptionsDialog::setData(OptionsDialog::options_data const &d)

{
    Qt::CheckState check;
//    QList<QAbstractButton *> bn;


//    bn = _bg->buttons();
    _dlg_data = d;
    check = d.manual_scale ? Qt::Checked : Qt::Unchecked;
    _manual_check_box->setCheckState(check);

    if (!d.manual_scale)
    {
        _y_min->setEnabled(false);
        _y_max->setEnabled(false);
    }

    _y_min->setText(QString("%1").arg(d.y_min));
    _y_max->setText(QString("%1").arg(d.y_max));
    _seconds->setText(QString("%1").arg(d.seconds));
}

/********************************************************************
 * OptionsDialog::getData(OptionsDialog::options_data &d)
 *
 * Obtains the user modified data from the dialog box object.
 *
 * @param OptionsDialog::options_data &d: The data buffer used to
 * return the data to the caller.
 *
 *******************************************************************/

void OptionsDialog::getData(OptionsDialog::options_data &d)

{
    _dlg_data.manual_scale = _manual_check_box->isChecked();
    _dlg_data.y_min_changed = _y_min->isModified();

    if (_dlg_data.y_min_changed)
    {
        _dlg_data.y_min = _y_min->text().toDouble();
    }

    _dlg_data.y_max_changed = _y_max->isModified();

    if (_dlg_data.y_max_changed)
    {
        _dlg_data.y_max = _y_max->text().toDouble();
    }

    _dlg_data.seconds_changed = _seconds->isModified();

    if (_dlg_data.seconds_changed)
    {
        _dlg_data.seconds = _seconds->text().toDouble();
    }

    d = _dlg_data;
}

/********************************************************************
 * OptionsDialog::scale_mode_changed(int state)
 *
 * Slot gets called when the manual scaling mode checkbox changes
 * state. This function determines whether the state has changed from
 * its original value.  If so, it sets a flag in the data structure.
 * The state of the checkbox may change several times during the
 * lifetime of the dialog box, but if the ending state of the checkbox
 * is what it was originally the flag will reflect no change.  This
 * function also enables and disables the QLineEdit controls.  If the
 * manual scaling checkbox is set, the edit lines are enabled to
 * accept user supplied values.
 *
 * @param int mode: the new state.
 *
 * @return int: 2 if checked, 0 if unchecked.
 *
 *******************************************************************/

void OptionsDialog::scale_mode_changed(int state)

{
    bool enabled;


    if ((!_dlg_data.manual_scale && state == 2) || (_dlg_data.manual_scale && state == 0))
    {
        _dlg_data.manual_scale_changed = true;
    }
    else
    {
        _dlg_data.manual_scale_changed = false;
    }

    enabled = (state == 2) ? true : false;
    _y_min->setEnabled(enabled);
    _y_max->setEnabled(enabled);

}
