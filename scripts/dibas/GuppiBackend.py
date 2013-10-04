import struct
import ctypes
import binascii
import player
import math
import time
from datetime import datetime, timedelta
from Backend import Backend
import os

class GuppiBackend(Backend):
    """
    A class which implements some of the GUPPI specific parameter calculations.
    This class is specific to the Incoherent BOF designs.

    GuppiBackend(theBank, theMode, theRoach, theValon, unit_test)

    * *theBank:* A *BankData* object, bank data from the configuration file.
    * *theMode:* A *ModeData* object, mode data from the configuration file
    * *theRoach:* A *katcp_wrapper* object, the katcp client to the FPGA
    * *theValon:* A *ValonKATCP* object, the interface to the ROACH's Valon synthesizer
    * *unit_test:* Unit test flag; set to *True* if unit testing,
      *False* if not. Allows unit testing without involving the
      hardware.
    """
    def __init__(self, theBank, theMode, theRoach, theValon, unit_test = False):
        """
        Creates an instance of the vegas internals.
        GuppiBackend( bank )
        Where bank is the instance of the player's Bank.
        """
        Backend.__init__(self, theBank, theMode, theRoach, theValon, unit_test)
        # This needs to happen on construct so that status monitors can
        # switch their data buffer format
        self.set_status(BACKEND="GUPPI")
        # The default switching in the Backend ctor is a static SIG, NOCAL, and no blanking

        # defaults
        self.max_databuf_size = 128 # in MBytes [Not sure where this ties in. Value from the manager]

        self.obs_mode = 'SEARCH'
        """Parameter 'obs_mode': GUPPI observation mode"""
        self.nchan = self.mode.nchan
        """Parameter 'nchan': Number of channels"""
        self.integration_time = 40.96E-6
        """Parameter 'integration_time': Lenght of integration, in seconds."""
        self.overlap = 0
        self.scale_i = 1
        self.scale_q = 1
        self.scale_u = 1
        self.scale_v = 1
        self.offset_i = 0
        self.offset_q = 0
        self.offset_u = 0
        self.offset_v = 0
        self.only_i = 0
        self.bandwidth = 1500.0
        self.chan_dm = 0.0
        self.rf_frequency = 2000.0
        self.nbin = 256
        self.tfold = 1.0
        self.dm = 0.0
        # Almost all receivers are dual polarization
        self.nrcvr = 2
        self.feed_polarization = 'LIN'


        if self.dibas_dir is not None:
           self.pardir = self.dibas_dir + '/etc/config'
        else:
            self.pardir = '/tmp'
        self.parfile = 'example.par'

        self.params["bandwidth"         ] = self.set_bandwidth
        self.params["integration_time"  ] = self.set_integration_time
        self.params["nbin"              ] = self.set_nbin
        self.params["obs_frequency"     ] = self.set_obs_frequency
        self.params["obs_mode"          ] = self.set_obs_mode
        self.params["only_i"            ] = self.set_only_i
        self.params["offset_i"          ] = self.set_offset_I
        self.params["offset_q"          ] = self.set_offset_Q
        self.params["offset_u"          ] = self.set_offset_U
        self.params["offset_v"          ] = self.set_offset_V
        self.params["scale_i"           ] = self.set_scale_I
        self.params["scale_q"           ] = self.set_scale_Q
        self.params["scale_u"           ] = self.set_scale_U
        self.params["scale_v"           ] = self.set_scale_V
        self.params["tfold"             ] = self.set_tfold
        self.params["feed_polarization" ] = self.setFeedPolarization
        self._fft_params_dep()

    ### Methods to set user or mode specified parameters
    ### Not sure how these map for GUPPI

    # TBF, all bandwidth probably belongs in base class
    def set_bandwidth(self, bandwidth):
        """
        Sets the bandwidth in MHz. This value should match the valon output frequency.
        (The sampling rate being twice the valon frequency.)
        """
        if  abs(bandwidth) > 200 and abs(bandwidth) < 2000:
            self.bandwidth = bandwidth
        else:
            raise Exception("Bandwidth of %d MHz is not a legal bandwidth setting" % (bandwidth))

    def set_chan_dm(self, dm):
        """
        Sets the dispersion measure for coherent search modes.
        Other modes should have this set to zero.
        """
        pass

    def setFeedPolarization(self, polar):
        """
        Sets the FD_POLN (feed polarization) keyword in status memory and PSR FITS files.
        Legal values are 'LIN' (linear) or 'CIRC' (circular)
        """
        if isinstance(polar, str) and polar.upper() in ['LIN', 'CIRC']:
            self.feed_polarization = polar
        else:
            raise Exception("bad value: legal values are 'LIN' (linear) or 'CIRC' (circular)")

    def set_par_file(self, file):
        """
        Sets the pulsar profile ephemeris file
        """
        self.parfile = file

    def set_nbin(self, nbin):
        """
        For cal and fold modes, this sets the number of bins in a pulse profile.
        Ignored in other modes.
        """
        self.nbin = nbin

    def set_obs_mode(self, mode):
        """
        Sets the observing mode.
        Legal values for the currently selected mode are:
        SEARCH, FOLD, CAL, or RAW
        """
        # only incoherent modes. Coherent modes handled by GuppiCODDBackend class.
        legalmodes = ["SEARCH", "FOLD", "CAL", "RAW"]
        m = mode.upper()
        if m in legalmodes:
            self.obs_mode = m
        else:
            raise Exception("set_obs_mode: mode must be one of %s" % str(legalmodes))

    def set_obs_frequency(self, f):
        """
        Sets the center frequency of the observing band.
        """
        self.rf_frequency = f

    def set_integration_time(self, integ_time):
        """
        Sets the integration time. The actual value used may be adjusted to make the interval
        be an even multiple of the hardware accumulation rate. (Actual value in TBIN keyword.)
        """
        self.integration_time = integ_time

    def set_scale_I(self, v):
        """
        Sets the hardware scaling factor for the I stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.scale_i = v

    def set_scale_Q(self, v):
        """
        Sets the hardware scaling factor for the Q stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.scale_q = v

    def set_scale_U(self, v):
        """
        Sets the hardware scaling factor for the U stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.scale_u = v

    def set_scale_V(self, v):
        """
        Sets the hardware scaling factor for the V stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.scale_v = v

    def set_offset_I(self, v):
        """
        Sets the hardware offset factor for the I stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.offset_i = v

    def set_offset_Q(self, v):
        """
        Sets the hardware offset factor for the I stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.offset_q = v

    def set_offset_U(self, v):
        """
        Sets the hardware offset factor for the I stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.offset_u = v

    def set_offset_V(self, v):
        """
        Sets the hardware offset factor for the I stokes parameter.
        Range is 0.0 through 65535.99998.
        """
        self.offset_v = v

    def set_tfold(self, tf):
        """
        Sets the software integration time per profile for all folding and cal modes.
        This is ignored in other modes.
        """
        self.tfold = tf

    def set_only_i(self, only_i):
        """
        Controls whether to 'record only summed polarizations' mode. Zero indicates that
        full stokes data should be recorded. One means to record only summed polarizations.
        This will be set to zero when using the 'FAST4K' observing mode.
        """
        self.only_i = only_i

    def prepare(self):
        """
        A place to hang the dependency methods.
        """

        self._hw_nchan_dep()
        self._acc_len_dep()
        self._chan_bw_dep()
        self._ds_time_dep()
        self._ds_freq_dep()
        self._pfb_overlap_dep()
        self._pol_type_dep()
        self._tbin_dep()
        self._only_I_dep()
        self._packet_format_dep()
        self._npol_dep()
        self._tfold_dep()
        self._node_bandwidth_dep()

        self._set_registers()
        self._set_status_keys()

        # program I2C: input filters, noise source, noise or tone
        self.set_if_bits()

        # The prepare after construction, starts the HPC and
        # arm's the roach. This gets packets flowing. If the roach is
        # not primed, the start() will fail because of the state of the
        # net thread being 'waiting' instead of 'receiving'
        if self.hpc_process is None:
            self.start_hpc()
            time.sleep(5)
            self.arm_roach()

    def earliest_start(self):
        """
        Returns the earliest time this backend can start.
        """
        now = datetime.utcnow()
        earliest_start = self.round_second_up(now + self.mode.needed_arm_delay + timedelta(seconds=2))
        return earliest_start

    def _start(self, starttime):
        """
        start(self, starttime = None)

        *starttime:* a datetime object

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
        'needed_arm_delay'. During this delay it tells the HPC program
        to start its net, accum and disk threads, and waits for the HPC
        program to report that it is receiving data. It then calculates
        the time it needs to sleep until just after the penultimate PPS
        signal. At that time it wakes up and arms the ROACH. The ROACH
        should then send the initial packet at that time.
        """

        if self.hpc_process is None:
            self.start_hpc()

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
                raise Exception("Not enough time to arm ROACH.")
        else: # No start time provided
            starttime = earliest_start
        # everything OK now, starttime is valid, go through the start procedure.
        self.start_time = starttime
        max_delay = self.mode.needed_arm_delay - timedelta(microseconds = 1500000)
        print now, starttime, max_delay

        self.hpc_cmd('START')
        status,wait = self._wait_for_status('NETSTAT', 'receiving', max_delay)

        if not status:
            self.hpc_cmd('STOP')
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
            self.scan_running = False
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

    # Algorithmic dependency methods, not normally called by users

    def _acc_len_dep(self):
        """
        Calculates the hardware accumulation length.
        The register values must be in the range of 0 to 65535, in even powers of two, minus one.
        """
        acc_length = 2**int(math.log(int(self.integration_time * abs(self.bandwidth) * 1E6/self.hw_nchan + 0.5))/math.log(2))-1

        if acc_length < 0 or acc_length > 65535:
            raise Exception("Hardware accumulation length too long. Reduce integration time or bandwidth.")
        else:
            self.acc_length = acc_length
            self.acc_len = self.acc_length+1

    def _chan_bw_dep(self):
        """
        Calculates the CHAN_BW status keyword
        Result is bandwidth of each channel in MHz
        """
        self.obsnchan = self.hw_nchan

        chan_bw = self.bandwidth / float(self.hw_nchan)
        #if self.bandwidth < 800:
        #    chan_bw = -1.0 * chan_bw
        self.chan_bw = chan_bw

    def _ds_time_dep(self):
        """
        Calculate the down-sampling time status keyword
        """

        #if 'SEARCH' in self.obs_mode:
        #    dst = self.integration_time * self.bandwidth * 1E6 / self.nchan
        #    power_of_two = 2 ** int(math.log(dst)/math.log(2))
        #    self.ds_time = power_of_two
        #else:
        # Paul indicated that in incoherent modes ds_time should always be 1
        self.ds_time = 1

    def _ds_freq_dep(self):
        """
        Calculate the DS_FREQ status keyword.
        This is used only when an observer wants to reduce the number of channels
        in software, while using a higher number of hardware channels in SEARCH
        or COHERENT_SEARCH modes.
        """
        if self.obs_mode.upper() in ["SEARCH", "COHERENT_SEARCH"]:
            self.ds_freq = self.hw_nchan / self.nchan
        else:
            self.ds_freq = 1

    def _hw_nchan_dep(self):
        """
        Can't find direct evidence for this, but seemed logical ...
        """
        if 'COHERENT' in self.obs_mode:
            self.hw_nchan = self.nchan # number of nodes
        else:
            self.hw_nchan = self.nchan
        self.node_nchan = self.hw_nchan

    def _pfb_overlap_dep(self):
        """
        Randy/Jason indicated that the new guppi designs will have 12 taps in all modes.
        """
        self.pfb_overlap = 12

    def _pol_type_dep(self):
        """
        Calculates the POL_TYPE status keyword.
        Depends upon a synthetic mode name having FAST4K for that mode, otherwise
        non-4k coherent mode is assumed.
        """
        if 'COHERENT' in self.obs_mode:
            self.pol_type = 'AABBCRCI'
        elif 'FAST4K' in self.mode.name.upper():
            self.pol_type = 'AA+BB'
        else:
            self.pol_type = 'IQUV'

    def _npol_dep(self):
        """
        Calculates the number of polarizations to be recorded.
        Most cases it is all four, except in FAST4K, or when the user
        has indicated they only want 1 stokes product)
        """
        self.npol = 4
        if 'FAST4K' in self.mode.name.upper():
            self.npol   = 1
        elif self.only_i:
            self.npol = 1

    def _node_bandwidth_dep(self):
        """
        Calculations the bandwidth seen by this HPC node
        """
        if 'COHERENT' in self.obs_mode:
            self.node_bandwidth = self.bandwidth / 8
        else:
            self.node_bandwidth = self.bandwidth

    def _tbin_dep(self):
        """
        Calculates the TBIN status keyword
        """
        self.tbin = float(self.acc_len * self.hw_nchan) / (abs(self.bandwidth)*1E6)

    def _tfold_dep(self):
        if 'COHERENT' == self.obs_mode:
            self.fold_time = 1



    def _packet_format_dep(self):
        """
        Calculates the PKTFMT status keyword
        """
        if 'FAST4K' in self.mode.name.upper():
            self.packet_format = 'FAST4K'
        else:
            self.packet_format = '1SFA'


    def _only_I_dep(self):
        """
        Calculates the ONLY_I status keyword
        """
        # Note this requires that the config mode name contains 'FAST4K' in the name
        if 'FAST4K' in self.mode.name.upper():
            self.only_i = 0
        elif self.obs_mode.upper() not in ["SEARCH", "COHERENT_SEARCH"]:
            self.only_i = 0

    def _set_status_keys(self):
        """
        Collect the status keywords
        """
        statusdata = {}
        statusdata['ACC_LEN' ] = self.acc_len
        statusdata["BASE_BW" ] = self.filter_bw
        statusdata["BANKNAM" ] = self.bank.name if self.bank else 'NOTSET'        
        statusdata['BLOCSIZE'] = self.blocsize
        statusdata['CHAN_DM' ] = self.dm
        statusdata['CHAN_BW' ] = self.chan_bw
        statusdata['DATADIR' ] = self.dataroot
        statusdata['DATAHOST'] = self.datahost
        statusdata['DATAPORT'] = self.dataport
        statusdata['PROJID'  ] = self.projectid
        statusdata['OBSERVER'] = self.observer
        statusdata['DS_TIME' ] = self.ds_time
        statusdata['SCANLEN' ] = self.scan_length
        statusdata['FFTLEN'  ] = self.fft_len
        statusdata['FD_POLN' ] = self.feed_polarization

        statusdata['NPOL'    ] = self.npol
        statusdata['NRCVR'   ] = self.nrcvr
        statusdata['NBIN'    ] = self.nbin
        statusdata['NBITS'   ] = 8

        statusdata['OBSFREQ' ] = self.rf_frequency
        statusdata['OBSBW'   ] = self.node_bandwidth
        statusdata['OBSNCHAN'] = repr(self.node_nchan)
        statusdata['OBS_MODE'] = self.obs_mode
        statusdata['OFFSET0' ] = '0.0'
        statusdata['OFFSET1' ] = '0.0'
        statusdata['OFFSET2' ] = '0.0'
        statusdata['OFFSET3' ] = '0.0'
        statusdata['ONLY_I'  ] = self.only_i
        statusdata['OVERLAP' ] = self.overlap


        statusdata['POL_TYPE'] = self.pol_type
        statusdata['PFB_OVER'] = self.pfb_overlap
        if self.parfile is not None:
            if self.parfile[0] == '/':
                statusdata['PARFILE'] = self.parfile
            else:
                statusdata['PARFILE'] = '%s/%s' % (self.pardir, self.parfile)
        statusdata['PKTFMT'  ] = self.packet_format

        statusdata['SCALE0'  ] = '1.0'
        statusdata['SCALE1'  ] = '1.0'
        statusdata['SCALE2'  ] = '1.0'
        statusdata['SCALE3'  ] = '1.0'
        statusdata['TBIN'    ] = self.tbin
        statusdata['TFOLD'   ] = self.tfold

        self.set_status(**statusdata)

    def _set_registers(self):
        regs = {}

        if not self.test_mode:
            self.valon.set_frequency(0, abs(self.bandwidth))
        regs['ACC_LENGTH'] = self.acc_length
        regs['SCALE_I']    = int(self.scale_i*65536)
        regs['SCALE_Q']    = int(self.scale_q*65536)
        regs['SCALE_U']    = int(self.scale_u*65536)
        regs['SCALE_V']    = int(self.scale_v*65536)
        regs['OFFSET_I']   = int(self.offset_i*65536)
        regs['OFFSET_Q']   = int(self.offset_q*65536)
        regs['OFFSET_U']   = int(self.offset_u*65536)
        regs['OFFSET_V']   = int(self.offset_v*65536)
        #regs['FFT_SHIFT'] = 0xaaaaaaaa (Set by config file)

        self.set_register(**regs)

    def _fft_params_dep(self):
        """
        Calculate the FFTLEN, and BLOCSIZE status keywords
        """
        self.fft_len = 16384
        self.blocsize = 33554432 # defaults
