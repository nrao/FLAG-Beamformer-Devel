/*******************************************************************
 ** Mutex.h - Implements a Mutex class that encapsulates Windows/Posix
 *            mutexes.
 *
 *  Copyright (C) 2002 Associated Universities, Inc. Washington DC, USA.
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
 *  $Id: Mutex.cc,v 1.4 2006/09/28 18:51:30 rcreager Exp $
 *
 *******************************************************************/

#include "Mutex.h"

Mutex::~Mutex()
{
#if defined(WIN32)
    CloseHandle(mutex);
#else
    pthread_mutex_destroy(&mutex);
#endif
}

Mutex::Mutex()
{
#if defined(WIN32)
    mutex = CreateMutex(0, FALSE, 0);
#else
    pthread_mutex_init(&mutex, 0);
#endif
}

int Mutex::lock()
{
#if defined(WIN32)
    return (WaitForSingleObject(mutex, INFINITE) != WAIT_FAILED) ? 0 : 1;
#else
    return pthread_mutex_lock(&mutex);
#endif
}

int Mutex::unlock()
{
#if defined(WIN32)
    return (ReleaseMutex(mutex) != 0) ? 0 : 1;
#else
    return pthread_mutex_unlock(&mutex);
#endif
}
