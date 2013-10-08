/*******************************************************************
 ** module EventDispatcher.h - This object allows any client class to
 *         register one of its member functions as an event handler
 * function for a given event.  The event handler can be registered not
 * only by event, but also by name and by TID, so that even though
 * registered, it will only be called if the specific event is truly
 * intended for it.  The interesting feature of EventDispatcher is that
 * it allows the registration of member functions of unrelated classes to
 * be registered as callbacks.  The classes do not have to belong to any
 * inheritance hierarchy (The only requirement is that the event handler
 * member functions have similar signatures.  The signature is dictated
 * by the helper classes EvenHandler and TCallback<typename T>.  More
 * on these follows.)
 * EventDispatcher does this by registering as a callback any object of
 * type CallbackBase.  The client class in turn derive from class CallbackBase
 * a class of its own that knows how to encapsulate the specific object
 * pointer and a pointer to the member function that will handle the event.
 * This has been rendered trivial by providing a template class
 * TCallback<typename T>, which inherits from CallbackBase, to easily
 * do this.  All a client class needs to do to register one of its own member
 * functions as a callback function in the EventDispatcher class is
 * to use this template to create a compatible CallbackBase object:
 *
 * #include "EventDispatcher.h"
 *
 * class F
 *
 * {
 *   public:
 *
 *     F() {};
 *     void FOO(void *, void *);
 *     void BAR(void *, void *);
 *
 *   private:
 *
 *     TCallback<F> *foo_evh, *bar_evh;
 * };
 *
 * Later, in F's constructor:
 *
 * foo_evh = new TCallback<F>(this, &F::FOO);
 * bar_evh = new TCallback<F>(this, &F::BAR);
 *
 * As can be seen, TCallback's constructor takes a pointer to the
 * object using it (the 'this' pointer) and a pointer to the user class'
 * member function that will actually do the event handling.
 * At this point, whenever the event handler is needed, it can be used
 * to subscribe to the event:
 *
 * EventDispatcher ed;
 * ed.subscribe(EV_FOO, foo_evh);
 * ed.subscribe(EV_BAR, bar_evh);
 *
 * and when it is no longer needed, it is unsubscribed:
 *
 * ed.unsubscribe(EV_FOO, foo_evh);
 * ed.unsubscribe(EV_BAR, bar_evh);
 *
 * A handler can be registered and unregistered as often as needed.
 *
 * Finally, in F's destructor, the event objects are deleted.
 *
 * TCallback objects also take have a void * as a data member.  This
 * pointer points to callback object specific instance data.  This
 * allows the use of a single callback function to act as a callback
 * for several different events.  For example, it could pass the data
 * received from the event object to the data object supplied in this
 * custom data pointer.  Thus, all callback functions must take two
 * void *'s as parameters.  The custom data pointer in TCallback's
 * constructor and Parameters() member function default to 0, so this
 * custom pointer needn't be used if unique object/callback member
 * functions combinations are used.
 *
 * Note that references to the event objects are stored in EventDispatcher's
 * dispatch map as pointers.  This must be so because the event map deals
 * with CallbackBase objects, but CallbackBase is a pure abstract base class.
 * The actual objects are all derived from CallbackBase.  Thus, the lifetime
 * of these event handler objects must span the calls to Register() and
 * Unregister().
 *
 * EventDispatcher is thread safe.  Any task can register and unregister
 * an event handler any time it likes with no worries.  This means that
 * it should be ready to be suspended when it calls Register() and Unregister().
 *
 *  Copyright (C) 2001 Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: EventDispatcher.h,v 1.5 2006/01/18 23:17:56 rcreager Exp $
 *
 *******************************************************************/

#ifndef _EVENTDISPATCHER_H_
#define _EVENTDISPATCHER_H_

#include "Mutex.h"
#include "ThreadLock.h"

#include <map>
#include <list>
#include <algorithm>

class CallbackBase;


/********************************************************************
 ** class EventDispatcher
 *
 *  registers subscribers to events by event.
 *
 *******************************************************************/

template <typename T> class EventDispatcher

{
  public:

    EventDispatcher();
    ~EventDispatcher();

    void subscribe(T ev, CallbackBase *evh);
    void unsubscribe(T ev, CallbackBase *evh);
    bool subscribed(T ev, CallbackBase *evh);
    unsigned int subscribers(T ev);
    void dispatch(T ev, void *dp);


  private:

    typedef std::list<CallbackBase *> disp_lst;
    typedef std::map<T, disp_lst> ev_map;

    ev_map _em;
    mutable Mutex _ics;

};

/********************************************************************
 ** class Callback
 *
 *  This is the base event handler class.  This class is used to store
 *  event handlers in EventDispatcher's dispatch maps.  The actual event
 *  handler objects are derived from this class using TCallback<typename T>,
 *  a template class.  Thus, any class can easily customize an event handler
 *  for itself that is a subclass of Callback and thus compatible with
 *  EventDispatcher.
 *
 *******************************************************************/

class CallbackBase

{
  public:

    virtual ~CallbackBase() {};
    void Execute(void *p) {execute(p);}

  protected:

    CallbackBase() {}

  private:

    virtual void execute(void *) = 0;


};

/********************************************************************
 ** class TCallback
 *
 *  This is the actual class that encapsulates an event handler.  It
 *  is a template class, thus allowing it to be used by any class to
 *  encapsulate one of its own member functions as an event handler.
 *
 *******************************************************************/

template <typename T> class TCallback : public CallbackBase

{
  public:

    TCallback();
    TCallback(T *p, void (T::*hndlr)(void *, void *), void * = 0);
    ~TCallback();

    void Parameters(T *p, void (T::*hndlr)(void *, void *), void * = 0);
    void *Data() {return _cust_data;}

  private:

    virtual void execute(void *);

    T *_fp;
    void (T::*_callback)(void *, void *);
    void *_cust_data;

};

/********************************************************************
 ** TCallback<T>::TCallback()
 *
 *  @mfunc Default constructor for a TCallback<T> object.  This allows
 *         the object to be used in arrays.  Member function Parameters()
 *         must then be used to set up the object before being used.
 *
 *  @parm T * | p | A pointer to the using object (normally that object's
 *                  'this' pointer.
 *  @parm void (T::*)(void *, void *) | hndlr | A pointer to the member
 *          function of the using class that will be used as the event
 *          handler.
 *  @parm void * | dp | pointer to data to be used by this particular
 *                      instance of the callback.
 *
 *******************************************************************/

template <typename T> TCallback<T>::TCallback()

{
    _fp = 0;
    _callback = 0;
    _cust_data = 0;

}

/********************************************************************
 ** TCallback<T>::TCallback(T *, void (T::*)(void *, void *))
 *
 *  @mfunc Constructs a TCallback<T> object using the data passed
 *         into the constructor.
 *
 *  @parm T * | p | A pointer to the using object (normally that object's
 *                  'this' pointer.
 *  @parm void (T::*)(void *, void *) | hndlr | A pointer to the member
 *          function of the using class that will be used as the event
 *          handler.
 *  @parm void * | dp | pointer to data to be used by this particular
 *                      instance of the callback.
 *
 *******************************************************************/

template <typename T> TCallback<T>::TCallback(T *p,
                                              void (T::*hndlr)(void *, void *),
                                              void *dp)

{
    Parameters(p, hndlr, dp);

}

/********************************************************************
 * TCallback<T>::~TCallback()
 *
 ** Destructor.  Removes custom data if present.
 *
 *******************************************************************/

template <typename T> TCallback<T>::~TCallback()

{
}

/********************************************************************
 ** TCallback<T>::Parameters(T *, void (T::*)(void *, void *))
 *
 *  @mfunc Loads the values needed by the object to function.  This
 *         function is used to re-use an object or to initialize one
 *         if the default constructor was used to create the object.
 *
 *  @parm T * | p | A pointer to the using object (normally that object's
 *                  'this' pointer.
 *  @parm void (T::*)(void *, void *) | hndlr | A pointer to the member
 *          function of the using class that will be used as the event
 *          handler.
 *  @parm void * | dp | pointer to data to be used by this particular
 *                      instance of the callback.
 *
 *******************************************************************/

template <typename T> void TCallback<T>::Parameters(T *p,
                                                    void (T::*hndlr)(void *, void *),
                                                    void *dp)

{
    _fp = p;
    _callback = hndlr;
    _cust_data = dp;

}

/********************************************************************
 ** TCallback<T>::execute(void *)
 *
 *  @mfunc Executes the handler by calling the owning object's member
 *         function using the pointer to the object and the pointer to
 *         the member function.
 *
 *  @parm void * | ev_data | The event data.  This data can be anything,
 *        wich doesn't matter to execute(), as it passes it on to the
 *        member function that serves as the actual callback function.
 *
 *******************************************************************/

template <typename T> void TCallback<T>::execute(void *ev_data)

{
    if (_fp && _callback)
    {
        (_fp->*_callback)(ev_data, reinterpret_cast<void *>(_cust_data));
    }

}

/********************************************************************
 ** EventDispatcher::EventDispatcher()
 *
 *  @mfunc Constructor for the event dispatcher class
 *
 *******************************************************************/

template <typename T> EventDispatcher<T>::EventDispatcher()

{
}

/********************************************************************
 ** EventDispatcher::~EventDispatcher()
 *
 *  @mfunc Destructor for the event dispatcher class
 *
 *******************************************************************/

template <typename T> EventDispatcher<T>::~EventDispatcher()

{
}

/********************************************************************
 ** EventDispatcher::subscribe(CallbackBase *, T)
 *
 *  @mfunc Places a pointer to the event handler into the dispatcher map.
 *         The map first checks to see if the pointer isn't already
 *         in the map.  If not, it adds it.  If no entry for ev
 *         exist, the act of checking will create it.
 *
 *  @parm T | ev | The event
 *  @parm CallbackBase * | evh | The event handler object.
 *
 *******************************************************************/

template <typename T> void EventDispatcher<T>::subscribe(T ev, CallbackBase *evh)

{
    ThreadLock<Mutex> l(_ics);


    l.lock();

    if (find(_em[ev].begin(), _em[ev].end(), evh) == _em[ev].end())
    {
        _em[ev].push_back(evh);
    }

}

/********************************************************************
 ** EventDispatcher::unsubscribe(T ev, CallbackBase *evh)
 *
 *  @mfunc Removes the pointer to the event handler from the dispatcher map.
 *         The map first checks to see if the pointer is in the map.
 *         If it is, it removes it.  If this removal results in no entries
 *         for the event and tid map keys, it removes them too.  Though the
 *         event keys are a small number (80 or so), the tid keys could
 *         go on forever if not removed.
 *
 *  @parm int | ev | The event number
 *  @parm CallbackBase * | evh | The event handler object.
 *
 *******************************************************************/

template <typename T> void EventDispatcher<T>::unsubscribe(T ev, CallbackBase *evh)

{
    typename ev_map::iterator handlers;
    disp_lst::iterator ev_handler;
    ThreadLock<Mutex> l(_ics);


    l.lock();

    if ((handlers = _em.find(ev)) != _em.end())
    {
        if ((ev_handler = find(handlers->second.begin(),
                               handlers->second.end(), evh)) != handlers->second.end())
        {
            handlers->second.erase(ev_handler); // remove handler from list of handlers
        }

        if (handlers->second.empty())           // No more handlers? Remove event entry.
        {
            _em.erase(handlers);
        }
    }

}

/********************************************************************
 * EventDispatcher::subscribed(int ev, CallbackBase *evh)
 *
 ** Checks to see if event handler 'evh' is subscribed to event 'ev'.
 *
 * @param int ev: The event
 * @param CallbackBAse *evh: The event handler
 *
 * @return 'true' if 'evh' is subscribed to 'ev', 'false' otherwise.
 *
 *******************************************************************/

template <typename T> bool EventDispatcher<T>::subscribed(T ev, CallbackBase *evh)

{
    typename ev_map::iterator handlers;
    disp_lst::iterator ev_handler;
    ThreadLock<Mutex> l(_ics);


    l.lock();

    if ((handlers = _em.find(ev)) != _em.end())
    {
        if ((ev_handler = find(handlers->second.begin(),
                               handlers->second.end(), evh)) != handlers->second.end())
        {
            return true;
        }
    }

    return false;

}

/********************************************************************
 * EventDispatcher::subscribers(T ev)
 *
 ** Returns the number of subscribers to the event 'ev'.
 *
 * @param T ev: The event
 *
 * @return unsigned int, the number of subscribers
 *
 *******************************************************************/

template <typename T> unsigned int EventDispatcher<T>::subscribers(T ev)

{
    typename ev_map::iterator handlers;
    ThreadLock<Mutex> l(_ics);


    l.lock();

    if ((handlers = _em.find(ev)) != _em.end())
    {
        return (unsigned int)handlers->second.size();
    }

    return 0;

}

/********************************************************************
 ** EventDispatcher::dispatch(int ev, void *dp, const char *n, unsigned long tid)
 *
 *  @mfunc This function executes any callback function that has been
 *         registered by the event number.
 *
 *  @parm int | ev | The event number
 *  @parm void * | dp | The data for the event
 *
 *******************************************************************/

template <typename T> void EventDispatcher<T>::dispatch(T ev, void *dp)

{
    disp_lst::iterator ev_handler;
    typename ev_map::iterator handlers;
    ThreadLock<Mutex> l(_ics);


    l.lock();

    if ((handlers = _em.find(ev)) != _em.end())
    {
        for (ev_handler = handlers->second.begin();
             ev_handler != handlers->second.end();
             ++ev_handler)
        {
            (*ev_handler)->Execute(dp);
        }
    }

}

#endif // _EVENTDISPATCHER_H_
