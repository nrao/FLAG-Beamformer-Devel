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
from VegasBackend import VegasBackend
from vegas_ssg import SwitchingSignals
import subprocess
import time
from datetime import datetime, timedelta
import os
import apwlib.convert as apw
from lbw_mixer_funcs import LBWMixerCalcs

class VegasL8LBWBackend(VegasBackend):
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
        VegasBackend.__init__(self, theBank, theMode, theRoach , theValon, hpc_macs, unit_test)

        if 'lbw8' in theMode.shmkvpairs['MODENAME']:
            nsubbands = 8
        else:
            nsubbands = 1

        self.nchan = theMode.nchan
        self.gain1 = [ 1024 ] * nsubbands
        self.gain2 = [ 1024 ] * nsubbands


        # default dependent values, computed from Parameters:
        # a resonable default:
        self.subbandfreq = [ convertToMHz(self.frequency/2) ] * nsubbands
        self.actual_subband_freq = self.subbandfreq
        self.nsubbands = nsubbands
        # L8 specific parameters
        self.params["subband_freq" ] = self._setSubbandFreq
        self.params["gain1"        ] = self._setGain1
        self.params["gain2"        ] = self._setGain2

        self.lbwmixer = LBWMixerCalcs(self.frequency)

        self.prepare()


    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        VegasBackend.__del__(self)


    def _sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """

        # extract mode number from mode name, which is expected to be
        # 'MODEx' where 'x' is the number we want:
        mode = int(self.mode.name[4:])

        self.sampler_frequency = self.frequency * 1e6 / 32
        # calculate the fpga frequency
        self.fpga_clock = self.frequency * 1e6 / 8

        if mode > 9 and mode < 20:
            self.nsubband = 1
        else:
            self.nsubband = 8


    def _setGain1(self, gain1):
        """
        Sets the quantization gain1 for the subbands
        Parameter must be a list of length 1 or 8 (matching the number of subbands in use)
        """
        if not isinstance(gain1, list) or len(gain1) != self.nsubbands:
            raise "gain1 is not a list or the number of entries does not match the number of subbands"
        else:
            self.gain1 = gain1

    def _setGain2(self, gain2):
        """
        Sets the gain2 for the subbands (definition?)
        Parameter must be a list of length 1 or 8 (matching the number of subbands in use)
        """
        if not isinstance(gain2, list) or len(gain2) != self.nsubbands:
            raise "gain2 is not a list or the number of entries does not match the number of subbands"
        else:
            self.gain2 = gain2

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
        self.actual_subband_freq = [ 0 ] * self.nsubbands

        # calculate the nearest lo frequency and save it for writing the SUBxFREQ keywords
        self.actual_subband_freq = []
        for subband_num in range(len(self.subbandfreq)):
            _,actual_lo = self.lbwmixer.lo_setup(self.subbandfreq[subband_num], subband_num)
            self.actual_subband_freq.append(actual_lo)
            self._set_status_str("SUB%iFREQ" % subband_num, actual_lo)
        mixerdict = self.lbwmixer.get_lo_results()
        for i in mixerdict.keys():
            kwval = { i : mixerdict[i] }
            self.set_register(**kwval)


    def _gain_dep(self):
        """
        Do the quantization gain calculation and append the result to the register dictionary
        """
        for subband in range(self.nsubbands):
            kwval = { 's'+str(subband)+'_quant_gain1' : self.gain1[subband] }
            self.set_register(**kwval)
            kwval = { 's'+str(subband)+'_quant_gain2' : self.gain2[subband] }
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
        self._subfreq_dep(self.frequency)
        self._gain_dep()
        self._mode_select_dep()

        # now update all the status keywords needed for this mode:
        self._set_state_table_keywords()
        self._init_gpu_resources()



    def _init_gpu_resources(self):
        """
        When resources change (e.g. number of channels, subbands, etc.) We
        clear the initialize indicator and tell the HPC to reallocate resources.
        """
        self.set_status(GPUCTXIN="FALSE")
        self.hpc_cmd("INIT_GPU")
        status,wait = self._wait_for_status('GPUCTXIN', 'TRUE', timedelta(seconds=75))

        if not status:
            raise Exception("init_gpu_resources(): timed out waiting for 'GPUCTXIN=TRUE'")


    def _set_state_table_keywords(self):
        """Update status memory keywords. Calls the base class version first to
        get a dictionary with the default base class values set, then
        adds/modifies kvpairs specific to this mode, and finally writes
        the data to shared memory.

        """
        # Add some additional keyword value pairs
        self._set_status_str("NSUBBAND", self.nsubbands)

        # In the case of LBW1, we replicate the one subband frequency into all 8 keywords
        if len(self.actual_subband_freq) == 1:
            sub_band_frequencies = [ self.actual_subband_freq[0] ] * 8
        else:
            sub_band_frequencies = self.actual_subband_freq

        for i in range(len(sub_band_frequencies)):
            self._set_status_str("SUB%iFREQ" % (i), sub_band_frequencies[i])

        for i in range(self.nsubbands):
            statusdata["_MCR1_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MCDL_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MFQR_%02d" % (i+1)] = str(self.frequency_resolution)

        # Now write all of the keyword list
        if self.bank is not None:
            self.set_status(**statusdata)
        else:
            for i in statusdata.keys():
                print "%s = %s" % (i, statusdata[i])

        return statusdata
