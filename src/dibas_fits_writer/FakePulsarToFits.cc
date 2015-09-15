//# Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
//#
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//#
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning GBT software should be addressed as follows:
//# GBT Operations
//# National Radio Astronomy Observatory
//# P. O. Box 2
//# Green Bank, WV 24944-0002 USA

#include <stdio.h>
#include <sys/time.h>

#include "FakePulsarToFits.h"
#include "BfPulsarFitsIO.h"
#include "BfFitsIO.h"

FakePulsarToFits::FakePulsarToFits()
{

    // TBF

}

bool FakePulsarToFits::convertToFits()
{
    int i, si;
    BfPulsarFitsIO *fits;

    if(!parseFiles()) {
        printf("Parsing Files failed.");
        return false;
    }

    // TBF: all files have same number of samples?
    int numSamples = files[0]->getNumSamples();
    int numChannels = files[0]->getNumSamples();

    // TBF: call the function that will convert this parsed data to
    // the format and freq. space needed for each fits file
    // For now create some fake data; notice we aren't using numChannels above yet
    int dataSize = NUM_BEAMS * NUM_PULSAR_CHANNELS;
    float tempData[dataSize];
    for (i=0; i<dataSize; i++)
        tempData[i] = (float)i;

    // init the FITS writers
    for (i=0; i < numBeams; i++)
    {
        //fitsio.push_back(new BfPulsarFitsIO("/tmp", false));
        fits = new BfPulsarFitsIO("/tmp", false, i);

        // TBF: how to specify start time otherwise?
        double start_time = 0;
        timeval tv;
        gettimeofday(&tv, 0);
        // Get the current time as an MJD for use in the FITS file names
        start_time = BfFitsIO::timeval_2_mjd(&tv);
        fits->set_startTime(start_time);

        // TBF: set bank!
        //fitsio->setBankName();

        fits->open();

        // write each sample to disk
        for (si=0; si<numSamples; si++)
        {
            // TBF: mcnt == sample counter?
            fits->write(si, tempData);
        }

        fits->close();

        // we may need these objects later?
        fitsio.push_back(fits);
    }
    return true;
}

bool FakePulsarToFits::addFiles(const char* dir)
{
    // TBF: find all files in a dir, assuming a naming convention and add them
    return true;
}

bool FakePulsarToFits::addFile(const char* path)
{
    files.push_back(new FakePulsarFile(path));
    return true;
}

bool FakePulsarToFits::parseFiles()
{
    for(vector<FakePulsarFile *>::iterator it = files.begin(); it < files.end(); it++)
    {
        if(!(*it)->parse())
            return false;
    }
    return true;
}
