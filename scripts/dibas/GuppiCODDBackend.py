import struct
import ctypes
import binascii
import player
import math
import time
from datetime import datetime, timedelta
from Backend import Backend
import os
import sys
import traceback
from set_arp import set_arp

def formatExceptionInfo(maxTBlevel=5):
    """
    Obtains information from the last exception thrown and extracts
    the exception name, data and traceback, returning them in a tuple
    (string, string, [string, string, ...]).  The traceback is a list
    which will be 'maxTBlevel' deep.
    """
    cla, exc, trbk = sys.exc_info()
    excName = cla.__name__
    excArgs = exc.__str__()
    excTb = traceback.format_tb(trbk, maxTBlevel)
    return (excName, excArgs, excTb)

def printException(formattedException):
    """
    Takes the tuple provided by 'formatExceptionInfo' and prints it
    out exactly as an uncaught exception would be in an interactive
    python shell.
    """
    print "Traceback (most recent call last):"

    for i in formattedException[2]:
        print i,

    print "%s: %s" % (formattedException[0], formattedException[1])

class GuppiCODDBackend(Backend):
    """
    A class which implements some of the GUPPI specific parameter calculations.
    This class is specific to the coherent mode BOF designs.

    GuppiCODDBackend(theBank, theMode, theRoach, theValon, unit_test)

    * *theBank:* A *BankData* object, bank data from the configuration file.
    * *theMode:* A *ModeData* object, mode data from the configuration file
    * *theRoach:* A *katcp_wrapper* object, the katcp client to the FPGA
    * *theValon:* A *ValonKATCP* object, the interface to the ROACH's Valon synthesizer
    * *unit_test:* Unit test flag; set to *True* if unit testing,
      *False* if not. Allows unit testing without involving the
      hardware.
    """
    def __init__(self, theBank, theMode, theRoach, theValon, hpc_macs, unit_test = False):
        """
        Creates an instance of the class.
        """

        # Only one HPC Player will be controlling a roach in the CODD
        # mode family. Therefore figure out if we are the one; if not,
        # set the parameters 'theRoach' and 'theValon' to None, even if
        # this HPC does control a roach in other modes.

        if theMode.cdd_master_hpc == theBank.name:
            Backend.__init__(self, theBank, theMode, theRoach, theValon, hpc_macs, unit_test)
        else:
            Backend.__init__(self, theBank, theMode, None, None, None, unit_test)

        # This needs to happen on construction so that status monitors can
        # change their data buffer format.
        self.set_status(BACKEND="GUPPI")
        # The default switching in the Backend ctor is a static SIG, NOCAL, and no blanking

        self.max_databuf_size = 128 # in MBytes
        self.scale_p0 = 1.0
        self.scale_p1 = 1.0
        self.only_i = 0
        self.bandwidth = self.frequency
        self.dm = 0.0
        self.rf_frequency = 1430.0
        self.overlap = 0
        self.tfold = 1.0
        self.nbin = 256
        # Most all receivers are dual polarization
        self.nrcvr = 2
        self.nchan = self.mode.nchan # total number of channels in the design, not per node
        self.num_nodes = 8
        self.feed_polarization = 'LIN'
        self.obs_mode = 'COHERENT_SEARCH'
        bank_names = {'A' : 0, 'B' : 1, 'C' : 2, 'D' : 3, 'E' : 4, 'F' : 5, 'G' : 6, 'H' : 7 }
        self.node_number = bank_names[self.bank.name[-1]]
        self.integration_time =40.96E-6
        self.scan_length = 30.0
        if self.dibas_dir is not None:
            self.pardir = self.dibas_dir + '/etc/config'
        else:
            self.pardir = '/tmp'
        self.parfile = 'example.par'
        self.datadir = '/lustre/gbtdata/JUNK' # Needs integration with projectid
        # register set methods
#        self.params["bandwidth"         ] = self.set_bandwidth
        self.params["dm"                ] = self.set_dm
        self.params["integration_time"  ] = self.set_integration_time
        self.params["nbin"              ] = self.set_nbin
        self.params["num_channels"      ] = self.set_nchannels
        self.params["obs_frequency"     ] = self.set_obs_frequency
        self.params["obs_mode"          ] = self.set_obs_mode
        self.params["par_file"          ] = self.set_par_file
        self.params["scale_p0"          ] = self.set_scale_P0
        self.params["scale_p1"          ] = self.set_scale_P1
        self.params["tfold"             ] = self.set_tfold
        self.params["only_i"            ] = self.set_only_i
        self.params["feed_polarization" ] = self.setFeedPolarization
        self.params["_node_number"      ] = self.setNodeNumber

        # Fill-in defaults if they exist
        if 'OBS_MODE' in self.mode.shmkvpairs.keys():
            self.set_param('obs_mode',   self.mode.shmkvpairs['OBS_MODE'])
        if 'ONLY_I' in self.mode.shmkvpairs.keys():
            self.set_param('only_i',   int(self.mode.shmkvpairs['ONLY_I']))

        if 'SCALE_P0' in self.mode.roach_kvpairs.keys():
            self.set_param('scale_p0', float(self.mode.roach_kvpairs['SCALE_P0']))
        if 'SCALE_P1' in self.mode.roach_kvpairs.keys():
            self.set_param('scale_p1', float(self.mode.roach_kvpairs['SCALE_P1']))

        if self.hpc_process is None:
            self.start_hpc()

        if self.cdd_master():
            self.arm_roach()

    def cdd_master(self):
        """
        Returns 'True' if this is a CoDD backend and it is master. False otherwise.
        """
        return self.bank.name == self.mode.cdd_master_hpc

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

    def set_scale_P0(self, p):
        """
        Sets the hardware scaling factor for the p0 polarization.
        Range is 0.0 through 65535.99998.
        """
        self.scale_p0 = p

    def set_scale_P1(self, p):
        """
        Sets the hardware scaling factor for the p1 polarization.
        Range is 0.0 through 65535.99998.
        """
        self.scale_p1 = p

    def set_tfold(self, tf):
        """
        Sets the software integration time per profile for all folding and cal modes.
        This is ignored in other modes.
        """
        self.tfold = tf

    # def set_bandwidth(self, bw):
    #     """
    #     Sets the total bandwidth in MHz. This value should match the valon output frequency.
    #     (The sampling rate being twice the valon frequency.)
    #     """
    #     self.bandwidth = bw

    def set_dm(self, dm):
        """
        Sets the dispersion measure for COHERENT_SEARCH mode.
        """
        self.dm = dm

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
        COHERENT_SEARCH, COHERENT_FOLD, COHERENT_CAL, and RAW
        """
        # Only coherent modes. Incoherent modes handled by 'GuppiBackend' class.
        legalmodes = ["COHERENT_SEARCH", "COHERENT_FOLD", "COHERENT_CAL", "RAW"]
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


    def set_nchannels(self, nchan):
        """
        This overrides the config file value nchan -- should not be used.
        """
        self.nchan = nchan

    def set_bandwidth(self, bandwidth):
        """
        Sets the bandwidth in MHz. This value should match the valon output frequency.
        (The sampling rate being twice the valon frequency.)
        """
        if  abs(bandwidth) > 199 and abs(bandwidth) < 2000:
            self.bandwidth = bandwidth
        else:
            raise Exception("Bandwidth of %d MHz is not a legal bandwidth setting" % (bandwidth))

    def set_integration_time(self, int_time):
        """
        Sets the integration time
        """
        self.integration_time = int_time

    def set_only_i(self, only_i):
        """
        Controls whether to 'record only summed polarizations' mode. Zero indicates that
        full stokes data should be recorded. One means to record only summed polarizations.
        This will be set to zero when using the 'FAST4K' observing mode.
        """
        self.only_i = only_i

    def setNodeNumber(self, node_num):
	self.node_number = node_num

    def prepare(self):
        """
        A place to hang the dependency methods.
        """

        self._node_nchan_dep()
        self._acc_len_dep()
        self._node_bandwidth_dep()
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
        self._node_rf_frequency_dep()
        self._dm_dep()
        self._fft_params_dep()

        self._set_status_keys()
        self.set_if_bits()

        if self.cdd_master():
            self.set_registers()
            # program I2C: input filters, noise source, noise or tone
            self.set_if_bits()

    def earliest_start(self):
        now = datetime.utcnow()
        earliest_start = self.round_second_up(
            now + self.mode.needed_arm_delay + timedelta(seconds=2))
        return earliest_start

    def start(self, starttime):
        """
        start(self, starttime = None)

        starttime: a datetime object

        --OR--

        starttime: a tuple or list(for ease of JSON serialization) of
        datetime compatible values: (year, month, day, hour, minute,
        second, microsecond), UTC.

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
        if self.cdd_master():
            self.arm_roach()
        self.scan_running = True
        return (True, "Successfully started roach for starttime=%s" % str(self.start_time))

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
        In CODD mode, acc_len is always 1
        """
        self.acc_len = 1

    def _chan_bw_dep(self):
        """
        Calculates the CHAN_BW status keyword
        Result is bandwidth of each PFM channel in MHz
        """
        self.obsnchan = self.node_nchan

        chan_bw = self.node_bandwidth / float(self.node_nchan)
        self.chan_bw = chan_bw

    def _ds_time_dep(self):
        """
        Calculate the down-sampling time status keyword
        """
        if 'SEARCH' in self.obs_mode.upper():
            dst = self.integration_time * abs(self.node_bandwidth) * 1E6 / self.node_nchan
            power_of_two = 2 ** int(math.log(dst)/math.log(2) + 0.5)
            self.ds_time = power_of_two
        else:
            self.ds_time = 1

    def _ds_freq_dep(self):
        """
        Calculate the DS_FREQ status keyword.
        This is used only when an observer wants to reduce the number of channels
        in software, while using a higher number of hardware channels in SEARCH
        or COHERENT_SEARCH modes.
        """
        if self.obs_mode.upper() in ["SEARCH", "COHERENT_SEARCH"]:
            self.ds_freq = self.nchan / self.node_nchan
        else:
            self.ds_freq = 1

    def _dm_dep(self):
        """
        Read DM from the parfile if COHERENT_FOLD mode, otherwise keep
        the user-set value.
        """
        if self.obs_mode.upper() == "COHERENT_FOLD":
            if self.parfile is not None:
                if self.parfile[0] == '/':
                    full_parfile = self.parfile
                else:
                    full_parfile = '%s/%s' % (self.pardir, self.parfile)
                self.dm = self.dm_from_parfile(full_parfile)

    def _node_nchan_dep(self):
        """
        Calculates the number of channels received by this node.
        This is always the total number of channels divided by
        the number of nodes for coherent modes.
        """
        self.node_nchan = self.nchan/self.num_nodes # number of nodes

    def _pfb_overlap_dep(self):
        """
        Randy/Jason indicated that the new guppi designs will have 12 taps in all modes.
        """
        self.pfb_overlap = 12

    def _pol_type_dep(self):
        """
        Calculates the POL_TYPE status keyword.  This is always AABBCRCI for
        coherent modes.
        """
        self.pol_type = 'AABBCRCI'

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
        self.node_bandwidth = self.bandwidth / self.num_nodes

    def _tbin_dep(self):
        """
        Calculates the TBIN status keyword
        """
        self.tbin = float(self.acc_len * self.node_nchan) / abs(self.node_bandwidth*1E6)

    def _tfold_dep(self):
        if 'COHERENT' == self.obs_mode:
            self.fold_time = 1


    def _packet_format_dep(self):
        """
        Calculates the PKTFMT status keyword
        """
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

    def _node_bandwidth_dep(self):
        """
        Calculates the bandwidth seen by this HPC node
        """
        self.node_bandwidth =  float(self.bandwidth) / float(self.num_nodes)

    def _node_rf_frequency_dep(self):
        """
        The band is divided amoung the various nodes like so:
         ^       ^^       ^^     ctr freq    ^^
         |       ||       ||        ^        ||
         +-------++-------++-----------------++--------- ...
             c0       c1            c2             c3

         So to mark each node's ctr freq c0...cn:

         where:
             rf_frequency is the center band center at the rx
             total_bandwidth is the number of nodes * node_bandwidth of each node
             chan_bw is the calculated number from the node_bandwidth and
             number of node channels
        """
        print "_node_rf_frequency_dep()"
        print "self.rf_frequency", self.rf_frequency
        print "self.bandwidth", self.bandwidth
        print "self.node_number", self.node_number
        print "self.node_bandwidth", self.node_bandwidth
        print "self.chan_bw", self.chan_bw

        self.node_rf_frequency = self.rf_frequency - self.bandwidth/2.0 + \
                                 self.node_number * self.node_bandwidth + \
                                 0.5*self.node_bandwidth - self.chan_bw/2.0

    def _fft_params_dep(self):
        """
        Calculate the OVERLAP, FFTLEN, and BLOCSIZE status keywords
        """
        if True:
            (fftlen, overlap_r, blocsize) = self.fft_size_params(self.rf_frequency,
                                                             self.bandwidth,
                                                             self.nchan,
                                                             self.dm,
                                                             self.max_databuf_size)
            self.fft_len = fftlen
            self.overlap = overlap_r
            self.blocsize = blocsize
        else:
            self.fft_len = 16384
            self.overlap = 0
            self.blocsize = 33554432 # defaults


    def _set_status_keys(self):
        """
        Collect and set the status memory keywords
        """
        statusdata = {}
        statusdata['ACC_LEN'  ] = self.acc_len
        statusdata["BASE_BW"  ] = self.filter_bw
        statusdata['BLOCSIZE' ] = self.blocsize
        statusdata['BANKNUM'  ] = self.node_number
        statusdata["BANKNAM"  ] = self.bank.name if self.bank else 'NOTSET'
        statusdata['CHAN_DM'  ] = self.dm
        statusdata['CHAN_BW'  ] = self.chan_bw
        statusdata["DATAHOST" ] = self.datahost;
        statusdata["DATAPORT" ] = self.dataport;
        statusdata['DATADIR'  ] = self.dataroot
        statusdata['PROJID'   ] = self.projectid
        statusdata['OBSERVER' ] = self.observer
        statusdata['SRC_NAME' ] = self.source
        statusdata['TELESCOP' ] = self.telescope
        statusdata['SCANLEN'  ] = self.scan_length


        statusdata['DS_TIME' ] = self.ds_time
        statusdata['FFTLEN'  ] = self.fft_len
        statusdata['FD_POLN' ] = self.feed_polarization
        statusdata['NPOL'    ] = self.npol
        statusdata['NRCVR'   ] = self.nrcvr
        statusdata['NBIN'    ] = self.nbin
        statusdata['NBITS'   ] = 8

        statusdata['OBSFREQ' ] = self.node_rf_frequency
        statusdata['OBSBW'   ] = self.node_bandwidth
        statusdata['OBSNCHAN'] = self.node_nchan
        statusdata['OBS_MODE'] = self.obs_mode
        statusdata['OFFSET0' ] = '0.0'
        statusdata['OFFSET1' ] = '0.0'
        statusdata['OFFSET2' ] = '0.0'
        statusdata['OFFSET3' ] = '0.0'
        statusdata['ONLY_I'  ] = self.only_i
        statusdata['OVERLAP' ] = self.overlap

        if self.parfile is not None:
            if self.parfile[0] == '/':
                statusdata['PARFILE'] = self.parfile
            else:
                statusdata['PARFILE'] = '%s/%s' % (self.pardir, self.parfile)

        statusdata['PFB_OVER'] = self.pfb_overlap
        statusdata['PKTFMT'  ] = self.packet_format
        statusdata['POL_TYPE'] = self.pol_type

        statusdata['SCALE0'  ] = '1.0'
        statusdata['SCALE1'  ] = '1.0'
        statusdata['SCALE2'  ] = '1.0'
        statusdata['SCALE3'  ] = '1.0'

        statusdata['TBIN'    ] = self.tbin
        statusdata['TFOLD'   ] = self.tfold

        self.set_status(**statusdata)

    def set_registers(self):
        """
        Set the coherent design registers
        """
        if not self.cdd_master():
            return

        regs = {}
        regs['SCALE_P0'] = int(self.scale_p0 * 65536)
        regs['SCALE_P1'] = int(self.scale_p1 * 65536)
        regs['N_CHAN'  ] = int(math.log(self.nchan)/math.log(2))
        #regs['FFT_SHIFT'] = 0xaaaaaaaa (Set by config file)

        self.set_register(**regs)

    # From guppi2_utils.py
    def dm_from_parfile(self,parfile):
        """
        dm_from_parfile(self,parfile):
            Read DM value out of a parfile and return it.
        """
        pf = open(parfile, 'r')
        for line in pf:
            fields = line.split()
            key = fields[0]
            val = fields[1]
            if key == 'DM':
                pf.close()
                return float(val)
        pf.close()
        return 0.0


    # Straight out of guppi2_utils.py massaged to fit in:
    def fft_size_params(self,rf,bw,nchan,dm,max_databuf_mb=128):
        """
        fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):

        Returns a tuple of size parameters (fftlen, overlap, blocsize)
        given the input rf (center of band) in MHz, bw, nchan, DM, and
        optional max databuf size in MB.
        """
        # Overlap needs to be rounded to a integer number of packets
        # This assumes 8-bit 2-pol data (4 bytes per samp) and 8
        # processing nodes.  Also GPU folding requires fftlen-overlap
        # to be a multiple of 64.
        # TODO: figure out best overlap for coherent search mode.  For
        # now, make it a multiple of 512
        pkt_size = 8192
        bytes_per_samp = 4
        node_nchan = nchan / 8
        round_fac = pkt_size / bytes_per_samp / node_nchan

        if (round_fac<512):
            round_fac=512
        rf_ghz = (rf - abs(bw)/2.0)/1.0e3
        print "DEBUG:", rf, bw, rf_ghz
        chan_bw = bw / nchan
        overlap_samp = 8.3 * dm * chan_bw**2 / rf_ghz**3
        overlap_r = round_fac * (int(overlap_samp)/round_fac + 1)
        # Rough FFT length optimization based on GPU testing
        fftlen = 16*1024
        if overlap_r<=1024:
            fftlen=32*1024
        elif overlap_r<=2048:
            fftlen=64*1024
        elif overlap_r<=16*1024:
            fftlen=128*1024
        elif overlap_r<=64*1024:
            fftlen=256*1024
        while fftlen<2*overlap_r:
            fftlen *= 2
        # Calculate blocsize to hold an integer number of FFTs
        # Uses same assumptions as above
        max_npts_per_chan = max_databuf_mb*1024*1024/bytes_per_samp/node_nchan
        nfft = (max_npts_per_chan - overlap_r)/(fftlen - overlap_r)
        npts_per_chan = nfft*(fftlen-overlap_r) + overlap_r
        blocsize = int(npts_per_chan*node_nchan*bytes_per_samp)
        return (fftlen, overlap_r, blocsize)


    def net_config(self, data_ip = None, data_port = None, dest_ip = None, dest_port = None):
        """
        net_config(self, data_i = None, data_port = None, dest_ip = None, dest_port = None)

        This function overrides the base class Backend net_config for a
        CoDD backend. If the CoDD backend is master, it will program the
        roach for output on 8 adapters, as configured in the config
        file.
        """
        # Only the master will have self.roach != None
        if self.roach:
            def tap_data(ips, gigbit_name):
                rvals = []

                for i in range(0, len(ips)):
                    tap = "tap%i" % i
                    gbe = gigbit_name + '%i' % i
                    bank = "BANK" + chr(65 + i)
                    ip = self._ip_string_to_int(ips[bank])
                    mac = self.bank.mac_base + ip
                    port = self.bank.dataport
                    rvals.append((tap, gbe, mac, ip, port))
                return rvals

            gigbit_name = self.mode.gigabit_interface_name
            dest_ip_register_name = self.mode.dest_ip_register_name
            dest_port_register_name = self.mode.dest_port_register_name

            taps = tap_data(self.mode.cdd_roach_ips, gigbit_name)

            for tap in taps:
                self.roach.tap_start(*tap)

            hpcs = self.mode.cdd_hpcs

            for i in range(0, len(hpcs)):
                ip_reg = dest_ip_register_name + '%i' % i
                pt_reg = dest_port_register_name + '%i' % i
                dest_ip = self.mode.cdd_hpc_ip_info[i][0]
                dest_port = self.mode.cdd_hpc_ip_info[i][1]
                self.roach.write_int(ip_reg, dest_ip)
                self.roach.write_int(pt_reg, dest_port)

            # now set up the arp tables:
            regs = [gigbit_name + '%i' % i for i in range(len(self.mode.cdd_roach_ips))]
            set_arp(self.roach, regs, self.hpc_macs)

        return 'ok'
