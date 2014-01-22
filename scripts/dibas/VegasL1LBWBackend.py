######################################################################
#
#  VegasLBWBackend.py -- A backend for LBW modes
#
#  Copyright (C) 2014 Associated Universities, Inc. Washington DC, USA.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Correspondence concerning GBT software should be addressed as follows:
#  GBT Operations
#  National Radio Astronomy Observatory
#  P. O. Box 2
#  Green Bank, WV 24944-0002 USA
#
#  $Id:$
#
######################################################################

import struct
import ctypes
import binascii
import player
from VegasBackend import VegasBackend
from vegas_ssg import SwitchingSignals
from Backend import convertToMHz
import subprocess
import time
from datetime import datetime, timedelta
import os
import apwlib.convert as apw
import math

class VegasL1LBWBackend(VegasBackend):
    """
    A class which implements the VEGAS LBW-specific parameter calculations.

    VegasL1LBWBackend(theBank, theMode, theRoach = None, theValon = None)

    Where:

    * *theBank:* Instance of specific bank configuration data BankData.
    * *theMode:* Instance of specific mode configuration data ModeData.
    * *theRoach:* Instance of katcp_wrapper
    * *theValon:* instance of ValonKATCP
    * *unit_test:* Set to true to unit test. Will not attempt to talk to
      roach, shared memory, etc.
    """
    def __init__(self, theBank, theMode, theRoach, theValon, hpc_macs, unit_test = False):
        """
        Creates an instance of the vegas internals.
        """

        VegasBackend.__init__(self, theBank, theMode, theRoach , theValon, hpc_macs, unit_test)
        # set up a trip wire to see if the GPU thread gets going
        self.set_status(GPUCTXIN='FALSE')

        # setup the parameter dictionary/methods
        self.params["gain"         ] = self.setLBWGain

        # In LBW mode the spec_tick is computed differently than in HBW
        # mode. Inform the switching signal builder of the change.
        self.spec_tick = self.computeSpecTick()
        self.ss.set_spec_tick(self.spec_tick)
        self.ss.set_hwexposr(self.hwexposr)
        self.clear_switching_states()
        self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = False)
        self.prepare()


    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        super(VegasL1LBWBackend, self).__del__()


    def computeSpecTick(self):
        """Returns the spec_tick value for this backend (the LBW value)

        """
        print "VegasL1LBWBackend::computeSpecTick: self.frequency =", self.frequency
        return 1024.0 / (convertToMHz(self.frequency) * 1e6)


    def add_switching_state(self, duration, blank = False, cal = False, sig_ref_1 = False):
        """add_switching_state(duration, blank, cal, sig_ref_1):

        Add a description of one switching phase. The LBW modes require
        computing the length in spec_ticks for blanking phases
        differntly from other phases. This function takes care of this.

        * *duration* is the length of this phase in seconds,
        * *blank* is the state of the blanking signal (True = blank, False = no blank)
        * *cal* is the state of the cal signal (True = cal, False = no cal)
        * *sig_ref_1* is the state of the sig_ref signal (True = ref, false = sig)

        Example to set up a 8 phase signal (4-phase if blanking is not
        considered) with blanking, cal, and sig/ref, total of 400 mS::

          be = Backend(None) # no real backend needed for example
          be.clear_switching_states()
          be.add_switching_state(0.01, blank = True,  cal = True,  sig_ref_1 = True)
          be.add_switching_state(0.09, blank = False, cal = True,  sig_ref_1 = True)
          be.add_switching_state(0.01, blank = True,  cal = True,  sig_ref_1 = False)
          be.add_switching_state(0.09, blank = False, cal = True,  sig_ref_1 = False)
          be.add_switching_state(0.01, blank = True,  cal = False, sig_ref_1 = True)
          be.add_switching_state(0.09, blank = False, cal = False, sig_ref_1 = True)
          be.add_switching_state(0.01, blank = True,  cal = False, sig_ref_1 = False)
          be.add_switching_state(0.09, blank = False, cal = False, sig_ref_1 = False)

        """

        if blank:
            dur = int(math.ceil(duration * self.chan_bw) * 1 / (self.spec_tick * self.chan_bw))
        else:
            dur = int(math.ceil(duration / self.hwexposr) * self.hwexposr / self.spec_tick)
        self.ss.add_phase(dur = dur, bl = blank, cal = cal, sr1 = sig_ref_1)
        return (True, self.ss.number_phases())

    ### Methods to set user or mode specified parameters
    ###

    def setLBWGain(self, gain):
        pass

    def prepare(self):
        """
        This command writes calculated values to the hardware and status memory.
        This command should be run prior to the first scan to properly setup
        the hardware.

        The sequence of commands to set up a measurement is thus typically::

          be.set_param(...)
          be.set_param(...)
          ...
          be.set_param(...)
          be.prepare()
        """
        # calculate the fpga_clock and sampler frequency
        self._sampler_frequency_dep()
        self._chan_bw_dep()
        self._obs_bw_dep()

        # calculate actual exposure
        self._exposure_dep()

        # Switching Signals info. Switching signals should have been
        # specified prior to prepare():
        self._setSSKeys()
        # program I2C: input filters, noise source, noise or tone
        self.set_if_bits()
        # now update all the status keywords needed for this mode:
        self._set_state_table_keywords()

        # set the roach registers:
        if self.roach:
            # write the switching signal specification to the roach:
            self.roach.write_int('ssg_length', self.ss.total_duration_granules())
            self.roach.write('ssg_lut_bram', self.ss.packed_lut_string())
            master = 1 if self.bank.i_am_master else 0
            sssource = 0 # internal
            bsource = 0 # internal
            ssg_ms_sel = self.mode.master_slave_sels[master][sssource][bsource]
            self.roach.write_int('ssg_ms_sel', ssg_ms_sel)
            self.roach.write_int('gain', self._gain)


    # Algorithmic dependency methods, not normally called by a users

    def _fpga_clocks_per_spec_tick(self):
        return 128

    def _chan_bw_dep(self):
        self.chan_bw = self.sampler_frequency / (self.nchan * 2.0)
        self.frequency_resolution = abs(self.chan_bw)
        print "_chan_bw_dep(): efsampfr = %d; nchan = %i; chan_bw = %d" % \
            (self.sampler_frequency, self.nchan, self.chan_bw)

    def _sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """

        # extract mode number from mode name, which is expected to be
        # 'MODEx' where 'x' is the number we want:
        self.sampler_frequency = convertToMHz(self.frequency) * 1e6 / 4
        self.nsubband = 1
        # calculate the fpga frequency
        self.fpga_clock = convertToMHz(self.frequency) * 1e6 / 8


    def _set_state_table_keywords(self):
        """
        Gather status sets here
        Not yet sure what to place here...
        """
        statusdata = super(VegasL1LBWBackend, self)._set_state_table_keywords()
        statusdata["BW_MODE"  ] = "low"
        statusdata["HWEXPOSR" ] = str(self.hwexposr)
        statusdata["EXPOSURE" ] = str(self.exposure)
        statusdata["EXPOCLKS" ] = str(self.expoclks)
        statusdata["OBS_MODE" ] = "LBW"

        if self.bank is not None:
            self.set_status(**statusdata)
        else:
            for i in statusdata.keys():
                print "%s = %s" % (i, statusdata[i])

        return statusdata
