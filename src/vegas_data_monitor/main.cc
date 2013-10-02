/*******************************************************************
 *  Experimental Mustang data monitoring application.  Uses QT4.
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
 *  $Id: main.cc,v 1.2 2009/12/07 14:31:30 rcreager Exp $
 *
 *******************************************************************/

#include <QApplication>
#include <QString>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <string>

#include "VegasBankPanel.h"
#include "ConfigData.h"
#include "ConfigFile.h"

using namespace std;

bool _get_config(ConfigData &d);
void _get_default_config(ConfigData &d);

int main(int argc, char *argv[])
{
    ConfigData d;

    if (!_get_config(d))
    {
        cout << "WARNING: No configuration file found in '.' or '~' or $YGOR_TELESCOPE="
             << getenv("YGOR_TELESCOPE") << ". Using default configuration 8x8." << endl;
        _get_default_config(d);
    }

    QString source(d.source.c_str());
    QApplication app(argc, argv);
    VegasBankPanel panel(d, source);
    panel.setGeometry(100, 100, 970, 850);
    panel.show();
    return app.exec();
}

/********************************************************************
 * _get_config(ConfigData &d)
 *
 * Retrieves a configuration from a user config file.  The function
 * first looks in the current directory for mustangdm.conf, then in
 * the user's home directory for .mustangdm
 *
 * @param ConfigData &d: The location that the configuration data will
 * be stored.
 *
 * @return bool, true if it found a file and configured successfully,
 * false if not.
 *
 *******************************************************************/

bool _get_config(ConfigData &d)

{
    ConfigFile cf;
    String fn;


    try
    {
        fn = "./vegasdm.conf";
        cout << "Trying config file: " << fn << endl;
        cf.Load(fn);
    }
    catch (ConfigFile::Exception e)
    {
        try
        {
            fn = getenv("HOME");
            fn += "/.vegasdm";
            cout << "Trying config file: " << fn << endl;
            cf.Load(fn);
        }
        catch (ConfigFile::Exception e)
        {

            fn = getenv("YGOR_TELESCOPE");

            if (fn.empty())
            {
                fn = getenv("DIBAS_DIR");
            }

            try
            {
                fn += "/etc/config/vegasdm.conf";
                cout << "Trying config file: " << fn << endl;
                cf.Load(fn);
            }
            catch (ConfigFile::Exception e)
            {
                cerr << "_get_config() encountered an exception: " << e.what() << endl;
                return false;
            }
        }
    }

    try
    {
        vector<int> sub_sys;

        cf.SetCurrentSection("Data");
        cf.Get("Source", d.source);
        cf.Get("subsystems", sub_sys);
        string subdeviceA = "BankAMgr";

        if (d.source == "accessor")
        {
            for (vector<int>::iterator i = sub_sys.begin(); i != sub_sys.end(); ++i)
            {
                string subdev = subdeviceA;
                subdev[4] += *i - 1;
                d.devices.push_back(subdev);
            }
        }
        else if (d.source == "katcp") // katcp we want 'vegasr2-1,
                                      // vegasr2-2' etc.
        {
            for (vector<int>::iterator i = sub_sys.begin(); i != sub_sys.end(); ++i)
            {
                char buf[12];

                snprintf(buf, 12, "vegasr2-%i", *i);
                d.devices.push_back(buf);
            }
        }


        cf.SetCurrentSection("StripChartOptions");
        cf.Get("ManualScaling", d.manual_scaling);
        cf.Get("YMinVal", d.ymin);
        cf.Get("YMaxVal", d.ymax);
        cf.Get("seconds", d.seconds);
        cf.Get("Lines", d.lines);
        return true;
    }
    catch (ConfigFile::Exception e)
    {
        cerr << "_get_config() encountered an exception: " << e.what() << endl;
    }

    return false;
}

/********************************************************************
 * _get_default_config(ConfigData &d)
 *
 * Loads the ConfigData structure with a default configuration.
 *
 * @param ConfigData &d: the configuration information goes here.
 *
 *******************************************************************/

void _get_default_config(ConfigData &d)

{
    d.source = "accessor";
    d.manual_scaling = false;
    d.ymin = 0.0;
    d.ymax = 0.0;
    d.lines = 2;
    d.devices.push_back("BankAMgr");
}
