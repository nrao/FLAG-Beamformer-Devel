/*******************************************************************
 ** ThreadLock.h - A template accepting any object that has a lock()
 *  and an unlock() member function.  ThreadLock assumes that the
 *  lock() and unlock() functions return 0 for success (as would the
 *  underlying pthread_mutex_(un)lock() functions).  The object
 *  will unlock() the contained object when it goes out of scope,
 *  making for exception-safe locking.
 *
 *  Copyright (C) 2004 Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: ThreadLock.h,v 1.1 2004/02/11 15:44:00 rcreager Exp $
 *
 *******************************************************************/

#if !defined(_THREADLOCK_H_)
#define _THREADLOCK_H_

#include <assert.h>

template<typename X> class ThreadLock
{
  public:

    explicit ThreadLock (X &p) :  _the_lock(p), locked(false), rval(0)  {}
    ~ThreadLock ()                                                      {unlock();}


    int lock()
    {
        if ((rval = _the_lock.lock()) == 0)
        {
            locked = true;
        }

        return rval;
    }

    int unlock()
    {
        if (locked)
        {
            if ((rval = _the_lock.unlock()) == 0)
            {
                locked = false;
            }

            return rval;
        }

        return 0;
    }

    int last_error()
    {
        return rval;
    }


  private:

    X &_the_lock;
    bool locked;
    int rval;
};

#endif

