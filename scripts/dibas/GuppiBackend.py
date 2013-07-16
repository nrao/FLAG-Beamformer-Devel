
import struct
import ctypes
import binascii
import player
import math
import time
from Backend import Backend
import os

class GuppiBackend(Backend):
    """
    A class which implements some of the GUPPI specific parameter calculations.
    This class is specific to the Incoherent BOF designs.
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
        self.obs_mode = 'SEARCH'
        self.max_databuf_size = 128 # in MBytes [Not sure where this ties in. Value from the manager]
        self.nchan = self.mode.nchan
        self.integration_time = 40.96E-6
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
        self.set_bandwidth(800.0)
        self.chan_dm = 0.0
        self.rf_frequency = 2000.0
        self.nbin = 256
        self.tfold = 1.0
        self.dm = 0.0
        # Almost all receivers are dual polarization
        self.nrcvr = 2


        if self.dibas_dir is not None:
           self.pardir = self.dibas_dir + '/etc/config'
        else:
            self.pardir = '/tmp'
        self.parfile = 'example.par'

        self.params["bandwidth"]      = self.set_bandwidth
        self.params["integration_time"] = self.set_integration_time
        self.params["nbin"]           = self.set_nbin
        self.params["obs_frequency"]  = self.set_obs_frequency
        self.params["obs_mode"]       = self.set_obs_mode
        #self.params["overlap"]        = self.set_overlap
        self.params["only_i"      ]   = self.set_only_i
        self.params["offset_i"    ]   = self.set_offset_I
        self.params["offset_q"    ]   = self.set_offset_Q
        self.params["offset_u"    ]   = self.set_offset_U
        self.params["offset_v"    ]   = self.set_offset_V
        self.params["scale_i"     ]   = self.set_scale_I
        self.params["scale_q"     ]   = self.set_scale_Q
        self.params["scale_u"     ]   = self.set_scale_U
        self.params["scale_v"     ]   = self.set_scale_V
        self.params["tfold"       ]   = self.set_tfold
        self.fft_params_dep()
        
    ### Methods to set user or mode specified parameters
    ### Not sure how these map for GUPPI

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

        self.hw_nchan_dep()
        self.acc_len_dep()
        self.chan_bw_dep()
        self.ds_time_dep()
        self.ds_freq_dep()
        self.pfb_overlap_dep()
        self.pol_type_dep()
        self.tbin_dep()
        self.only_I_dep()
        self.packet_format_dep()
        self.npol_dep()
        self.tfold_dep()
        self.node_bandwidth_dep()

        self.set_registers()
        self.set_status_keys()


    def start(self):
        """
        An incoherent mode start routine.
        """
        if self.hpc_process is None:
            self.start_hpc()
            time.sleep(5)
        self.hpc_cmd("start")
        time.sleep(3)
        self.arm_roach()
        
        self.scan_running = True
        while self.scan_running:
            time.sleep(3)
            if self.hpc_process is None:
                self.scan_running = False
                Exception("HPC Process was stopped or failed");
            if self.get_status('DISKSTAT') == "exiting":
                print "HPC Disk thread did not appear to start -- ending scan"
                self.scan_running = False
            elif self.get_status('NETSTAT') == "exiting":
                print "HPC Net thread did not appear to start -- ending scan"
                self.scan_running = False
            elif self.check_keypress('q') == True:
                print 'User terminated scan'
                self.hpc_cmd('stop')
                self.scan_running = False
        print "Scan Completed"


    # Algorithmic dependency methods, not normally called by users

    def acc_len_dep(self):
        """
        Calculates the hardware accumulation length.
        The register values must be in the range of 0 to 65535, in even powers of two, minus one.
        """
        acc_length = 2**int(math.log(int(self.integration_time * self.bandwidth * 1E6/self.hw_nchan + 0.5))/math.log(2))-1

        if acc_length < 0 or acc_length > 65535:
            raise Exception("Hardware accumulation length too long. Reduce integration time or bandwidth.")
        else:
            self.acc_length = acc_length
            self.acc_len = self.acc_length+1

    def chan_bw_dep(self):
        """
        Calculates the CHAN_BW status keyword
        Result is bandwidth of each channel in MHz
        """
        self.obsnchan = self.hw_nchan

        chan_bw = self.bandwidth / float(self.hw_nchan)
        #if self.bandwidth < 800:
        #    chan_bw = -1.0 * chan_bw
        self.chan_bw = chan_bw

    def ds_time_dep(self):
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

    def ds_freq_dep(self):
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

    def hw_nchan_dep(self):
        """
        Can't find direct evidence for this, but seemed logical ...
        """
        if 'COHERENT' in self.obs_mode:
            self.hw_nchan = self.nchan # number of nodes
        else:
            self.hw_nchan = self.nchan
        self.node_nchan = self.hw_nchan

    def pfb_overlap_dep(self):
        """
        Randy/Jason indicated that the new guppi designs will have 12 taps in all modes.
        """
        self.pfb_overlap = 12

    def pol_type_dep(self):
        """
        Calculates the POL_TYPE status keyword.
        Depends upon a synthetic mode name having FAST4K for that mode, otherwise
        non-4k coherent mode is assumed.
        """
        if 'COHERENT' in self.obs_mode:
            self.pol_type = 'AABBCRCI'
        elif 'FAST4K' in self.mode.mode.upper():
            self.pol_type = 'AA+BB'
        else:
            self.pol_type = 'IQUV'

    def npol_dep(self):
        """
        Calculates the number of polarizations to be recorded.
        Most cases it is all four, except in FAST4K, or when the user
        has indicated they only want 1 stokes product)
        """
        self.npol = 4
        if 'FAST4K' in self.mode.mode.upper():
            self.npol   = 1
        elif self.only_i:
            self.npol = 1

    def node_bandwidth_dep(self):
        """
        Calculations the bandwidth seen by this HPC node
        """
        if 'COHERENT' in self.obs_mode:
            self.node_bandwidth = self.bandwidth / 8
        else:
            self.node_bandwidth = self.bandwidth

    def tbin_dep(self):
        """
        Calculates the TBIN status keyword
        """
        self.tbin = float(self.acc_len * self.hw_nchan) / (self.bandwidth*1E6)

    def tfold_dep(self):
        if 'COHERENT' == self.obs_mode:
            self.fold_time = 1



    def packet_format_dep(self):
        """
        Calculates the PKTFMT status keyword
        """
        if 'FAST4K' in self.mode.mode.upper():
            self.packet_format = 'FAST4K'
        else:
            self.packet_format = '1SFA'


    def only_I_dep(self):
        """
        Calculates the ONLY_I status keyword
        """
        # Note this requires that the config mode name contains 'FAST4K' in the name
        if 'FAST4K' in self.mode.mode.upper():
            self.only_i = 0
        elif self.obs_mode.upper() not in ["SEARCH", "COHERENT_SEARCH"]:
            self.only_i = 0

    def set_status_keys(self):
        """
        Collect the status keywords
        """
        statusdata = {}
        statusdata['ACC_LEN' ] = self.acc_len
        statusdata['BLOCSIZE'] = self.blocsize
        statusdata['CHAN_DM' ] = self.dm
        statusdata['CHAN_BW' ] = self.chan_bw
        statusdata['DATADIR' ] = self.dataroot
        statusdata['PROJID'  ] = self.projectid
        statusdata['DS_TIME' ] = self.ds_time

        statusdata['FFTLEN'  ] = self.fft_len

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
            statusdata['PARFILE'] = '%s/%s' % (self.pardir, self.parfile)
        statusdata['PKTFMT'  ] = self.packet_format

        statusdata['SCALE0'  ] = '1.0'
        statusdata['SCALE1'  ] = '1.0'
        statusdata['SCALE2'  ] = '1.0'
        statusdata['SCALE3'  ] = '1.0'
        statusdata['TBIN'    ] = self.tbin
        statusdata['TFOLD'   ] = self.tfold

        self.set_status(**statusdata)

    def set_registers(self):
        regs = {}

        if not self.test_mode:
            self.valon.set_frequency(0, self.bandwidth)
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

    def fft_params_dep(self):
        """
        Calculate the FFTLEN, and BLOCSIZE status keywords
        """
        self.fft_len = 16384
        self.blocsize = 33554432 # defaults


