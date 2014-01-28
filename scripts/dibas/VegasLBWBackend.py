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

from VegasBackend import VegasBackend
from vegas_ssg import SwitchingSignals
from Backend import convertToMHz
from datetime import datetime, timedelta
import math

class VegasLBWBackend(VegasBackend):
    """
    A class which implements the VEGAS LBW-specific parameter calculations.

    VegasLBWBackend(theBank, theMode, theRoach = None, theValon = None)

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

        # gain array. May be 1 or 8 in length, depending on mode.
        self.gain = theMode.gain

        # setup the parameter dictionary/methods
        self.params["gain"         ] = self.setLBWGain

        # In LBW mode the spec_tick is computed differently than in HBW
        # mode. Inform the switching signal builder of the change.
        self.spec_tick = self.computeSpecTick()
        self.ss.set_spec_tick(self.spec_tick)
        self.ss.set_hwexposr(self.hwexposr)
        self.clear_switching_states()
        self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = False)


    def computeSpecTick(self):
        """Returns the spec_tick value for this backend (the LBW value)

        """
        print "VegasLBWBackend::computeSpecTick: self.frequency =", self.frequency
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

    def needs_reset(self):
        return False

    def setLBWGain(self, gain):
        """Sets the gain to a new value.

        * *gain*: the new value. May be a scalar, in which case it is
                  assumed to be the first element of the 'gain'
                  property. This works for all modes, but you should be
                  aware that for modes 20-29 you need 8 gain
                  elements. In that case 'gain' should be a list with 8
                  gain values.

        """
        if isinstance(gain, list):
            self.gain = gain
        else:
            self.gain = [gain]


    def _init_gpu_resources(self):
        """
        When resources change (e.g. number of channels, subbands, etc.) We
        clear the initialize indicator and tell the HPC to reallocate resources.
        """
        if not self.test_mode:
            self.write_status(GPUCTXIN="FALSE")
            self.hpc_cmd("INIT_GPU")
            status,wait = self._wait_for_status('GPUCTXIN', 'TRUE', timedelta(seconds=75))

            if not status:
                raise Exception("init_gpu_resources(): timed out waiting for 'GPUCTXIN=TRUE'")


    # Algorithmic dependency methods, not normally called by a users

    def _fpga_clocks_per_spec_tick(self):
        return 128


    def _set_state_table_keywords(self):
        """
        Gather status sets here for LBW cases.
        """
        super(VegasLBWBackend, self)._set_state_table_keywords()
        self.set_status(BW_MODE  = "low")
        self.set_status(HWEXPOSR = str(self.hwexposr))
        self.set_status(EXPOSURE = str(self.exposure))
        self.set_status(EXPOCLKS = str(self.expoclks))
        self.set_status(OBS_MODE = "LBW")
