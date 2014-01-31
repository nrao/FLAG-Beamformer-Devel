######################################################################
#
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
######################################################################
import struct
import ctypes
import binascii
import player
from Backend import Backend,convertToMHz
from VegasLBWBackend import VegasLBWBackend
from vegas_ssg import SwitchingSignals
import subprocess
import time
from datetime import datetime, timedelta
import os
import apwlib.convert as apw
from lbw_mixer_funcs import LBWMixerCalcs

class VegasL8LBWBackend(VegasLBWBackend):
    """
    A class which implements some of the VEGAS specific parameter calculations
    for the L8LBW1 and L8LBW8 modes.

    VegasL8LBWBackend(theBank, theMode, theRoach = None, theValon = None)

    Where:

    * *theBank:* Instance of specific bank configuration data BankData.
    * *theMode:* Instance of specific mode configuration data ModeData.
    * *theRoach:* Instance of katcp_wrapper
    * *theValon:* instance of ValonKATCP
    * *unit_test:* Set to true to unit test. Will not attempt to talk to
      roach, shared memory, etc.
    """
    def __init__(self, theBank, theMode, theRoach=None, theValon=None, hpc_macs=None, unit_test = False):
        """
        Creates an instance of the vegas internals for the L8LBW firmware.
        """

        #print str(theMode)
        # mode_number may be treated as a constant; the Player will
        # delete this backend object and create a new one on mode
        # change.
        VegasLBWBackend.__init__(self, theBank, theMode, \
                                 theRoach , theValon, hpc_macs, unit_test)

        if 'lbw8' in theMode.backend_name.lower():
            nsubbands = 8
        else:
            nsubbands = 1

        self.nsubbands = nsubbands

        if not self.gain:
            self.gain = [ 1024 ] * nsubbands

        # default dependent values, computed from Parameters:
        # a resonable default:
        self.subbandfreq = [ convertToMHz(self.frequency/2) * 1.0e6 ] * nsubbands
        self.actual_subband_freq = self.subbandfreq
        # L8 specific parameters
        self.params["subband_freq" ] = self._setSubbandFreq
        self.lbwmixer = LBWMixerCalcs(self.frequency)

        self.progdev()
        self.net_config()

        if self.mode.roach_kvpairs:
            self.write_registers(**self.mode.roach_kvpairs)

        self.reset_roach()
        self.clear_switching_states()
        self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = False)

        self.prepare()
        self.start_hpc()
        self.start_fits_writer()
        self._init_gpu_resources()

    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        VegasBackend.__del__(self)


    def _sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """
        self.sampler_frequency = convertToMHz(self.frequency) * 1e6 / 32


    def _mixer_cnt_dep(self):
        """
        This is a calculated non-user-settable parameter, which specifies the length
        of the LO bram tables.
        For now this should be the bram_size - 2 i.e. (1<<10) - 2
        In the 8 subwindow mode, all mixer_cnt registers are set,
        in the 1 subwindow mode, only the first mixer_cnt register is set.
        """
        LOG_LO_BRAM_LENGTH = 10
        reg_size = (1<<LOG_LO_BRAM_LENGTH)-2 # length of each BRAM register
        for i in range(self.nsubbands):
            reg_val = { "s" + i + "_mixer_cnt" : reg_size }
            self.set_register(**reg_val)


    def _mode_select_dep(self):
        """
        Sets the mode_sel register based on the number of subbands
        Zero specifies 8 band mode, 1 specifies 1 band mode
        """
        # base the value on the LSB bit of the number of subbands (has the correct sense)
        value = self.nsubbands & 0x1
        self.set_register(mode_sel=value)


    def _setSubbandFreq(self, subbandfreq):
        """
        A list specifying the frequencies for each subband for a given bank.
        Should probably check to verify len(subbandfreq) is consistent with current mode.
        """
        if not isinstance(subbandfreq, list) or len(subbandfreq) not in [1,8]:
            raise Exception("The parameter 'subband_frq_list' " \
                            "must be a list of 1 or 8 frequencies for each bank subband")

        # convert the list of values into Hz
        self.subbandfreq = []

        for subband in range(len(subbandfreq)):
            self.subbandfreq.append(convertToMHz(subbandfreq[subband]) *1e6)

    def _setNumberSubbands(self, nsubbands):
        """
        Selects the Number of subbands. Legal values are 1 or 8.
        NOTE: This should not be a user parameter, better to have it specified by config file/mode.
        """
        if nsubbands not in [1,8]:
            raise Exception("number of sub-bands must be 1 or 8.")
        self.nsubbands = nsubbands


    def _subfreq_dep(self):
        """
        Compute the baseband frequencies for each sub-band.
        """
        # tell the mixer utility what the valon frequency is
        self.lbwmixer.set_valon_frequency(self.frequency)

        print "nsubbands is ", len(self.subbandfreq)
        print "we think it  ", self.nsubbands

        if self.nsubbands == 1:
            if len(self.subbandfreq) < 1:
                raise Exception("The number of subband frequencies is not 1")
        else:
            if len(self.subbandfreq) != 8:
                raise Exception("The number of subband frequencies is not 8")

        # clear out previous results
        self.lbwmixer.clear_results()

        # calculate the nearest lo frequency and save it for writing the SUBxFREQ keywords
        self.actual_subband_freq = []

        for subband_num in range(len(self.subbandfreq)):
            _,actual_lo = self.lbwmixer.lo_setup(self.subbandfreq[subband_num], subband_num)
            self.actual_subband_freq.append(actual_lo)

        mixerdict = self.lbwmixer.get_lo_results()

        for i in mixerdict.keys():
            kwval = { i : mixerdict[i] }
            self.set_register(**kwval)


    def _gain_dep(self):
        """
        Do the quantization gain calculation and append the result to the register dictionary
        """
        for subband in range(self.nsubbands):
            kwval = { 's'+str(subband)+'_quant_gain' : self.gain[subband] }
            self.set_register(**kwval)


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

        super(VegasL8LBWBackend, self).prepare()
        self._subfreq_dep()
        self._gain_dep()
        self._mode_select_dep()

        # though parent has already called this, call again because the
        # three calls above have added things.
        self._set_state_table_keywords()

        # Talk to outside things: status memory, HPC programs, roach
        if self.bank is not None:
            self.write_status(**self.status_mem_local)
        else:
            for i in self.status_mem_local.keys():
                print "%s = %s" % (i, self.status_mem_local[i])

        if self.roach:
            self.write_registers(**self.roach_registers_local)


    def _set_state_table_keywords(self):
        """Update status memory keywords. Calls the base class version first to
        get a dictionary with the default base class values set, then
        adds/modifies kvpairs specific to this mode, and finally writes
        the data to shared memory.

        """
        # Add some additional keyword value pairs.
        super(VegasL8LBWBackend, self)._set_state_table_keywords()
        self.set_status(NSUBBAND = str(self.nsubbands))

        # In the case of LBW1, we replicate the one subband frequency into all 8 keywords
        if len(self.actual_subband_freq) == 1:
            sub_band_frequencies = [ self.actual_subband_freq[0] ] * 8
        else:
            sub_band_frequencies = self.actual_subband_freq

        statusdata = {}

        print str(self.subbandfreq)
        for i in range(len(sub_band_frequencies)):
            print "SUB%iFREQ =" % (i), str(sub_band_frequencies[i])
            statusdata["SUB%iFREQ" % (i)] = str(sub_band_frequencies[i])

        for i in range(self.nsubbands):
            statusdata["_MCR1_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MCDL_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MFQR_%02d" % (i+1)] = str(self.frequency_resolution)

        self.status_mem_local.update(statusdata)
