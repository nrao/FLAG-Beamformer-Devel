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
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "FakePulsarFile.h"

using namespace::std;

FakePulsarFile::FakePulsarFile(const char *path) :
    fileNumSamples(0)
  , fileNumChans(0)  
{
    filename = string(path);
}

// reads the file, and converts contents into data format
bool FakePulsarFile::parse()
{
    if(!readFile())
        return false;

    char delim = ' ';
    int di = 0;
    
    for (vector<string>::iterator it=fileLines.begin(); it != fileLines.end(); it++)
    {
        vector<float> lineData;
        data.push_back(lineData);
        string s = *it;
        vector<string> elems;
        stringstream ss(s);
        string item;
        while (getline(ss, item, delim)) {
            if (string::npos==item.find(' ')) //&& (item.compare(string(''))==0)
            {
                try {
                    //printf("%f\n", atof(item.c_str()));
                    data[di].push_back(atof(item.c_str()));
                } catch (...) {
                    printf("bad conversion: %s\n", item.c_str());
                }
            }    
            elems.push_back(item);
        }
        di++;
    }    

    // TBF: all samples have same number of channels?
    if (di > 0)
        fileNumChans = data[0].size();

    printf("some data: %f, %f, %f\n", data[0][0], data[0][1], data[1][0]);
    return true;    
        
}

// http://stackoverflow.com/questions/236129/split-a-string-in-c
// TBF: can't get these functions to link!
/*
vector<string> FakePulsarFile::splitThat(const string &s, char delim) {
    vector<string> elems;
    splitThis(s, delim, elems);
    return elems;
}

vector<string> &splitThis(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
*/

// Simply gets the contents of the file into memory
bool FakePulsarFile::readFile()
{
    string line;
    printf("parsing file\n");
    ifstream f;
    f.open(filename, ios::out);
    if (f.is_open()) {
        while (getline(f, line))
        {
            //printf(line.c_str());
            fileLines.push_back(line);
        }
    } else {
        return false;
    }
    // each line should be a sample
    fileNumSamples = fileLines.size();
    f.close();
    return true;
}
