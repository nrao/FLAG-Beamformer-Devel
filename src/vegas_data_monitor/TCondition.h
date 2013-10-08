/*******************************************************************
 ** TCondition.h - A condition variable template class useful for
 *                 simple (for now) condition variable applications.
 *  For more complex applications requiring finer grained control of the
 *  condition and the action to be taken, use ConditionVar or bare
 *  pthread_cond_t.
 *
 *  Typical use:
 *
 *      TCondition<int> tc(0);
 *
 *      // in thread A, wait for tc to become 5
 *      tc.wait(5);  // or tc.wait(5, 50000), time out after 50000 microseconds
 *
 *      // in thread B, set the condition and signal/broadcast it:
 *      tc.signal(5);  // or tc.broadcast(5)
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
 *  $Id: TCondition.h,v 1.6 2008/04/14 17:08:56 rcreager Exp $
 *
 *******************************************************************/

#ifndef _TCONDITION_H_
#define _TCONDITION_H_

#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "Mutex.h"

template <typename T> class TCondition : public Mutex

{
  public:
  
    TCondition(T const &val);
    virtual ~TCondition();

    void get_value(T &v);
    void set_value(T v);
    T &value();

    void wait(T const &s);
    bool wait(T const &s, int usecs);
    void wait_with_lock(T const &s);
    
    void signal();
    void signal(T const &s);
    void broadcast();
    void broadcast(T const &s);


  protected:

    TCondition(TCondition &);

    T _value;
    pthread_cond_t _cond;

};

/*********************************************************************
 * TCondition::TCondition(T const &val)
 *
 ** Constructor, sets the initial condition value and initializes
 *  the internal pthread_cond_t object.
 *
 * @param T const &val: The initial value, must be provided.
 *
 *********************************************************************/
 
template <typename T> TCondition<T>::TCondition(T const &val)
                                      : _value(val)

{
    pthread_cond_init(&_cond, NULL);

}

/*********************************************************************
 * TCondition::~TCondition()
 *
 ** Destroys the internal data member pthread_cond_t.
 *
 *********************************************************************/
 
template <typename T> TCondition<T>::~TCondition()

{
    pthread_cond_destroy(&_cond);

}


/********************************************************************
 * TCondition::get_value(T &v)
 *
 ** Allows a caller to get the value of the underlying condition variable.
 *
 *  @param T &v: The value buffer passed in by the caller.
 *
 *
 *******************************************************************/

template <typename T> void TCondition<T>::get_value(T &v)

{
    lock();
    v = _value;
    unlock();

}

/********************************************************************
 * TCondition<T>::value()
 *
 * Returns a reference to the value data member.
 *
 * @return A reference to _value.
 *
 *******************************************************************/

template <typename T> T &TCondition<T>::value()

{
    return _value;
}

/********************************************************************
 * TCondition::set_value(T &v)
 *
 ** Allows a caller to set the value of the underlying condition variable,
 *  without doing a broadcast/signal.
 *
 *  @param T v: The value
 *
 *
 *******************************************************************/

template <typename T> void TCondition<T>::set_value(T v)

{
    lock();
    _value = v;
    unlock();

}

/********************************************************************
 * TCondition::signal()
 *
 ** Signals that state parameter has been updated.  This releases
 *  exactly one thread waiting for the condition.  If no threads
 *  are waiting, nothing happens.  If many threads are waiting,
 *  the one that is released is not specified.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::signal()

{
    pthread_cond_signal(&_cond);
}

/********************************************************************
 * TCondition::signal()
 *
 ** Signals that state parameter has been updated.  This releases
 *  exactly one thread waiting for the condition.  If no threads
 *  are waiting, nothing happens.  If many threads are waiting,
 *  the one that is released is not specified.
 *
 *  @param T const &s: Updates the condition value and signals the
 *
 *
 *******************************************************************/

template <typename T> void TCondition<T>::signal(T const &s)

{
    lock();
    _value = s;
    pthread_cond_signal(&_cond);
    unlock();

}

/********************************************************************
 ** TCondition::broadcast()
 *
 *  @mfunc Broadcasts that state parameter has been updated.  Releases
 *         all threads waiting for this condition.  If no threads are
 *         waiting, nothing happens.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::broadcast()

{
    pthread_cond_broadcast(&_cond);
}

/********************************************************************
 ** TCondition::broadcast()
 *
 *  @mfunc Broadcasts that state parameter has been updated.  Releases
 *         all threads waiting for this condition.  If no threads are
 *         waiting, nothing happens.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::broadcast(T const &s)

{
    lock();
    _value = s;
    pthread_cond_broadcast(&_cond);
    unlock();

}

/********************************************************************
 * TCondition::wait(T const &s, int usecs)
 *
 ** Waits for a corresponding signal or broadcast.
 *
 * @param T const &s: The condition value we are waiting for.
 * @param int usecs: The timeout in microseconds.
 *
 * @return true if wait succeeded for the particular value.  false if it
 *         timed out.
 *
 *******************************************************************/

template <typename T> bool TCondition<T>::wait(T const &s, int usecs)

{
    timeval curtime;
    timespec to;
    int status;
    bool rval = true;


    gettimeofday(&curtime, 0);
    to.tv_nsec = (usecs % 1000000 + curtime.tv_usec) * 1000;
    to.tv_sec  = to.tv_nsec / 1000000000;
    to.tv_nsec -= to.tv_sec * 1000000000;
    to.tv_sec  +=  curtime.tv_sec + usecs / 1000000;
    lock();

    while (_value != s)
    {
        status = pthread_cond_timedwait(&_cond, &mutex, &to);

        if (status == ETIMEDOUT)
        {
            rval = false;
            break;
        }
    }

    unlock();
    return rval;

}

/********************************************************************
 * TCondition::wait(T const &s)
 *
 ** Waits for a corresponding signal or broadcast.
 *
 * @param T const &s: The condition value we are waiting for.
 *
 * @return true if wait succeeded for the particular value.  false if it
 *         timed out.
 *
 *******************************************************************/

template <typename T> void TCondition<T>::wait(T const &s)

{
    wait_with_lock(s);
    unlock();

}

template <typename T> void TCondition<T>::wait_with_lock(T const &s)

{
    lock();

    while (_value != s)
    {
        pthread_cond_wait(&_cond, &mutex);
    }
    // Do not unlock!
}
#endif
