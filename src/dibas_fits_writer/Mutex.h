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
//#	GBT Operations
//#	National Radio Astronomy Observatory
//#	P. O. Box 2
//#	Green Bank, WV 24944-0002 USA

#ifndef Mutex_h
#define Mutex_h
#include <pthread.h>
/// Mutex.h
/// A class for mutual exclusion in posix threads.
class Mutex
{
public:
    int lock();
    int unlock();
    Mutex();
    virtual ~Mutex();
private:
    pthread_mutex_t mutex;
};

/// An auto-unlocking mutex. While Mutex objects are necessarily visible in
/// either a global or class member scope, MutexLock's should be in a local
/// scope. If an exception occurrs or the method/function returns, the MutexLock
/// going out of scope will cause its destructor to be called, wherein the
/// contained Mutex will be unlocked, preserving the resource.
class MutexLock
{
public:
/// Creates a MutexLock. When the MutexLock destructor is called
/// the Mutex is contains will be unlocked.
/// @param mutex A Mutex object reference.
    MutexLock(Mutex &mutex) : mtx(mutex), locked(false)
    {
        mtx.lock();
        locked=true;
    }
    ~MutexLock() { unlock(); };
    void unlock()
    {
        if (locked==true)
            mtx.unlock();
    }
    void lock()
    {
        if (locked==false)
        {
            mtx.lock();
            locked=true;
        }
    }
    
private:
    Mutex &mtx;
    bool locked;
};

#endif
