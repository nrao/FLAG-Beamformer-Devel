
import struct
import ctypes
import binascii
import player
from Backend import Backend
from vegas_ssg import SwitchingSignals
import subprocess
import time
from datetime import datetime, timedelta
import os



class SWbits:
    """
    A class to hold and encode the bits of a single phase of a switching signal generator phase
    """
    SIG=0
    REF=1
    CALON=1
    CALOFF=0

class VegasBackend(Backend):
    """
    A class which implements some of the VEGAS specific parameter calculations.
    """
    def __init__(self, theBank, theMode, theRoach, theValon, unit_test = False):
        """
        Creates an instance of the vegas internals.

        VegasBackend(theBank, theMode, theRoach = None, theValon = None)

        Where:

        theBank: Instance of specific bank configuration data BankData.
        theMode: Instance of specific mode configuration data ModeData.
        theRoach: Instance of katcp_wrapper
        theValon: instance of ValonKATCP
        unit_test: Set to true to unit test. Will not attempt to talk to
        roach, shared memory, etc.
        """

        # mode_number may be treated as a constant; the Player will
        # delete this backend object and create a new one on mode
        # change.
        Backend.__init__(self, theBank, theMode, theRoach , theValon, unit_test)
        # Important to do this as soon as possible, so that status application
        # can change its data buffer format
        self.set_status(BACKEND='VEGAS')
        # In VEGAS mode, i_am_master means this particular backend
        # controls the switching signals. (self.bank is from base class.)
        self.i_am_master = self.bank.i_am_master

        # Parameters:
        self.setPolarization('SELF')
        self.setNumberChannels(self.mode.nchan)
        self.requested_integration_time = 1.0
        self.setFilterBandwidth(self.mode.filter_bw)
        self.setAccLen(self.mode.acc_len)
        self.setValonFrequency(self.mode.frequency)

        # dependent values, computed from Parameters:
        self.nspectra = 1
        self.frequency_resolution = 0.0
        self.fpga_clock = None
        self.fits_writer_process = None
        self.scan_length = 30.0

        # setup the parameter dictionary/methods
        self.params["polarization"] = self.setPolarization
        self.params["nchan"]        = self.setNumberChannels
        self.params["exposure"]     = self.setIntegrationTime
        self.params["filter_bw"]    = self.setFilterBandwidth
        self.params["num_spectra"]  = self.setNumberSpectra
        self.params["acc_len"]      = self.setAccLen
        self.params["scan_length"]  = self.setScanLength

        # the status memory key/value pair dictionary
        self.sskeys = {}
        # the switching signals builder
        self.ss = SwitchingSignals(self.frequency, self.nchan)
        self.clear_switching_states()
        self.add_switching_state(1.0, blank = False, cal = False, sig_ref_1 = False)
        self.prepare()
        self.start_hpc()
        self.start_fits_writer()


    def __del__(self):
        """
        Perform some cleanup tasks.
        """
        if self.fits_writer_process is not None:
            print "Deleting FITS writer!"
            self.stop_fits_writer()

    ### Methods to set user or mode specified parameters
    ###

    def setFilterBandwidth(self, fbw):
        self.filter_bw = fbw

    def setAccLen(self, acclen):
        """
        Not used on the VEGAS backend, usually the value is set from
        the dibas.conf configuration file.
        """
        self.acc_len = acclen
        
    def setScanLength(self, length):
        """
        This parameter controls how long the scan will last in seconds.
        """
        self.scan_length = length


    def setPolarization(self, polar):
        """
        setPolarization(x)
        where x is a string 'CROSS', 'SELF1', 'SELF2', or 'SELF'
        """

        try:
            self.num_stokes = {'CROSS': 4, 'SELF1': 1, 'SELF2': 1, 'SELF': 2}[polar]
            self.polarization = polar
        except KeyError:
            raise Exception("polarization string must be one of: CROSS, SELF1, SELF2, or SELF")

    def setNumberChannels(self, nchan):
        self.nchan = nchan

    def setADCsnap(self, snap):
        self.adc_snap = snap

    def setNumberSpectra(self, nspectra):
        self.nspectra = nspectra

    def setFilterBandwidth(self, bw):
        self.filter_bandwidth = bw

    def setIntegrationTime(self, int_time):
        self.requested_integration_time = int_time

    def prepare(self):
        """
        This command writes calculated values to the hardware and status memory.
        This command should be run prior to the first scan to properly setup
        the hardware.
        """
        # calculate the fpga_clock and sampler frequency
        self.sampler_frequency_dep()
        self.chan_bw_dep()
        self.obs_bw_dep()

        # Switching Signals info. Switching signals should have been
        # specified prior to prepare():
        self.setSSKeys()

        # now update all the status keywords needed for this mode:
        self.set_state_table_keywords()

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

    def chan_bw_dep(self):
        self.chan_bw = self.sampler_frequency / (self.nchan * 2)
        self.frequency_resolution = abs(self.chan_bw)

    def sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """

        # extract mode number from mode name, which is expected to be
        # 'MODEx' where 'x' is the number we want:
        mode = int(self.mode.mode[4:])

        if mode < 13:
            self.sampler_frequency = self.frequency * 2
            self.nsubband = 1
        else:
            self.sampler_frequency = self.frequency / 64
            self.nsubband = 8
        # calculate the fpga frequency
        self.fpga_clock = self.frequency / 8


    def clear_switching_states(self):
        """
        resets/deletes the switching_states
        """
        self.ss.clear_phases()
        return (True, self.ss.number_phases())

    def add_switching_state(self, duration, blank = False, cal = False, sig_ref_1 = False):
        """
        add_switching_state(duration, blank, cal, sig):

        Add a description of one switching phase.
        Where:
            duration is the length of this phase in seconds,
            blank is the state of the blanking signal (True = blank, False = no blank)
            cal is the state of the cal signal (True = cal, False = no cal)
            sig is the state of the sig_ref signal (True = ref, false = sig)

        Example to set up a 8 phase signal (4-phase if blanking is not
        considered) with blanking, cal, and sig/ref, total of 400 mS:
          be = Backend(None) # no real backend needed for example
          be.clear_switching_states()
          be.add_switching_state(0.01, blank = True, cal = True, sig = True)
          be.add_switching_state(0.09, cal = True, sig = True)
          be.add_switching_state(0.01, blank = True, cal = True)
          be.add_switching_state(0.09, cal = True)
          be.add_switching_state(0.01, blank = True, sig = True)
          be.add_switching_state(0.09, sig = True)
          be.add_switching_state(0.01, blank = True)
          be.add_switching_state(0.09)
        """
        self.ss.add_phase(dur = duration, bl = blank, cal = cal, sr1 = sig_ref_1)
        return (True, self.ss.number_phases())

    def set_gbt_ss(self, period, ss_list):
        """
        set_gbt_ss(period, ss_list):

        adds a complete GBT style switching signal description.

        period: The complete period length of the switching signal.
        ss_list: A list of GBT phase components. Each component is a tuple:
        (phase_start, sig_ref, cal, blanking_time)
        There is one of these tuples per GBT style phase.

        Example:
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
                self.add_switching_state(blt, sig_ref_1 = ss_list[i][1], cal = ss_list[i][2], blank = True)
                self.add_switching_state(nblt, sig_ref_1 = ss_list[i][1], cal = ss_list[i][2], blank = False)
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


    def setSSKeys(self):
        self.sskeys.clear()
        states = self.ss.gbt_phase_starts()
        cal = states['cal']
        sig_ref_1 = states['sig/ref']
        self.nPhases = len(sig_ref_1)
        empty_list = [0 for i in range(self.nPhases)] # For sig_ref_2, or I or E as appropriate

        for i in range(len(states['blanking'])):
            self.set_status_str('_SBLK_%02d' % (i+1), states['blanking'][i])

        for i in range(len(cal)):
            self.set_status_str('_SCAL_%02d' % (i+1), cal[i])

        for i in range(len(states['phase-starts'])):
            self.set_status_str('_SPHS_%02d' % (i+1), states['phase-starts'][i])

        for i in range(len(sig_ref_1)):
            self.set_status_str('_SSRF_%02d' % (i+1), sig_ref_1[i])

        master = self.i_am_master # TBF! Make sure this exists...
        self.setEcal(empty_list if master else cal)
        self.setEsigRef1(empty_list if master else sig_ref_1)
        self.setEsigRef2(empty_list)
        self.setIcal(cal if master else empty_list)
        self.setIsigRef1(sig_ref_1 if master else empty_list)
        self.setIsigRef2(empty_list)
        # self.sskeys now populated, and will be written with other status keys/vals.

    def setEcal(self, cals):
        """
        External CAL

        cals is a list of integers where
        1 indicates the external cal is ON
        0 indicates the external cal is OFF
        """
        for i in range(len(cals)):
            self.set_status_str('_AECL_%02d' % (i+1), cals[i])

    def setEsigRef1(self, sr):
        """
        External Sig/Ref 1

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self.set_status_str('_AESA_%02d' % (i+1), sr[i])

    def setEsigRef2(self, sr):
        """
        External Sig/Ref 2

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self.set_status_str('_AESB_%02d' % (i+1), sr[i])

    def setIcal(self, cals):
        """
        Internal CAL

        cals is a list of integers where
        1 indicates the external cal is ON
        0 indicates the external cal is OFF
        """
        for i in range(len(cals)):
            self.set_status_str('_AICL_%02d' % (i+1), cals[i])

    def setIsigRef1(self, sr):
        """
        Internal Sig/Ref 1

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self.set_status_str('_AISA_%02d' % (i+1), sr[i])

    def setIsigRef2(self, sr):
        """
        Internal Sig/Ref 2

        sr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(sr)):
            self.set_status_str('_AISB_%02d' % (i+1), sr[i])

    def obs_bw_dep(self):
        """
        Observation bandwidth dependency
        """
        self.obs_bw = self.chan_bw * self.nchan

    def set_status_str(self, x, y):
        """
        Add/update an item to the status memory keyword list
        """
        self.sskeys[x] = str(y)

    def set_state_table_keywords(self):
        """
        Gather status sets here
        Not yet sure what to place here...
        """
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
        statusdata["OBSSEVER" ] = DEFAULT_VALUE;
        statusdata["OBSID"    ] = DEFAULT_VALUE;
        statusdata["PKTFMT"   ] = DEFAULT_VALUE;

        statusdata["SUB0FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB1FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB2FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB3FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB4FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB5FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB6FREQ" ] = DEFAULT_VALUE;
        statusdata["SUB7FREQ" ] = DEFAULT_VALUE;
        statusdata["SWVER"    ] = DEFAULT_VALUE;

        # add in the generated keywords from the setup
        for x,y in self.sskeys.items():
            statusdata[x] = y

        statusdata["BW_MODE"  ] = "high" # mode 1
        statusdata["BOFFILE"  ] = str(self.bof_file)
        statusdata["CHAN_BW"  ] = str(self.chan_bw)
        statusdata["EFSAMPFR" ] = str(self.sampler_frequency)
        statusdata["EXPOSURE" ] = str(self.requested_integration_time)
        statusdata["FPGACLK"  ] = str(self.fpga_clock)
        statusdata["OBSNCHAN" ] = str(self.nchan)
        statusdata["OBS_MODE" ] = "HBW" # mode 1
        statusdata["OBSBW"    ] = self.obs_bw
        statusdata["PKTFMT"   ] = "SPEAD"
        statusdata["NCHAN"    ] = str(self.nchan)
        statusdata["NPOL"     ] = str(2)
        statusdata["NSUBBAND" ] = self.nsubband
        statusdata["SUB0FREQ" ] = self.frequency / 2
        statusdata["SUB1FREQ" ] = self.frequency / 2
        statusdata["SUB2FREQ" ] = self.frequency / 2
        statusdata["SUB3FREQ" ] = self.frequency / 2
        statusdata["SUB4FREQ" ] = self.frequency / 2
        statusdata["SUB5FREQ" ] = self.frequency / 2
        statusdata["SUB6FREQ" ] = self.frequency / 2
        statusdata["SUB7FREQ" ] = self.frequency / 2

        statusdata["BASE_BW"  ] = self.filter_bandwidth # From MODE
        statusdata["BANKNAM"  ] = self.bank.name if self.bank else 'NOBANK'
        statusdata["MODENUM"  ] = str(self.mode.mode) # from MODE
        statusdata["NOISESRC" ] = "OFF"  # TBD??
        statusdata["NUMPHASE" ] = str(self.nPhases)
        statusdata["SWPERIOD" ] = str(self.ss.total_duration())
        statusdata["SWMASTER" ] = "VEGAS" # TBD
        statusdata["POLARIZE" ] = self.polarization
        statusdata["CRPIX1"   ] = str(self.nchan/2 + 1)
        statusdata["SWPERINT" ] = str(int(self.requested_integration_time / self.ss.total_duration()))
        statusdata["NMSTOKES" ] = str(self.num_stokes)
        # should this get set by Backend?
        statusdata["DATAHOST" ] = self.datahost;
        statusdata["DATAPORT" ] = self.dataport;
        statusdata['DATADIR'  ]  = self.dataroot
        statusdata['PROJID'   ]  = self.projectid
        statusdata['SCANLEN'  ]  = self.scan_length        

        for i in range(8):
            statusdata["_MCR1_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MCDL_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MFQR_%02d" % (i+1)] = str(self.frequency_resolution)

        if self.bank is not None:
            self.set_status(**statusdata)
        else:
            for i in statusdata.keys():
                print "%s = %s" % (i, statusdata[i])


    def earliest_start(self):
        now = datetime.now()
        earliest_start = self.round_second_up(now + self.mode.needed_arm_delay)
        return earliest_start

    def start(self, starttime = None):
        """
        start(self, starttime = None)

        starttime: a datetime object

        --OR--

        starttime: a tuple or list(for ease of JSON serialization) of
        datetime compatible values: (year, month, day, hour, minute,
        second, microsecond).

        Sets up the system for a measurement and kicks it off at the
        appropriate time, based on 'starttime'.  If 'starttime' is not
        on a PPS boundary it is bumped up to the next PPS boundary.  If
        'starttime' is not given, the earliest possible start time is
        used.

        start() will require a needed arm delay time, which is specified
        in every mode section of the configuration file as
        'needed_arm_delay'. During this delay it tells the HPC program
        to start its net, accum and disk threads, and waits for the HPC
        program to report that it is receiving data. It then calculates
        the time it needs to sleep until just after the penultimate PPS
        signal. At that time it wakes up and arms the ROACH. The ROACH
        should then send the initial packet at that time.
        """

        if self.hpc_process is None:
            self.start_hpc()
        if self.fits_writer_process is None:
            self.start_fits_writer()

        now = datetime.now()
        earliest_start = self.earliest_start()

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
                raise Exception("Not enough time to arm ROACH.")
        else: # No start time provided
            starttime = earliest_start
        # everything OK now, starttime is valid, go through the start procedure.
        max_delay = self.mode.needed_arm_delay - timedelta(microseconds = 1500000)
        print now, starttime, max_delay

        # The CODD bof's don't have a status register
        if not self.mode.cdd_mode:
            val = self.roach.read_int('status')
            if val & 0x01:
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
        now = datetime.now()

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

    def stop(self):
        """
        Stops a scan.
        """
        if self.scan_running:
            self.hpc_cmd('stop')
            self.fits_writer_cmd('stop')
            self.scan_running = False
            return (True, "Scan ended")
        else:
            return (False, "No scan running!")

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

        self.stop_fits_writer()
        fits_writer_program = "vegasFitsWriter"

        sp_path = self.dibas_dir + '/exec/x86_64-linux/' + fits_writer_program
        self.fits_writer_process = subprocess.Popen((sp_path, ))


    def stop_fits_writer(self):
        """
        stop_fits_writer()
        Stops the fits writer program and make it exit.
        To stop an observation use 'stop()' instead.
        """
        if self.fits_writer_process is None:
            return False # Nothing to do

        # First ask nicely
        self.fits_writer_cmd('stop')
        self.fits_writer_cmd('quit')
        time.sleep(1)
        # Kill and reclaim child
        self.fits_writer_process.communicate()
        # Kill if necessary
        if self.fits_writer_process.poll() == None:
            # still running, try once more
            self.fits_writer_process.communicate()
            time.sleep(1)

            if self.fits_writer_process.poll() is not None:
                killed = True
            else:
                self.fits_writer_process.communicate()
                killed = True;
        else:
            killed = False
        self.fits_writer_process = None
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

        fifo_name = "/tmp/vegas_fits_control"

        try:
            fh = os.open("/tmp/vegas_fits_control", os.O_WRONLY | os.O_NONBLOCK)
        except:
            print "fifo open for fits writer program failed"
            raise
            return False

        os.write(fh, cmd + '\n')
        os.close(fh)
        return True

######################################################################
# TBF: Make these work!

def testCase1():
    """
    An example test case FWIW.
    """
    global be

    be = VegasBackend(None)
    # A few things which should come from the conf file via the bank
    # b.bank_name='BankH'
    be.mode = 1 ## get this from bank when bank is not None
    be.acc_len = 768 ## from MODE config

    be.clear_switching_states()
    ssg_duration = 0.025
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.0),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.0),
                   (0.5, SWbits.REF, SWbits.CALON, 0.0),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.0))
                  )

    be.setValonFrequency(1E9)
    be.setPolarization('SELF')
    be.setNumberChannels(1024) # mode 1
    be.setFilterBandwidth(800E6)
    be.setIntegrationTime(ssg_duration)

    # call dependency methods and update shared memory
    be.prepare()

def testCase2():
    """
    An example test case from configtool setup.
    """

    global be

    config = ConfigParser.ConfigParser()
    config.readfp(open("dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE1")

    be = VegasBackend(b, m, None, None, unit_test = True)
    # A few things which should come from the conf file via the bank
#    be.mode = 1 ## get this from bank?
#    be.acc_len = 768 ## from MODE config

    be.clear_switching_states()
    ssg_duration = 0.1
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    be.setValonFrequency(1E9)    # config file
    be.setPolarization('SELF')
    be.setNumberChannels(1024)   # mode 1 (config file)
    be.setFilterBandwidth(800E6) # config file?
    be.setIntegrationTime(ssg_duration)

    # call dependency methods and update shared memory
    be.prepare()


def testCase3():
    """
    Example of how to set up a VEGAS-style 8-phase switching signal of
    duration 400mS, CAL on for 200 mS then off, and sig/ref switching
    every 100 mS. Blanking occurs on every phase transition of CAL and
    sig/ref.
    """
    global be
    be = VegasBackend(None)
    be.mode = 1 ## get this from bank?
    be.acc_len = 768 ## from MODE config

    be.setValonFrequency(1E9)    # config file
    be.setPolarization('SELF')
    be.setNumberChannels(1024)   # mode 1 (config file)
    be.setFilterBandwidth(800E6) # config file?
    be.setIntegrationTime(0.4)

    be.clear_switching_states()
    be.add_switching_state(0.01, blank = True,  cal = True,  sig = True)
    be.add_switching_state(0.09, blank = False, cal = True,  sig = True)
    be.add_switching_state(0.01, blank = True,  cal = True,  sig = False)
    be.add_switching_state(0.09, blank = False, cal = True,  sig = False)
    be.add_switching_state(0.01, blank = True,  cal = False, sig = True)
    be.add_switching_state(0.09, blank = False, cal = False, sig = True)
    be.add_switching_state(0.01, blank = True,  cal = False, sig = False)
    be.add_switching_state(0.09, blank = False, cal = False, sig = False)

    # # call dependency methods and update shared memory
    be.prepare()


if __name__ == "__main__":

    # testCase1()
    testCase2()
    # testCase3()
