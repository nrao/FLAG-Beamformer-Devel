//# Copyright (C) 2002 Associated Universities, Inc. Washington DC, USA.
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
//#     GBT Operations
//#     National Radio Astronomy Observatory
//#     P. O. Box 2
//#     Green Bank, WV 24944-0002 USA

//# $Id: Thread.h,v 1.5 2008/02/29 15:05:48 rcreager Exp $

#ifndef THREAD_H
#define THREAD_H

#include <assert.h>
#include <pthread.h>

/// A simple wrapper for threads.
template<typename T>
class Thread
{
    typedef Thread<T> THREAD;

    /// Redirect to member function.
    static void *thread_proc(void *thread)
    {
        return reinterpret_cast<THREAD *>(thread)->run();
    }

public:

    typedef void (T::*THREADPROC)();

    /// Destructor waits for system thread to exit.
    ~Thread()
    {
        stop();
    }

    /// Constructor takes T * and T::THREADPROC, ensuring that one object-one thread.
    /// Also takes a defaulted stack size for the thread.  If the default (0) is used,
    /// thread stack size defaults to that set by the system.
    Thread(T *object_, THREADPROC proc_, size_t stacksize_ = 0)
        : id(0)
        , object(object_)
        , proc(proc_)
        , stacksize(stacksize_)
    {
    }

    /// Start the thread.  Returns 0 on success.
    int start()
    {
        int err;


        assert(0 != object);
        assert(0 != proc);
        assert(0 == id);

#if _POSIX_THREAD_ATTR_STACKSIZE
        if (stacksize && sysconf(_SC_THREAD_ATTR_STACKSIZE) > 0)
        {
            pthread_attr_t attr;

            if ((err = pthread_attr_init(&attr)) != 0)
            {
                return err;
            }

            if ((err = pthread_attr_setstacksize(&attr, stacksize)) != 0)
            {
                return err;
            }

            err = pthread_create(&id, &attr, thread_proc, this);
        }
        else
        {
            err = pthread_create(&id, 0, thread_proc, this);
        }
#else
        err = pthread_create(&id, 0, thread_proc, this);
#endif
        return err;
    }

    bool running()
    {
        return (0 != id);
    }

    void stop()
    {
        if (running())
        {
            pthread_cancel(id);
            pthread_join(id, 0);
            id = 0;
        }
    }

    void stop_without_cancel()
    {
        if (running())
        {
            pthread_join(id, 0);
            id = 0;
        }
    }

private:
    /// Redirect to the actual thread procedure.
    void *run()
    {
        (object->*proc)();
        return 0;
    }
    
    pthread_t  id;              ///< identifies the system thread
    T         *object;          ///< thread data
    THREADPROC proc;            ///< thread procedure
    size_t stacksize;           ///< user specified thread stack size
};

#endif
