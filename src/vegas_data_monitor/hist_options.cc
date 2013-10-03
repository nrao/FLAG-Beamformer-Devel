/*******************************************************************
 *  hist_options.cc -- implements a histogram options dialog box that
 *  includes things like x-min and x-max & bin-size options.
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

#include "hist_options.h"

#include <QtGui>

#include <iostream>

using namespace std;



HistOptionsDialog::HistOptionsDialog(string title, QWidget *parent)
    : QDialog(parent)
{
    _x_min_label = new QLabel(tr("X m&in val:"));
    _x_max_label = new QLabel(tr("X m&ax val:"));
    _bin_label = new QLabel(tr("&Bin size:"));
    _x_min = new QLineEdit;
    _x_max = new QLineEdit;
    _bin_size = new QLineEdit;

    _x_min_label->setBuddy(_x_min);
    _x_max_label->setBuddy(_x_max);
    _bin_label->setBuddy(_bin_size);

    QGridLayout *x_range_layout = new QGridLayout;
    x_range_layout->addWidget(_x_min_label, 1, 0);
    x_range_layout->addWidget(_x_min, 1, 1);
    x_range_layout->addWidget(_x_max_label, 2, 0);
    x_range_layout->addWidget(_x_max, 2, 1);
    x_range_layout->addWidget(_bin_label, 3, 0);
    x_range_layout->addWidget(_bin_size, 3, 1);

    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *main_layout = new QVBoxLayout;
    main_layout->addLayout(x_range_layout);
    main_layout->addWidget(button_box);

    setLayout(main_layout);
    setWindowTitle(tr(title.c_str()));
}

/********************************************************************
 * HistOptionsDialog::setData(HistOptionsDialog::options_data const &d)
 *
 * Sets the data to be used in the dialog box.
 *
 * @param HistOptionsDialog::options_data const &d: A reference to
 * existing data used to populate the controls.
 *
 *******************************************************************/

void HistOptionsDialog::setData(HistOptionsDialog::options_data const &d)

{
    _dlg_data = d;
    _x_min->setText(QString("%1").arg(d.x_min));
    _x_max->setText(QString("%1").arg(d.x_max));
    _bin_size->setText(QString("%1").arg(d.bin_size));
}

/********************************************************************
 * HistOptionsDialog::getData(HistOptionsDialog::options_data &d)
 *
 * Obtains the user modified data from the dialog box object.
 *
 * @param HistOptionsDialog::options_data &d: The data buffer used to
 * return the data to the caller.
 *
 *******************************************************************/

void HistOptionsDialog::getData(HistOptionsDialog::options_data &d)

{
    _dlg_data.x_min_changed = _x_min->isModified();

    if (_dlg_data.x_min_changed)
    {
        _dlg_data.x_min = _x_min->text().toDouble();
    }

    _dlg_data.x_max_changed = _x_max->isModified();

    if (_dlg_data.x_max_changed)
    {
        _dlg_data.x_max = _x_max->text().toDouble();
    }

    _dlg_data.bin_size_changed = _bin_size->isModified();

    if (_dlg_data.bin_size_changed)
    {
        _dlg_data.bin_size = _bin_size->text().toInt();
    }

    d = _dlg_data;
}
