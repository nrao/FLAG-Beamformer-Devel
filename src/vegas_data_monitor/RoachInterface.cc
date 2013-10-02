// Parent
#include "RoachInterface.h"
// KATCP
extern "C" {
#include "katcp.h"
#include "katpriv.h"
#include "katcl.h"
#include "netc.h"
}//extern "C"
// STL
#include <cstring>
#include <iostream>
#include <sstream>

#include "TimeStamp.h"
#include "Mutex.h"
#include "ThreadLock.h"

#include <stdio.h>
#include <boost/circular_buffer.hpp>

#define MAX_STR 80

using namespace std;
using namespace boost;

class ScopedMutex
{
private:
    pthread_mutex_t &m_lock;

public:
    ScopedMutex(pthread_mutex_t &lock)
        :
        m_lock(lock)
    {
        pthread_mutex_lock(&m_lock);
    }
    ~ScopedMutex()
    {
        pthread_mutex_unlock(&m_lock);
    }
};

static int have_katcp(struct katcp_dispatch *d)
{
    if (d && d->d_line)
    {
        return have_katcl(d->d_line);
    }

    return 0;
}

RoachInterface::RoachInterface(const char *hostname, uint16_t portnum,
                               bool simulate)
    :
    fd(0),
    kd(0),
    host(0),
    port(0),
    sim(simulate)

{
    pthread_mutex_init(&lock, 0);
#define MAXHOSTLENGTH 64
    host = new char[MAXHOSTLENGTH];
#undef MAXHOSTLENGTH
    newAddress(hostname, portnum);

    //register_katcp(kd, "!command", "description", callback);
}

RoachInterface::~RoachInterface()
{
    if(kd)
    {
        shutdown_katcp(kd);
    }
    if(fd)
    {
        close(fd);
    }
    delete [] host;
    pthread_mutex_destroy(&lock);
}

void
RoachInterface::newAddress(const char *hostname, uint16_t portnum)
{
    if(sim) return;
    ScopedMutex m(lock);
    if(kd)
    {
        shutdown_katcp(kd);
    }
    if(fd)
    {
        close(fd);
    }
    if(hostname != host)
    {
        size_t n = strlen(hostname);
        strncpy(host, hostname, n+1);
    }
    port = portnum;
    fd = net_connect(host, port, 0);
    kd = setup_katcp(fd);
}

bool
RoachInterface::arm()
{
    if(sim) return true;
    return false;
}

bool
RoachInterface::loadBof(const char *bofname)
{
  if(sim) return true;
    if(strcmp(bofname, "") == 0) return unloadBof();
    ScopedMutex m(lock);
    string cmd = "progdev";
    send_katcp(kd, KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING | KATCP_FLAG_LAST, bofname);

    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd, 10);
}

bool
RoachInterface::unloadBof()
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "progdev";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING | KATCP_FLAG_LAST, "");
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd);
}

bool
RoachInterface::getValue(const char *reg, int8_t *buffer, int32_t length,
                         uint32_t offset)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "read";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING, reg,
               KATCP_FLAG_ULONG, (unsigned long)offset,
               KATCP_FLAG_ULONG | KATCP_FLAG_LAST, (unsigned long)length);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    if(resultOk(cmd))
    {
        arg_buffer_katcp(kd, 2, buffer, length);
        //strncpy(buffer, arg_string_katcp(kd, 2), length);
        return true;
    }
    return false;
}

bool
RoachInterface::getValue(const char *reg, uint32_t &value, uint32_t offset)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "wordread";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING, reg,
               KATCP_FLAG_ULONG | KATCP_FLAG_LAST, (unsigned long)offset);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    if(resultOk(cmd))
    {
        value = (uint32_t)arg_unsigned_long_katcp(kd, 2);
        return true;
    }
    return false;
}

bool
RoachInterface::setValue(const char *reg, const int8_t *buffer, int32_t length,
                         uint32_t offset)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "write";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING, reg,
               KATCP_FLAG_ULONG, (unsigned long)offset,
               KATCP_FLAG_BUFFER | KATCP_FLAG_LAST, buffer, length);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd);
}

bool
RoachInterface::setValue(const char *reg, uint32_t value, uint32_t offset)
{
    bool rval;

    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "wordwrite";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING, reg,
               KATCP_FLAG_ULONG, (unsigned long)offset,
               KATCP_FLAG_ULONG | KATCP_FLAG_LAST, (unsigned long)value);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    rval = resultOk(cmd);
    return rval;
}

bool
RoachInterface::tapStart(const char *device, const char *reg, const char *ip,
                         int32_t port, const char *mac)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "tap-start";
    append_string_katcp(kd, KATCP_FLAG_FIRST, (char *)(string("?") + cmd).c_str());
    append_args_katcp(kd, 0, (char *)"%s", device);
    append_args_katcp(kd, 0, (char *)"%s", reg);

    if(port != -1)
    {
        append_args_katcp(kd, 0, (char *)"%s", ip);
        // 00:00:00:00:00:00
        if(mac && (strlen(mac) == 17))
        {
            append_unsigned_long_katcp(kd, 0, port);
            append_args_katcp(kd, KATCP_FLAG_LAST, (char *)"%s", mac);
        }
        else
        {
            append_unsigned_long_katcp(kd, KATCP_FLAG_LAST, (unsigned long)port);
        }
    }
    else
    {
        append_args_katcp(kd, KATCP_FLAG_LAST, (char *)"%s", ip);
    }
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd, 1);
}

bool
RoachInterface::tapStop(const char *reg)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "tap-stop";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_STRING | KATCP_FLAG_LAST, reg);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd);
}

// I2C commands
bool
RoachInterface::getI2cValue(uint8_t addr, uint32_t nbytes, uint8_t *data)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "i2c-read";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_ULONG, (unsigned long)addr,
               KATCP_FLAG_ULONG | KATCP_FLAG_LAST, (unsigned long)nbytes);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));

    if(resultOk(cmd, 0, 100000))
    {
        uint32_t n = (uint32_t)arg_unsigned_long_katcp(kd, 2);
        if(n != nbytes) return false;
        arg_buffer_katcp(kd, 3, data, nbytes);
        return true;
    }
    return false;
}

bool
RoachInterface::setI2cValue(uint8_t addr, uint32_t nbytes, const uint8_t *data)
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "i2c-write";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
               KATCP_FLAG_ULONG, (unsigned long)addr,
               KATCP_FLAG_ULONG, (unsigned long)nbytes,
               KATCP_FLAG_BUFFER | KATCP_FLAG_LAST, data, nbytes);
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));

    return resultOk(cmd, 0, 100000);
}

void
RoachInterface::setTestMode(bool simulate)
{
    sim = simulate;
    newAddress(host, port);
}

bool
RoachInterface::status()
{
    if(sim) return true;
    ScopedMutex m(lock);
    string cmd = "fpgastatus";
    send_katcp(kd,
               KATCP_FLAG_FIRST | KATCP_FLAG_STRING | KATCP_FLAG_LAST, (string("?") + cmd).c_str());
    while(flushing_katcp(kd) && (write_katcp(kd) == 0));
    return resultOk(cmd, 3);
}

// Test func
// bool
// RoachInterface::intdiv(int a, int b, int &c)
// {
//     string cmd = "intdiv";
//     ScopedMutex m(lock);
//     send_katcp(kd,
//                KATCP_FLAG_FIRST | KATCP_FLAG_STRING, (string("?") + cmd).c_str(),
//                KATCP_FLAG_ULONG, (unsigned long)a,
//                KATCP_FLAG_ULONG | KATCP_FLAG_LAST, (unsigned long)b);
//     while(flushing_katcp(kd) && (write_katcp(kd) == 0));
//     if(resultOk(cmd))
//     {
//         c = arg_unsigned_long_katcp(kd, 2);
//         return true;
//     }
//     return false;
// }
// End test func

// void
// RoachInterface::getIncoming()
// {
//     timeval timeout;
//     timeout.tv_sec = 0;
//     timeout.tv_usec = 2500;
//     do
//     {
//         fd_set fsr;
//         FD_ZERO(&fsr);
//         FD_SET(fd, &fsr);

//         int r = select(fd + 1, &fsr, NULL, NULL, &timeout);
//         // An error occurred
//         if(r < 0) return false;
//         // Timeout; no more data
//         if(r == 0) break;
//         // Some data left
//         if(FD_ISSET(fd, &fsr))
//         {
//             read_katcp(kd);
//        }
//     } while(true);
// }

bool
RoachInterface::resultOk(string cmd, uint32_t stimeout, uint32_t ustimeout)
{
    if(fd <= 0)
    {
        return false;
    }

    timeval timeout;
    timeout.tv_sec = stimeout;
    timeout.tv_usec = ustimeout;
    TimeStamp entered = getTimeOfDay();

    do
    {
        fd_set fsr;
        FD_ZERO(&fsr);
        FD_SET(fd, &fsr);

        int r = select(fd + 1, &fsr, NULL, NULL, &timeout);
        // An error occurred
        if(r < 0)
        {
            cerr << "RoachInterface::resultOk(" << cmd << ")" << endl;
            perror("select() error: resultOk() == false");
            return false;
        }

        // Timeout; no more data
        if (r == 0)
        {
            cerr << "RoachInterface::resultOk(" << cmd << "): select() timed out" << endl;
            break;
        }

        // Data!
        if(FD_ISSET(fd, &fsr))
        {
            int read_result = read_katcp(kd);

            switch (read_result)
            {
            case -1: // error by read_katcp
                cerr << "RoachInterface::resultOk(" << cmd << ")" << endl;
                perror("read_katcp() error");
                return false;
            case 0:  // Ok? More data? read_katcl() is ambiguous
                     // about success case

                // have_katcp parses a single "line", i.e. one message
                while(have_katcp(kd))
                {
                    // TBF: skip inform/request messages?
                    if(arg_inform_katcp(kd))
                    {
                        continue;
                    }

                    char *response = arg_string_katcp(kd, 0);
                    // cmd doesn't have any "?!#", but the response will so add 1
                    if(strcmp(cmd.c_str(), response+1) == 0)
                    {
                        char *result = arg_string_katcp(kd, 1);
                        bool r = strcmp(result, KATCP_OK) == 0;

                        if (r)
                        {
                            return r;  // success
                        }
                        else
                        {
                            break;     // re-enter the select() loop, maybe
                        }              // all of response not received.
                    }
                }

                break;

            case 1: // this is returned by read_katcl() in the case of
                    // EOF, whatever that means.
                cerr << "RoachInterface::resultOk(" << cmd << "): read_result == 1 -- EOF" << endl;
                return false;
            default:
                cerr << "RoachInterface::resultOk(" << cmd << "): read_result == " << read_result << endl;
                return false;

            }
        }
    }
    while(true);

    return false;
}
