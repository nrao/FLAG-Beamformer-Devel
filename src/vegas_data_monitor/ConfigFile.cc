/*******************************************************************
 ** module ConfigFile.h  Reads an ASCII config file with entries of
 *         the form:
 *
 *           <KEY>:=<VALUE>
 *
 *         into an map.  The values can then easily be retrieved
 *         by key name
 *
 *  Copyright (C) 2000 Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: ConfigFile.cc,v 1.9 2011/08/18 14:41:25 mwhitehe Exp $
 *
 *******************************************************************/

#if !defined(VXWORKS)  // uses STL

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <set>
#include <algorithm>

// BOOST
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include "ConfigFile.h"
#include "NumericConversions.h"

using namespace std;

/********************************************************************
 ** ConfigFile::ConfigFile()
 *
 *  @mfunc Creates an object from the configuration list in the specified
 *         configuration file.
 *
 *  @parm const char * | fn | configuration file name.
 *
 *******************************************************************/


ConfigFile::ConfigFile()

{
}

/********************************************************************
 ** ConfigFile::ConfigFile(const string &fn)
 *
 *  @mfunc Creates an object from the configuration list in the specified
 *         configuration file.
 *
 *  @parm const string & | fn | configuration file name.
 *
 *******************************************************************/


ConfigFile::ConfigFile(string const &fn)

{
    if (get_entries(fn, sm))
    {
        config_file_name = fn;
    }
    else
    {
        Exception e;
        char buf[301];

        snprintf(buf, 300, "ConfigFile::ConfigFile(): Unable to open configuration file %s",
                 fn.c_str());
        e.what(buf);

        throw e;
    }

}

/********************************************************************
 ** ConfigFile::~ConfigFile()
 *
 *  @mfunc Destructor.  Does nothing, but base class destructor is
 *         virtual so we must have this.
 *
 *******************************************************************/

ConfigFile::~ConfigFile()

{
}

/********************************************************************
 ** ConfigFile::Load(const string &fn)
 *
 *  @mfunc Loads a configuration file and stores the values in the
 *         values map 'vm'.
 *
 *  @parm const string & | fn | The filespec of the configuration file.
 *
 *******************************************************************/

void ConfigFile::Load(const string &fn)

{
    if (!get_entries(fn, sm))
    {
        _throw_exception("ConfigFile::Load(): Could not load file %s", fn.c_str());
    }

    config_file_name = fn;

}

/********************************************************************
 ** ConfigFile::Save(const string &fn)
 *
 *  @mfunc Saves a configuration file
 *
 *  @parm const string & | fn | The filespec of the configuration file.
 *  @parm const string & | comments | Optional comments for the config file.
 *
 *******************************************************************/

void ConfigFile::Save(const string &fn, const string &comments)

{
    FILE *f;
    string file_name;
    section_map::iterator smi;
    map<string, string>::iterator vmi;


    file_name = fn.empty() ? config_file_name : fn;

    if ((f = fopen(file_name.c_str(), "w")) == NULL)
    {
        _throw_exception("ConfigFile::Save(): Unable to open %s for saving",
                         file_name.c_str());
    }

    if (!comments.empty())
    {
        fprintf(f, "#\n# %s\n#\n", comments.c_str());
    }

    for (smi = sm.begin(); smi != sm.end(); ++smi)
    {
        if (smi->first != "global")
        {
            fprintf(f, "\n[%s]\n\n", smi->first.c_str());
        }

        for (vmi = smi->second.map.begin(); vmi != smi->second.map.end(); ++vmi)
        {
            fprintf(f, "%s := %s\n", vmi->first.c_str(), vmi->second.c_str());
        }
    }

    fclose(f);
    config_file_name = file_name;
}

/********************************************************************
 ** ConfigFile::Clear()
 *
 *  @mfunc Clears out all of the entries from the map.
 *
 *******************************************************************/

void ConfigFile::Clear()

{
    if (!sm.empty())
    {
        sm.erase(sm.begin(), sm.end());
    }

}


/********************************************************************
 ** ConfigFile::Find(const string &key,  int &v)
 *
 *  @mfunc Retrieves the value of 'key' as an unsigned int
 *  returns true if found, else returns false
 *
 *  @parm const string & | key | Name of the value
 *  @parm int & | v | The value
 *
 *******************************************************************/
bool ConfigFile::Find(const string &key, unsigned int &v)

{
    string val;
    char *endp;

    try
    {
        get(key, val);
    }
    catch(...)
    {
        return false;
    }
    v = static_cast<int>(strtol(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }
    return true;
}

/********************************************************************
 ** ConfigFile::Get(const string &key, bool &v)
 *
 *  @mfunc Retrieves the value of 'key' as a bool.  Value in config
 *         file can be 'true' or 'false' regardless of case.
 *
 *  @parm const string & | key | Name of the value
 *  @parm bool & | v | The value
 *
 *  @rdesc 'true' if the value exists, 'false' otherwise
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, bool &v)

{
    string val;


    get(key, val);

    if (val == string("true"))
    {
        v = true;
    }
    else if (val == string("false"))
    {
        v = false;
    }
    else
    {
        _throw_exception("%s: value %s is neither 'true' or 'false'", key.c_str(), val.c_str());
    }
}

/********************************************************************
 ** ConfigFile::Get(const string &key, unsigned char &v)
 *
 *  @mfunc Retrieves the value of 'key' as an unsigned char
 *
 *  @parm const string & | key | Name of the value
 *  @parm unsigned char & | v | The value
 *
 *  @rdesc 'true' if the value exists, 'false' otherwise
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, unsigned char &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<unsigned char>(strtoul(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, char &v)
 *
 *  @mfunc Retrieves the value of 'key' as a char
 *
 *  @parm const string & | key | Name of the value
 *  @parm char & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, char &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<char>(strtol(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, unsigned short &v)
 *
 *  @mfunc Retrieves the value of 'key' as an unsigned short
 *
 *  @parm const string & | key | Name of the value
 *  @parm unsigned short & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, unsigned short &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<unsigned short>(strtoul(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, short &v)
 *
 *  @mfunc Retrieves the value of 'key' as a short
 *
 *  @parm const string & | key | Name of the value
 *  @parm short & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, short &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<short>(strtol(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, unsigned int &v)
 *
 *  @mfunc Retrieves the value of 'key' as an unsigned int
 *
 *  @parm const string & | key | Name of the value
 *  @parm unsigned int & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, unsigned int &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<unsigned int>(strtoul(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, int &v)
 *
 *  @mfunc Retrieves the value of 'key' as an int
 *
 *  @parm const string & | key | Name of the value
 *  @parm int & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, int &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<int>(strtol(val.c_str(), &endp, 0));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, unsigned long &v)
 *
 *  @mfunc Retrieves the value of 'key' as an unsigned long
 *
 *  @parm const string & | key | Name of the value
 *  @parm unsigned long & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, unsigned long &v)

{
    string val;
    char *endp;


    get(key, val);
    v = strtoul(val.c_str(), &endp, 0);

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, long &v)
 *
 *  @mfunc Retrieves the value of 'key' as a long
 *
 *  @parm const string & | key | Name of the value
 *  @parm long & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, long &v)

{
    string val;
    char *endp;


    get(key, val);
    v = strtol(val.c_str(), &endp, 0);

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, float &v)
 *
 *  @mfunc Retrieves the value of 'key' as a float.
 *
 *  @parm const string & | key | Name of the value
 *  @parm float & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, float &v)

{
    string val;
    char *endp;


    get(key, val);
    v = static_cast<float>(strtod(val.c_str(), &endp));

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, double &v)
 *
 *  @mfunc Retrieves the value of 'key' as a double
 *
 *  @parm const string & | key | Name of the value
 *  @parm double & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, double &v)

{
    string val;
    char *endp;


    get(key, val);
    v = strtod(val.c_str(), &endp);

    if (*endp != '\0' && !isspace(*endp))
    {
        _throw_non_numeric(key, val);
    }

}

/********************************************************************
 ** ConfigFile::Get(const string &key, string &v)
 *
 *  @mfunc Retrieves the value of 'key' as a string
 *
 *  @parm const string & | key | Name of the value
 *  @parm string & | v | The value
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, string &v)

{
    get(key, v);

}

/********************************************************************
 ** ConfigFile::Get(const string &key, vector<string> &vv)
 *
 *  @mfunc Retrieves the value of 'key' as a vector of strings.  The
 *         strings should be separated by commas (spaces optional).
 *
 *  @parm const string & | key | Name of the value
 *  @parm vector<string> & | vv | The value list
 *  @parm char | delim | string list delimiter (defaults to ',')
 *
 *******************************************************************/

void ConfigFile::Get(const string &key, vector<string> &vv, string delim)

{
    string val;
    string v;

    // TBF: get() expects string. We need std::string.
    get(key, val);
    v = val.c_str();
    vv.clear();
    boost::split(vv, v, boost::is_any_of(delim));
    // trim any whitespaces
    for_each (vv.begin(), vv.end(),
              boost::bind(&boost::trim<std::string>, _1, std::locale()));
 }

/********************************************************************
 * ConfigFile::Get(const string &key, vector<int> &vv)
 *
 * Returns a vector of integer values based on the specifier string
 * value that is associated with the key. The string specifier can
 * take the following forms:
 *
 *            1: single index, simplest form.
 *      1,2,4,8: A list of indexes
 *         0-31: A range of indexes
 *  1,2,4,16-32: A combination of possibilities.
 *
 * The first thing the routine will do is split along the commas.
 * This will give it a list of indexes and possible ranges.  Next, it
 * will check to see if any of the elements is a range, and expand
 * those.  Finally it will return the finished list.
 *
 * @param string const &key: the key
 * @param vector<int> &vv: The finished vector
 *
 *******************************************************************/
void ConfigFile::Get(const string &key, vector<int> &vv)

{
    int j(0), lr, hr;
    //unsigned int j(0), lr, hr;
    size_t pos;
    vector<string> elements, ranges;
    vector<string>::iterator index;
    string index_spec;
    set<int> mc;
    char *tailptr;


    get(key, index_spec);

    vv.clear();

    boost::split(elements, index_spec, boost::is_any_of(","));

    for (index = elements.begin(); index != elements.end(); ++index)
    {
        if (index->find("-", pos))
        {
            boost::split(ranges, *index, boost::is_any_of("-"));

            if (ranges.size() != 2)
            {
                _throw_exception("%s: Range specifier contains more than two range elements in '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            lr = (int)strtol(ranges[0].c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not convert beginning of range '%s' in '%s'",
                                 index_spec.c_str(), ranges[0].c_str(), index->c_str());
            }

            hr = (int)strtol(ranges[1].c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not convert end of range '%s' in '%s'",
                                 index_spec.c_str(), ranges[1].c_str(), index->c_str());
            }

            if (lr >= hr)
            {
                _throw_exception("%s: Incorrect range: low >= high, in '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            for (j = lr; j <= hr; ++j)
            {
                if (mc.find(j) == mc.end())
                {
                    vv.push_back(j);
                    mc.insert(j);
                }
                else
                {
                    _throw_exception("%s: Element '%u' is duplicated in index specifier",
                                     index_spec.c_str(), j);
                }
            }
        }
        else
        {
            lr = (int)strtol(index->c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not parse '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            if (mc.find(lr) == mc.end())
            {
                vv.push_back(lr);
                mc.insert(lr);
            }
            else
            {
                _throw_exception("%s: Element '%u' is duplicated in index specifier",
                                 index_spec.c_str(), j);
            }
        }
    }
}


void ConfigFile::Get(const string &key, vector<long> &vv)

{
    long j(0), lr, hr;
    //unsigned int j(0), lr, hr;
    size_t pos;
    vector<string> elements, ranges;
    vector<string>::iterator index;
    string index_spec;
    set<long> mc;
    char *tailptr;


    get(key, index_spec);

    vv.clear();

    boost::split(elements, index_spec, boost::is_any_of(","));


    for (index = elements.begin(); index != elements.end(); ++index)
    {
        if (index->find("-", pos))
        {
            boost::split(ranges, *index, boost::is_any_of("-"));

            if (ranges.size() != 2)
            {
                _throw_exception("%s: Range specifier contains more than two range elements in '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            lr = strtol(ranges[0].c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not convert beginning of range '%s' in '%s'",
                                 index_spec.c_str(), ranges[0].c_str(), index->c_str());
            }

            hr = strtol(ranges[1].c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not convert end of range '%s' in '%s'",
                                 index_spec.c_str(), ranges[1].c_str(), index->c_str());
            }

            if (lr >= hr)
            {
                _throw_exception("%s: Incorrect range: low >= high, in '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            for (j = lr; j <= hr; ++j)
            {
                if (mc.find(j) == mc.end())
                {
                    vv.push_back(j);
                    mc.insert(j);
                }
                else
                {
                    _throw_exception("%s: Element '%u' is duplicated in index specifier",
                                     index_spec.c_str(), j);
                }
            }
        }
        else
        {
            lr = strtol(index->c_str(), &tailptr, 10);

            if (*tailptr != 0)
            {
                _throw_exception("%s: Could not parse '%s'",
                                 index_spec.c_str(), index->c_str());
            }

            if (mc.find(lr) == mc.end())
            {
                vv.push_back(lr);
                mc.insert(lr);
            }
            else
            {
                _throw_exception("%s: Element '%u' is duplicated in index specifier",
                                 index_spec.c_str(), j);
            }
        }
    }
}


/********************************************************************
 ** ConfigFile::get(const string &key, string &val)
 *
 *  @mfunc First, attemtps to get an environment variable 'key' and
 *         the value associated with it.  This allows environment
 *         variables to override the config file.  It his fails,
 *         retrieves an iterator that points to a file_data structure
 *         based on 'key'.  This is a private function, used by all
 *         the public Get() functions.  Finally, throws an exception
 *         if 'key' is not found.
 *
 *  @parm const string & | key | Name of the value
 *  @parm file_data::iterator & | v | The value
 *
 *******************************************************************/

void ConfigFile::get(const string &key, string &val)

{
    section_map::iterator smi;
    map<string, string>::iterator vmi;


    val = getenv(key.c_str());

    if (val.empty())
    {
        if ((smi = sm.find(current_section)) != sm.end())
        {
            if ((vmi = smi->second.map.find(key)) != smi->second.map.end())
            {
                val = vmi->second;
            }
            else
            {
                _throw_exception("ConfigFile::Get(): No key \"%s\" found in "
                                 "section [%s] of configuration file \"%s\"",
                                 key.c_str(), current_section.c_str(),
                                 config_file_name.c_str());
            }
        }
        else
        {
            _throw_exception("ConfigFile::Get(): No config file loaded.");
        }
    }

}

/********************************************************************
 * ConfigFile::GetFirstSection(string &key)
 *
 ** Returns the first section header in alphabetical order, setting
 *  the ConfigFile object up to return subsequent section headers
 *  using GetNextSection.  If a config file has been read, there
 *  will always be at least one section, "global".
 *
 * @param string &key; the string object to receive the section header.
 *
 * @return 'true' if a section exists, false otherwise.
 *
 *******************************************************************/

bool ConfigFile::GetFirstSection(string &key)

{
    bool rval = false;


    if (!sm.empty())
    {
        sm_iter = sm.begin();
        key = sm_iter->first;
        ++sm_iter;
        rval = true;
    }

    return rval;

}

/********************************************************************
 * ConfigFile::GetNextSection(string &key)
 *
 ** Returns the next section in the map, if it exists.
 *
 * @param string &key; the string object to receive the section header.
 *
 * @return 'true' if a section exists, false otherwise.
 *
 *******************************************************************/

bool ConfigFile::GetNextSection(string &key)

{
    bool rval = false;


    if (sm_iter != sm.end())
    {
        key = sm_iter->first;
        ++sm_iter;
        rval = true;
    }

    return rval;

}

/********************************************************************
 * ConfigFile::GetSectionKeys(vector<string> &keys)
 *
 * Returns a vector of all the section keys.  Useful for situations
 * where a config loader may wish to iterate over similar sections, such
 * as "my_section_1", "my_section_2", etc. The loader could then look
 * for the common element in the key names and use the key if it finds it.
 *
 * @param vector<string> &keys: the returned keys.
 *
 *******************************************************************/

void ConfigFile::GetSectionKeys(vector<string> &keys)

{
    section_map::iterator i;

    keys.clear();

    for (i = sm.begin(); i != sm.end(); ++i)
    {
        keys.push_back(i->first);
    }
}

/********************************************************************
 * ConfigFile::GetCurrentSection(string &key)
 *
 ** Retrieves the name of the section currently being accessed.
 *
 * @param string &key; the string object to receive the section header.
 *
 *******************************************************************/

void ConfigFile::GetCurrentSection(string &key)

{
    if (current_section.empty())
    {
        _throw_exception("ConfigFile::GetCurrentSection(): Sections are not "
                         "being used in configuration file \"%s\"",
                         config_file_name.c_str());
    }

    key = current_section;


}

/********************************************************************
 * ConfigFile::SetCurrentSection(string const &key)
 *
 ** Sets a new section to use when getting or putting values.
 *
 * @param string const &key: The new section name to use.
 *
 *******************************************************************/

void ConfigFile::SetCurrentSection(string const &key, bool create)

{
    if (sm.find(key) == sm.end())
    {
        if (create)
        {
            value_map vm;
            sm[key] = vm;
        }
        else
        {
            _throw_exception("ConfigFile::SetCurrentSection(): Section [%s] does not "
                             " exist in configuration file \"%s\"",
                             key.c_str(), config_file_name.c_str());
        }
    }

    current_section = key;

}

/********************************************************************
 * ConfigFile::RemoveSection(string const &key)
 *
 ** Removes the section and all key/value pairs it contains.
 *
 * @param string cont &key: The name of the section
 *
 *******************************************************************/

void ConfigFile::RemoveSection(string const &key)

{
    section_map::iterator i;


    if ((i = sm.find(key)) == sm.end())
    {
        _throw_exception("ConfigFile::RemoveSection(): Section [%s] does not "
                         " exist in configuration file \"%s\"",
                         key.c_str(), config_file_name.c_str());
    }

    sm.erase(i);
}

/********************************************************************
 * ConfigFile::RemoveKey(string const &key)
 *
 ** Removes the key/value pair in the current section.
 *
 * @param string const &key: The key of the key/value pair to delete.
 *
 *******************************************************************/

void ConfigFile::RemoveKey(string const &key)

{
    section_map::iterator smi;
    map<string, string>::iterator vmi;


    if ((smi = sm.find(current_section)) != sm.end())
    {
        if ((vmi = smi->second.map.find(key)) != smi->second.map.end())
        {
            smi->second.map.erase(vmi);
        }
        else
        {
            _throw_exception("ConfigFile::RemoveKey(): No key \"%s\" found in "
                             "section [%s] of configuration file \"%s\"",
                             key.c_str(), current_section.c_str(),
                             config_file_name.c_str());
        }
    }
    else
    {
        _throw_exception("ConfigFile::RemoveKey(): No config file loaded.");
    }
}

/********************************************************************
 ** ConfigFile::GetFirst(string &, string &)
 *
 *  @mfunc Gets the first key/value pair, if it exists, and in the process
 *         sets up the container iterator for further pair retrievals
 *         using GetNext().
 *
 *  @parm string & | key | Key name of value
 *  @parm string & | val | value
 *
 *  @rdesc bool, true if there is a key/value pair, false otherwise.
 *
 *******************************************************************/

bool ConfigFile::GetFirst(string &key, string &val)

{
    section_map::iterator smi;


    if ((smi = sm.find(current_section)) != sm.end())
    {
        if (!smi->second.map.empty())
        {
            smi->second.iter = smi->second.map.begin();
            key = smi->second.iter->first;
            val = smi->second.iter->second;
            ++smi->second.iter;
            return true;
        }
    }

    return false;

}

/********************************************************************
 ** ConfigFile::GetNext(string &key, string &val)
 *
 *  @mfunc Gets the next key/value pair from the map, if it exists.
 *         This function may be called multiple times after first
 *         calling GetFirst().
 *
 *  @parm string & | key | Key name of value
 *  @parm string & | val | value.
 *
 *  @rdesc bool, true if there is a key/value pair, false otherwise.
 *
 *******************************************************************/

bool ConfigFile::GetNext(string &key, string &val)

{
    section_map::iterator smi;


    if ((smi = sm.find(current_section)) != sm.end())
    {
        if (smi->second.iter != smi->second.map.end())
        {
            key = smi->second.iter->first;
            val = smi->second.iter->second;
            ++smi->second.iter;
            return true;
        }
    }

    return false;

}

/********************************************************************
 ** ConfigFile::Put(const string &key, const string &v)
 *
 *  @mfunc Stores a new key/value pair into the map.
 *
 *  @parm const string & | key | Name of the value
 *  @parm const string & | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

bool ConfigFile::Put(const string &key, const string &v)

{
    bool new_pair = true;
    section_map::iterator smi;
    map<string, string>::iterator vmi;


    if ((smi = sm.find(current_section)) != sm.end())
    {
        if ((vmi = smi->second.map.find(key)) != smi->second.map.end())
        {
            vmi->second = v;
            new_pair = false;
        }
        else
        {
            smi->second.map[key] = v;
            smi->second.iter = smi->second.map.end();
        }
    }

    return new_pair;

}

/********************************************************************
 ** ConfigFile::Put(const string &key, vector<string> &vv, char delim)
 *
 *  @mfunc Saves the value of 'key' as a vector of strings.  The
 *         strings should be separated by a delimiter character
 *         (comma is default).
 *
 *  @parm const string & | key | Name of the value
 *  @parm const vector<string> & | vv | The value list
 *  @parm char | delim | string list delimiter (defaults to ',')
 *
 *  @rdesc 'true' if the value exists, 'false' otherwise
 *
 *******************************************************************/

bool ConfigFile::Put(const string &key, const vector<string> &vv, char delim)

{
    vector<string>::const_iterator i;
    string val("");


    for (i = vv.begin(); i != vv.end(); ++i)
    {
        if (i != vv.begin())
        {
            val += delim;
        }

        val += *i;
    }

    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::get_entries(const string &fn)
 *
 *  @mfunc This private function attempts to open the configuration
 *         file and read the contents into the object.  When reading
 *         the contents, a line in the file is assumed to be one
 *         of two types: comment (first character on line is '#')
 *         or configuration entry.  The configuration entry line is
 *         assumed to consist of two string values separated by the string
 *         ":=".  The first value is assumed to be the key for the
 *         entry, and the second is assumed to be the value associated
 *         with that key.
 *
 *  @parm const string & | fn | configuration file name.
 *
 *  @rdesc Returns 'true' if the file was found and opened, false
 *         otherwise.
 *
 *******************************************************************/

bool ConfigFile::get_entries(const string &fn, section_map &m)

{
    FILE *f;
    char *p1, *p2, *sp, *line, *pos;
    string buf(INPUT_BUF_LEN, 0);


    if ((f = fopen(fn.c_str(), "r")) == NULL)
    {
        return false;
    }

    current_section = "global";

    while (fgets((char *)buf.data(), INPUT_BUF_LEN - 1, f) != NULL)
    {
        line = (char *)buf.data();
        trim_spaces_from_head(line);

        if (line[0] == '#' || line[0] == 0x0A)
        {
            continue;
        }

        if (line[0] == '[')
        {
            if ((pos = strchr(line, ']')) != 0)
            {
                *(pos + 1) = 0;
                line = trim_char_from_head(line, '[');
                line = trim_char_from_tail(line, ']');
                current_section = line;
                continue;
            }

            continue;
        }

        if ((p1 = strtok(line, "=")) == NULL)       // Look for key name
        {                                           // If this line does not have a
            continue;                               // key/value pair, skip it.
        }

        p1 = trim_char_from_ends(p1, ':');          // just in case the line is in ConfigIO format: key := val
        p1 = trim_spaces_from_ends(p1);

        if ((p2 = strtok(NULL, "=")) == NULL)       // Look for value
        {                                           // Make sure there is one.  If not,
            continue;                               // skip the line.
        }

        if ((sp = strchr(p2, '#')) != NULL)
        {
            *sp = '\0';
        }

        p2 = trim_spaces_from_ends(p2);
        p2 = trim_char_from_ends(p2, '\'');
        p2 = trim_char_from_ends(p2, '"');

        if (p2 == '\0')                             // If, after all this, there is no
        {                                           // value, skip the line.
            continue;
        }

        m[current_section].map[p1] = p2;
    }

    fclose(f);
    return true;

}

/********************************************************************
 ** ConfigFile::next_param(char *)
 *
 *  @mfunc This private function acts essentially as the C library
 *         function strtok() does, except it uses the delimiter specified
 *         in the private data member 'delimiter' and it does not
 *         'look' into strings enclosed in double quotes, to allow
 *         strings with 'delimiter' characters to be passed intact
 *         as one unit.  For instance, if it receives the string
 *         0, 5, "error, the object is out of range", it should
 *         store 3 paramters:
 *          1- 0
 *          2- 5
 *          3- error, the object is out of range
 *         instead of:
 *          1- 0
 *          2- 5
 *          3- error
 *          4- the object is out of range
 *
 *  @parm char * | str | The parameter string to be sectioned.  As
 *        with strtok(), if this is not NULL, it is stored and used
 *        in subsequent calls where it is NULL.
 *
 *  @rdesc Returns the pointer to an individual parameter string,
 *         or NULL if the parameters string has been exhausted.
 *
 *******************************************************************/

char *ConfigFile::next_param(char *str)

{
    unsigned int i, dqf;
    size_t  len;
    char *tok;


    if (str != NULL)                // New string?
    {
        cmdb = str;                 // Yes, assign to working static pointer
    }
    else if (cmdb == NULL)          // No.  Is there a remaining string?
    {
        return NULL;                // If not, return NULL
    }

    if ((len = strlen(cmdb) + 1) == 1) // length of remaining string (including terminator)
    {
        cmdb = NULL;
        return NULL;
    }

    dqf = 0;                        // double quote flag

    for (i = 0; i < len; ++i)       // look over the entire string
    {
        if (cmdb[i] == delimiter)
        {
            if (!dqf)                       // Is it a delimiter?  If there is
            {                               // no open double quote mark
                cmdb[i] = '\0';             // then mark the delimiter with a
                tok = cmdb;                 // terminating null character (\0)
                cmdb = &cmdb[i + 1];        // and bump our source string pointer
                tok = trim_spaces_from_ends(tok);
                return tok;                 // and return the chunk of string.
            }
        }

        if (cmdb[i] == '"')
        {
            if (!dqf)                   // Is it a quote ? If there is no previous
            {                           // open quote, set the open quote flag.
                dqf = 1;
                cmdb[i] = ' ';          // Replace opening quote mark with a space
            }
            else                        // If there is a previous open quote mark, this is
            {                           // the closing quote mark so clear the flag.
                dqf = 0;
                cmdb[i] = '\0';         // Replace closing quote mark with a nul character
            }

            continue;
        }

        if (cmdb[i] == '\0')
        {
            tok = cmdb;                 // Reached the end of the string!
            cmdb = NULL;
            tok = trim_spaces_from_ends(tok);
            return tok;                 // and return the chunk of string.
        }
    }

    return NULL;

}

/********************************************************************
 ** ConfigFile::trim_spaces_from_head(char *)
 *
 *  @func Takes the provided pointer and returs a pointer to the first
 *        character in the array that is not a space.  The ASCIIZ string
 *        provided is in no way assumed to be constant.
 *
 *  @parm char * | str | The ASCIIZ string to trim
 *
 *  @rdesc A pointer to the first character that is not a space.
 *
 *******************************************************************/

char *ConfigFile::trim_spaces_from_head(char *str)

{
    char *p = str;

    while (isspace(*p))
    {
        ++p;
    }

    return p;

}

/********************************************************************
 ** ConfigFile::trim_spaces_from_tail(char *str)
 *
 *  @func Takes the provided ASCIIZ string and trims space characters
 *        which include tab and ret and new-line) from the tail end
 *        of the string.  It does this by walking in from the back
 *        until encountering a non-space character.  The last space
 *        character encountered is made into the null character to
 *        terminate the string.  The string memory is owned by the
 *        caller and is not considered constant.  It is up to the
 *        caller to free it properly.
 *
 *  @parm char * | str | The input string
 *
 *  @rdesc returns a pointer to the newly trimmed string.
 *
 *******************************************************************/

char *ConfigFile::trim_spaces_from_tail(char *str)

{
    char *sp;


    sp = strchr(str, '\0');

    do
    {
        --sp;
    }
    while (isspace(*sp));

    *(sp + 1) = '\0';
    return str;
}

/********************************************************************
 ** ConfigFile::trim_spaces_from_ends(char *str)
 *
 *  @func Trims spaces from the front and back of the provided ASCIIZ
 *        string and returns a pointer to the newly trimmed string.
 *        The string memory is the reponsibility of the caller, and
 *        is non-constant.
 *
 *  @parm char * | str | The input string
 *
 *  @rdesc returns a pointer to the newly trimmed string.
 *
 *******************************************************************/

char *ConfigFile::trim_spaces_from_ends(char *str)

{
    return trim_spaces_from_tail(trim_spaces_from_head(str));

}

/********************************************************************
 ** ConfigFile::trim_char_from_head(char *)
 *
 *  @func Takes the provided pointer and returs a pointer to the first
 *        character in the array that is not the given character.  The
 *        ASCIIZ string provided is in no way assumed to be constant.
 *
 *  @parm char * | str | The ASCIIZ string to trim
 *  @parm char | ch | The character to trim
 *
 *  @rdesc A pointer to the first character that is not the given character
 *
 *******************************************************************/

char *ConfigFile::trim_char_from_head(char *str, char ch)

{
    char *p = str;

    while (*p == ch)
    {
        ++p;
    }

    return p;

}

/********************************************************************
 ** ConfigFile::trim_char_from_tail(char *str, char ch)
 *
 *  @func Takes the provided ASCIIZ string and trims the given character
 *        from the tail end of the string.  It does this by walking in
 *        from the back until encountering a non-space character.  The
 *        last position encountered that has the given character is made
 *        into the null character to terminate the string.  The string
 *        memory is owned by the caller and is not considered constant.
 *        It is up to the caller to free it properly.
 *
 *  @parm char * | str | The input string
 *  @parm char | ch | The character to trim
 *
 *  @rdesc returns a pointer to the newly trimmed string.
 *
 *******************************************************************/

char *ConfigFile::trim_char_from_tail(char *str, char ch)

{
    char *sp;


    sp = strchr(str, '\0');

    do
    {
        --sp;
    }
    while (*sp == ch);

    *(sp + 1) = '\0';
    return str;
}

/********************************************************************
 ** ConfigFile::trim_char_from_ends(char *str, char ch)
 *
 *  @func Trims the given character from the front and back of the
 *        provided ASCIIZ string and returns a pointer to the newly
 *        trimmed string.  The string memory is the reponsibility of
 *        the caller, and is non-constant.
 *
 *  @parm char * | str | The input string
 *  @parm char | ch | The character to trim
 *
 *  @rdesc returns a pointer to the newly trimmed string.
 *
 *******************************************************************/

char *ConfigFile::trim_char_from_ends(char *str, char ch)

{
    return trim_char_from_tail(trim_char_from_head(str, ch), ch);

}

/********************************************************************
 * ConfigFile::_throw_exception(const char *fmt, ...)
 *
 ** Creates an error message, packages it into an exception and throws
 *  it.
 *
 * @param const char *fmt: The format string (uses varargs, vsprintf).
 * @param ... : whatever parameters needed to satisfy the format string.
 *
 *******************************************************************/

void ConfigFile::_throw_exception(const char *fmt, ...)

{
    int ret;
    va_list argptr;
    char buf[Exception::MSGLEN + 1];
    Exception e;


    va_start(argptr, fmt);
    ret = vsnprintf(buf, Exception::MSGLEN, fmt, argptr);
    va_end(argptr);
    e.what(buf);
    throw e;

}

/********************************************************************
 * ConfigFile::_throw_non_numeric(string const &key, string const &val)
 *
 ** Used by the Get() functions, throws an exception if the value
 *  matched to the key 'key' is not a numeric value.
 *
 * @param string const &key: The key
 * @param string const &val: The value.
 *
 *******************************************************************/

void ConfigFile::_throw_non_numeric(string const &key, string const &val)

{
    _throw_exception("ConfigFile::Get(): value \"%s\" for key \"%s\" "
                     "in section [%s] is not a numeric value",
                     val.c_str(), key.c_str(), current_section.c_str());

}

#endif // VXWORKS
