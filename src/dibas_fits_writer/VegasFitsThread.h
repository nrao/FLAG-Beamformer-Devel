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

//# $Id$

#ifndef VegasFitsThread_h
#define VegasFitsThread_h


class VegasFitsIO;

/// The thread namespace/class for the GBT-like vegas FITS writer.
/// The thread entry point from the main routine is via the
/// runFitsWriter() trampoline function.

class VegasFitsThread
{
public:
    VegasFitsThread() {};
    static void *run(struct vegas_thread_args *args);
    static void set_finished(struct vegas_thread_args *args);
    static void status_detach(vegas_status *st);
    static void setExitStatus(vegas_status *st);
    static void databuf_detach(vegas_databuf *);
    static void free_sdfits(vegas_status *st);
    static void close(VegasFitsIO *f);

protected:

}; 

// Calls directly into VegasFitsThread::run()
extern "C" void *runGbtFitsWriter(void *args);

#endif
