/*******************************************************************
 *
 *
 *  Copyright (C) <date> Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: data_source.h,v 1.2 2009/03/24 20:09:22 rcreager Exp $
 *
 *******************************************************************/

#if !defined(_DATA_SOURCE_H_)
#define _DATA_SOURCE_H_

#include <QObject>


class DataSource : public QObject

{
    Q_OBJECT

  public:

    DataSource(QObject *parent = 0);
    virtual ~DataSource();

  public slots:

    virtual void start() = 0;
    virtual void terminate() = 0;

  signals:

    void new_adc_data(std::string *subdev, std::string *adcpwr, int sze, char *data);
    void new_measpwr_data(std::string *subdev, std::string *measpwr, double);
    void error(int err, QString *errmsg);
    void bufferLevel(const QString &);

  protected:

    enum ERRORS
    {
        SUCCESS = 0,
        CONNECTION_REFUSED,
        NO_DATA,
    };

    void assert_error(int err = 0, QString *errmsg = 0);

  private:

    struct ds_impl;

    ds_impl *_impl;
};

#endif // _DATA_SOURCE_H
