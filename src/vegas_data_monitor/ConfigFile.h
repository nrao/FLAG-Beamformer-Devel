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
 *  $Id: ConfigFile.h,v 1.8 2011/08/18 14:41:17 mwhitehe Exp $
 *
 *******************************************************************/

#if !defined(_CONFIGFILE_H_)
#define _CONFIGFILE_H_
#if !defined(VXWORKS)     // uses STL

#include <string>
#include <map>
#include <list>
#include <vector>
#include <cstdio>

class ConfigFile

{
  public:

    class Exception
    {
      public:

        enum
        {
            MSGLEN = 300
        };

        void what(char const *msg)  {strncpy(_what, msg, MSGLEN); _what[MSGLEN] = 0;}
        char const *what() const    {return _what;}

      protected:

        char _what[MSGLEN + 1];
    };

    ConfigFile();
    explicit ConfigFile(const std::string &fn);
    virtual ~ConfigFile();

    void Load(const std::string &fn);
    void Save(const std::string &fn = "", const std::string &comments = "");
    void Clear();
    char const *Filename();

    bool GetFirstSection(std::string &key);
    bool GetNextSection(std::string &key);
    void GetSectionKeys(std::vector<std::string> &keys);

    bool GetFirst(std::string &key, std::string &val);
    bool GetNext(std::string &key, std::string &val);

    void GetCurrentSection(std::string &key);
    void SetCurrentSection(std::string const &key, bool create = false);
    void RemoveSection(std::string const &key);
    void RemoveKey(std::string const &key);

    void Get(std::string const &key, bool &v);
    void Get(std::string const &key, unsigned char &v);
    void Get(std::string const &key, char &v);
    void Get(std::string const &key, unsigned short &v);
    void Get(std::string const &key, short &v);
    void Get(std::string const &key, unsigned int &v);
    void Get(std::string const &key, int &v);
    void Get(std::string const &key, unsigned long &v);
    void Get(std::string const &key, long &v);
    void Get(std::string const &key, float &v);
    void Get(std::string const &key, double &v);
    void Get(std::string const &key, std::string &v);
    void Get(std::string const &key, std::vector<std::string> &vv, std::string delim = ",");
    void Get(std::string const &key, std::vector<int> &vv);
    void Get(std::string const &key, std::vector<long> &vv);

    bool Put(std::string const &key, bool v);
    bool Put(std::string const &key, unsigned char v);
    bool Put(std::string const &key, char v);
    bool Put(std::string const &key, unsigned short v);
    bool Put(std::string const &key, short v);
    bool Put(std::string const &key, unsigned int v);
    bool Put(std::string const &key, int v);
    bool Put(std::string const &key, unsigned long v);
    bool Put(std::string const &key, long v);
    bool Put(std::string const &key, float v, const char *fmt = "%g");
    bool Put(std::string const &key, double v, const char *fmt = "%lg");
    bool Put(std::string const &key, const std::string &v);
    bool Put(std::string const &key, const std::vector<std::string> &vv, char delim = ',');

    bool Find(std::string const &key, unsigned int &v);

  private:

    enum
    {
        MAX_CONV_CHAR = 255,
        INPUT_BUF_LEN = 1000,
    };

    struct value_map
    {
        std::map<std::string, std::string>::iterator iter;
        std::map<std::string, std::string> map;
    };

    typedef std::map<std::string, value_map> section_map;

    void get(const std::string &key, std::string &val);
    bool get_entries(const std::string &fn, section_map &m);
    char *next_param(char *str);
    char *trim_spaces_from_head(char *);
    char *trim_spaces_from_tail(char *);
    char *trim_spaces_from_ends(char *);
    char *trim_char_from_head(char *str, char ch);
    char *trim_char_from_tail(char *str, char ch);
    char *trim_char_from_ends(char *str, char ch);
    void _throw_exception(const char *fmt, ...);
    void _throw_non_numeric(std::string const &key, std::string const &val);

    char *cmdb;
    char delimiter;
    section_map sm;
    section_map::iterator sm_iter;
    std::string current_section;
    std::string config_file_name;
};

/********************************************************************
 ** ConfigFile::Put(const std::string &key, bool v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from bool to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm bool | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, bool v)

{
    std::string val;

    val = v ? "true" : "false";
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, unsigned char v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from unsigned char to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm unsigned char | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, unsigned char v)

{
    return Put(key, (int)v);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, char v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from char to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm char | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, char v)

{
    return Put(key, (int)v);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, unsigned short v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from unsigned short to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm unsigned short | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, unsigned short v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%hu", v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, short v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from unsigned int to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm short | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, short v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%hi", v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, unsigned int v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from unsigned int to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm unsigned int | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, unsigned int v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%u", v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, int v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from int to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm int | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, int v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%i", v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, unsigned long v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from unsigned long to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm unsigned long | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, unsigned long v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%lu", v);
    std::string val = buf;
    return Put(key, val);
}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, long v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from long to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm long | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, long v)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, "%li", v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, float v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from float to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm float | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, float v, const char *fmt)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, fmt, v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 ** ConfigFile::Put(const std::string &key, double v)
 *
 *  @mfunc Stores a new key/value pair into the map.  The value is
 *         converted from double to std::string.
 *
 *  @parm const std::string & | key | Name of the value
 *  @parm double | v | The value
 *
 *  @rdesc 'false' if the value already exists, 'true' if it doesn't
 *         already exists.  It will be created in this case.  The
 *         value is updated in either case.
 *
 *******************************************************************/

inline bool ConfigFile::Put(const std::string &key, double v, const char *fmt)

{
    char buf[MAX_CONV_CHAR + 1];

    std::snprintf(buf, MAX_CONV_CHAR, fmt, v);
    std::string val = buf;
    return Put(key, val);

}

/********************************************************************
 * ConfigFile::Filename()
 *
 ** Returns the config file's filename.
 *
 * @return char const *, the filename.
 *
 *******************************************************************/

inline char const *ConfigFile::Filename()

{
    return config_file_name.c_str();

}

#endif // VXWORKS
#endif // _CONFIGFILE_H_
