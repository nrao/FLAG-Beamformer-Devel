/*******************************************************************
 *  data_source.cc -- implements the common parts of the DataSource
 *  class, like error handling.
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
 *  $Id: data_source.cc,v 1.1 2009/03/24 20:14:20 rcreager Exp $
 *
 *******************************************************************/

#include "data_source.h"

struct DataSource::ds_impl

{
    ds_impl();

    bool connection_refused;
    bool no_data;

    bool errors_set();
    void clear_errors();
};


DataSource::ds_impl::ds_impl()
    : connection_refused(false),
      no_data(false)

{
}

bool DataSource::ds_impl::errors_set()

{
    return connection_refused || no_data;
}

void DataSource::ds_impl::clear_errors()

{
    connection_refused = false;
    no_data = false;
}

DataSource::DataSource(QObject *parent)
    : QObject(parent)

{
    _impl = new ds_impl;
}

DataSource::~DataSource()

{
    delete _impl;
}

/********************************************************************
 * void DataSource::assert_error(int errno, QString *errmsg)
 *
 * Sends a signal if 'errno' is asserted for the first time since
 * being cleared.
 *
 * @param int errno: the error number
 * @param QString *errmsg: If not NULL, a description of the error.
 *
 *******************************************************************/

void DataSource::assert_error(int err, QString *errmsg)

{
    switch(err)
    {
        case SUCCESS:
            if (_impl->errors_set())
            {
                _impl->clear_errors();
                emit error(0, NULL);
            }

            break;

        case CONNECTION_REFUSED:
            if (!_impl->connection_refused)
            {
                _impl->connection_refused = true;
                emit error(CONNECTION_REFUSED, errmsg);
            }

            break;

        case NO_DATA:
            if (!_impl->no_data)
            {
                _impl->no_data = true;
                emit error(NO_DATA, errmsg);
            }

            break;
    };
}

