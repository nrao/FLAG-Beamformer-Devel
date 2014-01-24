######################################################################
#
#  VegasBackend.py -- The standard HBW Vegas backend; also serves as a
#  base class for the LBW backends.
#
#  Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
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
#import player
from Backend import Backend, convertToMHz
from vegas_ssg import SwitchingSignals
import subprocess
import time
from datetime import datetime, timedelta
import os
import math
import apwlib.convert as apw

class VegasBackend(Backend):
    """
    A class which implements some of the VEGAS specific parameter calculations.

    VegasBackend(theBank, theMode, theRoach = None, theValon = None)

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

        # mode_number may be treated as a constant; the Player will
        # delete this backend object and create a new one on mode
        # change.
        Backend.__init__(self, theBank, theMode, theRoach , theValon, hpc_macs, unit_test)
        # Important to do this as soon as possible, so that status application
        # can change its data buffer format
        self.set_status(BACKEND='VEGAS')

        # In VEGAS mode, i_am_master means this particular backend
        # controls the switching signals. (self.bank is from base class.)
        self.i_am_master = self.bank.i_am_master

        # the switching signals builder
        self.ss = SwitchingSignals()

        # Parameters:
        self.setPolarization('SELF')
        self.setNumberChannels(self.mode.nchan)
        self.requested_integration_time = 1.0

        # self.setValonFrequency(self.mode.frequency)

        # dependent values, computed from Parameters:
        self.nspectra = 1
        self.frequency_resolution = 0.0
        self.fpga_clock = None
        self.fits_writer_process = None
        self.scan_length = 30.0
        self.spec_tick = self.computeSpecTick()
        print "VegasBackend: self.spec_tick =", self.spec_tick
        self.setHwExposr(self.mode.hwexposr)

        # setup the parameter dictionary/methods
        self.params["polarization" ] = self.setPolarization
        self.params["nchan"        ] = self.setNumberChannels
        self.params["exposure"     ] = self.setIntegrationTime
        self.params["hwexposr"     ] = self.setHwExposr
        self.params["num_spectra"  ] = self.setNumberSpectra

        # the status memory key/value pair dictionary
        self.sskeys = {}
        self.ss.set_spec_tick(self.spec_tick)
        self.ss.set_hwexposr(self.hwexposr)
        self.clear_switching_states()
        self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = False)
        # self.prepare()
        self.start_hpc()
        self.start_fits_writer()


    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        if self.fits_writer_process is not None:
            print "Deleting FITS writer!"
            self.stop_fits_writer()

    def cleanup(self):
        """
        This explicitly cleans up any child processes. This will be called
        by the player before deleting the backend object.
        """
        print "VegasBackend: cleaning up hpc and fits writer."
        self.stop_hpc()
        self.stop_fits_writer()


    def computeSpecTick(self):
        """Returns the spec_tick value for this backend (the HBW value)

        """
        print "VegasBackend::computeSpecTick: self.frequency =", convertToMHz(self.frequency) * 1e6
        print "self.nchan =", self.nchan
        st = float(self.nchan) / (convertToMHz(self.frequency) * 1e6)
        print "st =", st
        return st

    ### Methods to set user or mode specified parameters
    ###

    def setPolarization(self, polar):
        """
        setPolarization(self, polar)

        *x* is a string 'CROSS', 'SELF1', 'SELF2', or 'SELF'
        """

        try:
            self.num_stokes = {'CROSS': 4, 'SELF1': 1, 'SELF2': 1, 'SELF': 2}[polar]
            self.polarization = polar
        except KeyError:
            raise Exception("polarization string must be one of: CROSS, SELF1, SELF2, or SELF")

    def setNumberChannels(self, nchan):
        """
        Sets the number of channels used by this mode. This is bof
        specific, and should match the requirements of the bof.
        """
        self.nchan = nchan

    def setADCsnap(self, snap):
        """
        """
        self.adc_snap = snap

    # TBF: What does nspectra do?
    def setNumberSpectra(self, nspectra):
        """
        Number of sub-bands.
        """
        self.nspectra = nspectra

    def setIntegrationTime(self, int_time):
        """
        Sets the integration time for each integration.
        """
        self.requested_integration_time = int_time

    def setHwExposr(self, hwexposr):
        """Sets the hwexposr value, usually the value is set from the
        dibas.conf configuration file. Also sets the acc_len, which
        falls out of the computation to ensure hwexposure is an even
        multiple of spec_ticks (that multiple is acc_len).

        """
        fpart, ipart = math.modf(hwexposr / self.spec_tick)

        if fpart >= 0.5:
            ipart = int(ipart) + 1

        self.hwexposr = self.spec_tick * ipart
        self.acc_len = ipart
        
    def needs_reset(self):
        """
        For some BOF's there is a status register which can flag an error state.
        """
        val = self.roach.read_int('status')
        if val & 0x01:
            return True
        else:
            return False


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
            self.set_register(acc_len=self.acc_len)
            # write the switching signal specification to the roach:
            self.roach.write_int('ssg_length', self.ss.total_duration_granules())
            self.roach.write('ssg_lut_bram', self.ss.packed_lut_string())
            master = 1 if self.bank.i_am_master else 0
            sssource = 0 # internal
            bsource = 0 # internal
            ssg_ms_sel = self.mode.master_slave_sels[master][sssource][bsource]
            self.roach.write_int('ssg_ms_sel', ssg_ms_sel)


    # Algorithmic dependency methods, not normally called by a users

    def _fpga_clocks_per_spec_tick(self):
        return self.nchan / 8

    def _exposure_dep(self):
        """Computes the actual exposure, based on the requested integration
           time. If the number of switching phases is > 1, then the
           actual exposure will be an integer multiple of the switching
           period. If the number of switching phases is == 1, then the
           exposure will be an integer multiple of hwexposr.

        """
        init_exp = self.requested_integration_time
        fpga_clocks_per_spec_tick = self._fpga_clocks_per_spec_tick()

        if self.ss.number_phases() > 1:
            sw_period = self.ss.total_duration()
            sw_granules = self.ss.total_duration_granules()

            print "sw_period =", sw_period, "sw_granules =", sw_granules
            print "init_exp =", init_exp

            r = init_exp / sw_period
            fpart, ipart = math.modf(r)

            if fpart > (sw_period / 100.0):
                ipart = ipart + 1.0

            self.expoclks = int(ipart * sw_granules * fpga_clocks_per_spec_tick)
        else: # number of phases = 1
            r = init_exp / self.hwexposr
            fpart, ipart = math.modf(r)

            if fpart > (init_exp / 100.0):
                ipart = ipart + 1.0

            self.expoclks = int(ipart * self.hwexposr * self.fpga_clock)

        self.exposure = float(self.expoclks) / self.fpga_clock


    def _chan_bw_dep(self):
        self.chan_bw = self.sampler_frequency / (self.nchan * 2)
        self.frequency_resolution = abs(self.chan_bw)

    def _sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """

        self.sampler_frequency = self.frequency * 1e6 * 2
        self.fpga_clock = self.frequency * 1e6 / 8
        self.nsubband = 1


    def clear_switching_states(self):
        """
        resets/deletes the switching_states
        """
        self.ss.clear_phases()
        return (True, self.ss.number_phases())

    def add_switching_state(self, duration, blank = False, cal = False, sig_ref_1 = False):
        """
        add_switching_state(duration, blank, cal, sig_ref_1):

        Add a description of one switching phase.
        Where:

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
        dur = int(math.ceil(duration / self.hwexposr) * self.hwexposr / self.spec_tick)
        self.ss.add_phase(dur = dur, bl = blank, cal = cal, sr1 = sig_ref_1)
        print "added switching phase", dur, blank, cal, sig_ref_1
        return (True, self.ss.number_phases())

    def set_gbt_ss(self, period, ss_list):
        """
        set_gbt_ss(period, ss_list):

        adds a complete GBT style switching signal description.

        * *period:* The complete period length of the switching signal.
        * *ss_list:* A list of GBT phase components. Each component is a tuple:
          (phase_start, sig_ref, cal, blanking_time)
          There is one of these tuples per GBT style phase.

        Example::

         b.set_gbt_ss(period = 0.1,
                      ss_list = ((0.0, SWbits.SIG, SWbits.CALON, 0.025),
                                 (0.25, SWbits.SIG, SWbits.CALOFF, 0.025),
                                 (0.5, SWbits.REF, SWbits.CALON, 0.025),
                                 (0.75, SWbits.REF, SWbits.CALOFF, 0.025))
                     )

        """
        try:
            self.nPhases = len(ss_list)
            self.clear_switching_states()

            for i in range(self.nPhases):
                this_start = ss_list[i][0]
                next_start = 1.0 if i + 1 == self.nPhases else ss_list[i + 1][0]
                duration = next_start * period - this_start * period
                blt = ss_list[i][3]
                nblt = duration - blt
                self.add_switching_state(blt, sig_ref_1 = ss_list[i][1], \
                                         cal = ss_list[i][2], blank = True)
                self.add_switching_state(nblt, sig_ref_1 = ss_list[i][1], \
                                         cal = ss_list[i][2], blank = False)
        except TypeError:
            # input error, leave it in a sane state.
            self.clear_switching_states()
            self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = True)
            raise Exception("Possible syntax error with parameter 'ss_list'. " \
                                "If 'ss_list' only has one phase element, please " \
                                "use the '((),)' syntax instead of '(())'")
        return (True, self.ss.number_phases())

    def show_switching_setup(self):
        srline=""
        clline=""
        blline=""
        calOnSym = "--------"
        calOffSym= "________"
        srSigSym = "--------"
        srRefSym = "________"
        blnkSym  = "^ %.3f "
        noBlkSym = "        "

        states = self.ss.gbt_phase_starts()
        print states

        for i in range(len(states['phase-starts'])):
            if states['sig/ref'][i]:
                srline = srline +  srSigSym
            else:
                srline = srline +  srRefSym
            if states['cal'][i]:
                clline = clline  +  calOnSym
            else:
                clline = clline  +  calOffSym
            if states['blanking'][i] > 0.0:
                blline = blline  +  blnkSym % states['blanking'][i]
            else:
                blline = blline  +  noBlkSym

        print "CAL    :", clline
        print "SIG/REF:", srline
        print "BLANK  :", blline


    def _setSSKeys(self):
        self.sskeys.clear()
        states = self.ss.gbt_phase_starts()
        cal = states['cal']
        sig_ref_1 = states['sig/ref']
        self.nPhases = len(sig_ref_1)
        empty_list = [0 for i in range(self.nPhases)] # For sig_ref_2, or I or E as appropriate

        for i in range(len(states['blanking'])):
            self._set_status_str('_SBLK_%02d' % (i+1), states['blanking'][i])

        for i in range(len(cal)):
            self._set_status_str('_SCAL_%02d' % (i+1), cal[i])

        for i in range(len(states['phase-starts'])):
            self._set_status_str('_SPHS_%02d' % (i+1), states['phase-starts'][i])

        for i in range(len(sig_ref_1)):
            self._set_status_str('_SSRF_%02d' % (i+1), sig_ref_1[i])

        master = self.i_am_master # TBF! Make sure this exists...
        self._setEcal(empty_list if master else cal)
        self._setEsigRef1(empty_list if master else sig_ref_1)
        self._setEsigRef2(empty_list)
        self._setIcal(cal if master else empty_list)
        self._setIsigRef1(sig_ref_1 if master else empty_list)
        self._setIsigRef2(empty_list)
        # self.sskeys now populated, and will be written with other status keys/vals.

    def _setEcal(self, cals):
        """
        External CAL

        cals is a list of integers where
        1 indicates the external cal is ON
        0 indicates the external cal is OFF
        """
        for i in range(len(cals)):
            self._set_status_str('_AECL_%02d' % (i+1), cals[i])

    def _setEsigRef1(self, sr):
        """
        External Sig/Ref 1

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self._set_status_str('_AESA_%02d' % (i+1), sr[i])

    def _setEsigRef2(self, sr):
        """
        External Sig/Ref 2

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self._set_status_str('_AESB_%02d' % (i+1), sr[i])

    def _setIcal(self, cals):
        """
        Internal CAL

        cals is a list of integers where
        1 indicates the external cal is ON
        0 indicates the external cal is OFF
        """
        for i in range(len(cals)):
            self._set_status_str('_AICL_%02d' % (i+1), cals[i])

    def _setIsigRef1(self, sr):
        """
        Internal Sig/Ref 1

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self._set_status_str('_AISA_%02d' % (i+1), sr[i])

    def _setIsigRef2(self, sr):
        """
        Internal Sig/Ref 2

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self._set_status_str('_AISB_%02d' % (i+1), sr[i])

    def _obs_bw_dep(self):
        """
        Observation bandwidth dependency
        """
        self.obs_bw = self.chan_bw * self.nchan

    def _set_status_str(self, x, y):
        """
        Add/update an item to the status memory keyword list
        """
        self.sskeys[x] = str(y)

    def _set_state_table_keywords(self):
        """
        Gather status sets here
        Not yet sure what to place here...
        """
        print "_set_state_table_keywords() called."
        statusdata = {}
        DEFAULT_VALUE = "unspecified"

        statusdata["BW_MODE"  ] = DEFAULT_VALUE;
        statusdata["CAL_DCYC" ] = DEFAULT_VALUE;
        statusdata["CAL_FREQ" ] = DEFAULT_VALUE;
        statusdata["CAL_MODE" ] = DEFAULT_VALUE;
        statusdata["CAL_PHS"  ] = DEFAULT_VALUE;
        statusdata["CHAN_BW"  ] = DEFAULT_VALUE;

        statusdata["DATADIR"  ] = DEFAULT_VALUE;
        statusdata["DATAHOST" ] = DEFAULT_VALUE;
        statusdata["DATAPORT" ] = DEFAULT_VALUE;
        statusdata["EFSAMPFR" ] = DEFAULT_VALUE;
        statusdata["EXPOSURE" ] = DEFAULT_VALUE;
        statusdata["FILENUM"  ] = DEFAULT_VALUE;
        statusdata["FPGACLK"  ] = DEFAULT_VALUE;
        statusdata["HWEXPOSR" ] = DEFAULT_VALUE;
        statusdata["M_STTMJD" ] = 0;
        statusdata["M_STTOFF" ] = 0;
        statusdata["NBITS"    ] = 8;
        statusdata["NBITSADC" ] = 8;
        statusdata["NCHAN"    ] = DEFAULT_VALUE;

        statusdata["NPKT"     ] = DEFAULT_VALUE;
        statusdata["NPOL"     ] = DEFAULT_VALUE;
        statusdata["NSUBBAND" ] = DEFAULT_VALUE;
        statusdata["OBSBW"    ] = DEFAULT_VALUE;

        statusdata["OBSFREQ"  ] = DEFAULT_VALUE;
        statusdata["OBSNCHAN" ] = DEFAULT_VALUE;
        statusdata["OBS_MODE" ] = DEFAULT_VALUE;
        statusdata["OBSERVER" ] = DEFAULT_VALUE;
        statusdata["OBSID"    ] = DEFAULT_VALUE;
        statusdata["PKTFMT"   ] = DEFAULT_VALUE;
        statusdata["SRC_NAME" ] = DEFAULT_VALUE;
        statusdata["RA"       ] = DEFAULT_VALUE
        statusdata["DEC"      ] = DEFAULT_VALUE
        statusdata["RA_STR"   ] = DEFAULT_VALUE
        statusdata["DEC_STR"  ] = DEFAULT_VALUE
        statusdata["SUB0FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB1FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB2FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB3FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB4FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB5FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB6FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB7FREQ" ] = DEFAULT_VALUE;
        statusdata["SWVER"    ] = DEFAULT_VALUE;
        statusdata["TELESCOP" ] = DEFAULT_VALUE;

        # add in the config file default keywords; being defaults they
        # may be overridden below.
        for x,y in self.mode.shmkvpairs.items():
            statusdata[x] = y
        # add in the generated keywords from the setup
        for x,y in self.sskeys.items():
            statusdata[x] = y

        statusdata["OBSERVER" ] = self.observer
        statusdata["SRC_NAME" ] = self.source

        if self.source_ra_dec:
            ra = self.source_ra_dec[0]
            dec = self.source_ra_dec[1]
            statusdata["RA"] = ra.degrees
            statusdata["DEC"] = dec.degrees
            statusdata["RA_STR"] = "%02i:%02i:%05.3f" % ra.hms
            statusdata["DEC_STR"] = apw.degreesToString(dec.degrees)

        statusdata["TELESCOP" ] = self.telescope
        statusdata["BW_MODE"  ] = "high" # mode 1
        statusdata["BOFFILE"  ] = str(self.bof_file)
        statusdata["CHAN_BW"  ] = str(self.chan_bw)
        statusdata["EFSAMPFR" ] = str(self.sampler_frequency)
        statusdata["EXPOSURE" ] = str(self.exposure)
        statusdata["FPGACLK"  ] = str(self.fpga_clock)
        statusdata["OBSNCHAN" ] = str(self.nchan)
        statusdata["OBS_MODE" ] = "HBW" # mode 1
        statusdata["OBSBW"    ] = self.obs_bw
        statusdata["PKTFMT"   ] = "SPEAD"
        statusdata["NCHAN"    ] = str(self.nchan)
        statusdata["NPOL"     ] = str(2)
        statusdata["NSUBBAND" ] = self.nsubband
        # convertToMHz() normalizes the frequency to MHz, just in case
        # it is provided as Hz. So this will work in either case.
        statusdata["SUB0FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB1FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB2FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB3FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB4FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB5FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB6FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4
        statusdata["SUB7FREQ" ] = convertToMHz(self.sampler_frequency) * 1e6 / 4

        statusdata["BASE_BW"  ] = self.filter_bw # From MODE
        statusdata["BANKNAM"  ] = self.bank.name if self.bank else 'NOBANK'
        statusdata["MODENUM"  ] = str(self.mode.name) # from MODE
        statusdata["NOISESRC" ] = "OFF"  # TBD??
        statusdata["NUMPHASE" ] = str(self.nPhases)
        statusdata["SWPERIOD" ] = str(self.ss.total_duration())
        statusdata["SWMASTER" ] = "VEGAS" # TBD
        statusdata["POLARIZE" ] = self.polarization
        statusdata["CRPIX1"   ] = str(self.nchan/2 + 1)
        statusdata["SWPERINT" ] = str(int(self.requested_integration_time \
                                          / self.ss.total_duration() + 0.5))
        statusdata["NMSTOKES" ] = str(self.num_stokes)
        # should this get set by Backend?
        statusdata["DATAHOST" ] = self.datahost;
        statusdata["DATAPORT" ] = self.dataport;
        statusdata['DATADIR'  ] = self.dataroot
        statusdata['PROJID'   ] = self.projectid
        statusdata['SCANLEN'  ] = self.scan_length
        statusdata['CAL_FREQ' ] = self.cal_freq

        for i in range(8):
            statusdata["_MCR1_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MCDL_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MFQR_%02d" % (i+1)] = str(self.frequency_resolution)

        # If self == VegasBackend then this is the instantiated object,
        # so write to status memory. If not, then this is a base class
        # and the writing should be done by the derived class, which
        # presumably will add to/overwrite this dictionary.
        if isinstance(self, VegasBackend):
            if self.bank is not None:
                self.set_status(**statusdata)
            else:
                for i in statusdata.keys():
                    print "%s = %s" % (i, statusdata[i])

        # for derived class
        return statusdata

    def earliest_start(self):
        """
        Reports earliest possible start time, in UTC, for this backend.
        """
        now = datetime.utcnow()
        earliest_start = self.round_second_up(now + self.mode.needed_arm_delay + timedelta(seconds=2))
        return earliest_start

    def start(self, starttime = None):
        """
        start(self, starttime = None)

        *starttime:* a *datetime* object, representing a UTC start time.

        --OR--

        *starttime:* a tuple or list(for ease of JSON serialization) of
        datetime compatible values: (year, month, day, hour, minute,
        second, microsecond), UTC.

        Sets up the system for a measurement and kicks it off at the
        appropriate time, based on *starttime*.  If *starttime* is not
        on a PPS boundary it is bumped up to the next PPS boundary.  If
        *starttime* is not given, the earliest possible start time is
        used.

        *start()* will require a needed arm delay time, which is
        specified in every mode section of the configuration file as
        *needed_arm_delay*. During this delay it tells the HPC program
        to start its net, accum and disk threads, and waits for the HPC
        program to report that it is receiving data. It then calculates
        the time it needs to sleep until just after the penultimate PPS
        signal. At that time it wakes up and arms the ROACH. The ROACH
        should then send the initial packet at that time.

        If this function cannot start the measurement by *starttime*, an
        exception is thrown.
        """

        self.stop() # stop any possible monitor mode first. Monitor mode
                    # harmless, but keeps things straight here.
        now = datetime.utcnow()
        earliest_start = self.round_second_up(now) + self.mode.needed_arm_delay

        if starttime:
            if type(starttime) == tuple or type(starttime) == list:
                starttime = datetime(*starttime)

            if type(starttime) != datetime:
                raise Exception("starttime must be a datetime or datetime compatible tuple or list.")

            # Force the start time to the next 1-second boundary. The
            # ROACH is triggered by a 1PPS signal.
            starttime = self.round_second_up(starttime)
            # starttime must be 'needed_arm_delay' seconds from now.

            if starttime < earliest_start:
                raise Exception("Not enough time to arm ROACH. Start: %s, earliest possible start: %s" \
                                % (str(starttime), str(earliest_start)))
        else: # No start time provided
            starttime = earliest_start

        self.start_time = starttime
        max_delay = self.mode.needed_arm_delay - timedelta(microseconds = 1500000)
        print now, starttime, max_delay
        # if simulating, just sleep until start time and return
        if self.test_mode:
            sleeptime = self.start_time - now
            sleep(sleeptime.seconds)
            return (True, "Successfully started roach for starttime=%s" % str(self.start_time))

        # everything OK now, starttime is valid, go through the start procedure.
        if self.hpc_process is None:
            self.start_hpc()
        if self.fits_writer_process is None:
            self.start_fits_writer()

        # The CODD bof's don't have a status register
        if self.needs_reset():
            self.reset_roach()

        self.hpc_cmd('START')
        self.fits_writer_cmd('START')

        status,wait = self._wait_for_status('NETSTAT', 'receiving', max_delay)

        if not status:
            self.hpc_cmd('STOP')
            self.fits_writer_cmd('STOP')
            raise Exception("start(): timed out waiting for 'NETSTAT=receiving'")

        print "start(): waited %s for HPC program to be ready." % str(wait)

        # now sleep until arm_time
        #        PPS        PPS
        # ________|__________|_____
        #          ^         ^
        #       arm_time  start_time
        arm_time = starttime - timedelta(microseconds = 900000)
        now = datetime.utcnow()

        if now > arm_time:
            self.hpc_cmd('STOP')
            self.fits_writer_cmd('STOP')
            raise Exception("start(): deadline missed, arm time is in the past.")

        tdelta = arm_time - now
        sleep_time = tdelta.seconds + tdelta.microseconds / 1e6
        time.sleep(sleep_time)
        # We're now within a second of the desired start time. Arm:
        self.arm_roach()
        self.scan_running = True
        self.set_status(ACCBLKOU='-')
        return (True, "Successfully started roach for starttime=%s" % str(self.start_time))

    def stop(self):
        """
        Stops a scan.
        """
        if self.scan_running:
            self.hpc_cmd('stop')
            self.fits_writer_cmd('stop')
            self.scan_running = False
            self.set_status(DISKSTAT='-')
            return (True, "Scan ended")

        if self.monitor_mode:
            self.hpc_cmd('stop')
            self.monitor_mode = False
            return (True, "Ending monitor mode.")

        return (False, "No scan running!")

    def monitor(self):
        """
        Tells DAQ program to enter monitor mode.
        """
        self.hpc_cmd('monitor')
        self.monitor_mode = True
        return (True, "Start monitor mode.")

    def scan_status(self):
        """
        Returns the current state of a scan, as a tuple:
        (scan_running (bool), 'NETSTAT=' (string), and 'DISKSTAT=' (string))
        """

        return (self.scan_running,
                'NETSTAT=%s' % self.get_status('NETSTAT'),
                'DISKSTAT=%s' % self.get_status('DISKSTAT'))

    def start_fits_writer(self):
        """
        start_fits_writer()
        Starts the fits writer program running. Stops any previously running instance.
        """

        if self.test_mode:
            return

        self.stop_fits_writer()
        fits_writer_program = "vegasFitsWriter"

        sp_path = self.dibas_dir + '/exec/x86_64-linux/' + fits_writer_program
        self.fits_writer_process = subprocess.Popen((sp_path, ), stdin=subprocess.PIPE)


    def stop_fits_writer(self):
        """
        stop_fits_writer()
        Stops the fits writer program and make it exit.
        To stop an observation use 'stop()' instead.
        """

        if self.test_mode:
            return

        if self.fits_writer_process is None:
            return False # Nothing to do

        try:
            # First ask nicely
            self.fits_writer_process.communicate("quit\n")
            time.sleep(1)
            # Kill if necessary
            if self.fits_writer_process.poll() == None:
                # still running, try once more
                self.fits_writer_process.terminate()
                time.sleep(1)

                if self.fits_writer_process.poll() is not None:
                    killed = True
                else:
                    self.fits_writer_process.kill()
                    killed = True;
            else:
                killed = False
            self.fits_writer_process = None
        except OSError, e:
            print "While killing child process:", e
            killed = False
        finally:
            del self.hpc_process
            self.hpc_process = None

        return killed

    def fits_writer_cmd(self, cmd):
        """
        Opens the named pipe to the fits_writer_cmd program, sends 'cmd', and closes
        the pipe. Takes care not to block on an unconnected fifo.
        """
        if self.test_mode:
            return

        if self.fits_writer_process is None:
            raise Exception( "Fits writer program has not been started" )

        fh=self.fits_writer_process.stdin.fileno()
        os.write(fh, cmd + '\n')
        return True
