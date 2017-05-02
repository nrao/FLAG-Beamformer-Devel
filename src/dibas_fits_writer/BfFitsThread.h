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

//# $Id$

#ifndef BfFitsThread_h
#define BfFitsThread_h


class BfFitsIO;

/// The thread namespace/class for the GBT-like vegas FITS writer.
/// The thread entry point from the main routine is via the
/// runFitsWriter() trampoline function.

class BfFitsThread
{
public:
    BfFitsThread() {};
    /// The FITS writing main loop. Data blocks are obtained from data shared memory
    /// and processed by the BfFitsIO writer.
    /// Processing proceeds as follows:
    /// 1. A data block is waited to be filled.
    /// 2. When the block is full, each dataset contained in the block is processed
    /// by the DiskBuffer class (organizes the data and transposes it as necessary).
    /// 3. When a full integration is detected, the data is written as a row in the
    /// FITS file DATA table.
    static void *run(struct vegas_thread_args *args);
    static void set_finished(struct vegas_thread_args *args);
    static void status_detach(vegas_status *st);
    static void setExitStatus(vegas_status *st);
    static void databuf_detach(bf_databuf *);
    static void databuf_detach(void *);
    static void free_sdfits(vegas_status *st);
    static void close(BfFitsIO *f);
    //virtual void *databuf_attach(int id) = 0;

protected:

};

// Calls directly into BfFitsThread::run()
extern "C" void external_close(int sig); 
#endif
