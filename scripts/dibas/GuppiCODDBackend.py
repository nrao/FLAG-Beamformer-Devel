
import struct
import ctypes
import binascii
import player
import math
import time
from Backend import Backend
import os

class GuppiCODDBackend(Backend):
    """
    A class which implements some of the GUPPI specific parameter calculations.
    This class is specific to the coherent mode BOF designs.
    """
    def __init__(self, theBank, theMode, theRoach, theValon, unit_test = False):
        """
        Creates an instance of the vegas internals.
        GuppiBackend( bank )
        Where bank is the instance of the player's Bank.
        """
        Backend.__init__(self, theBank, theMode, theRoach, theValon, unit_test)
        # This needs to happen on construction so that status monitors can
        # change their data buffer format.
        self.set_status(BACKEND="GUPPI")
        # The default switching in the Backend ctor is a static SIG, NOCAL, and no blanking
            
        self.max_databuf_size = 128 # in MBytes
        self.scale_p0 = 1.0
        self.scale_p1 = 1.0
        self.only_i = 0
        self.set_bandwidth(1500)
        self.dm = 0.0
        self.rf_frequency = 750.0
        self.overlap = 0
        self.tfold = 1.0
        self.nbin = 256
        # Most all receivers are dual polarization
        self.nrcvr = 2
        self.nchan = 8 # Needs to be a config value!!!!!!
        self.num_nodes = 8

        bank_names = {'A' : 0, 'B' : 1, 'C' : 2, 'D' : 3, 'E' : 4, 'F' : 5, 'G' : 6, 'H' : 7 }
        self.node_number = bank_names[self.bank.name[-1]]

        self.integration_time =40.96E-6 # TBD JJB

        if dibas_dir is not None:
            self.pardir = dibas_dir + '/etc/config'
        else:
            self.pardir = '/tmp'
        self.parfile = 'example.par'
        self.datadir = '/lustre/gbtdata/JUNK' # Needs integration with projectid

        # register set methods
        self.params["bandwidth"]      = self.set_bandwidth
        self.params["dm"]             = self.set_dm
        # self.params["frequency"]      = self.set_valon_frequency
        self.params["integration_time"] = self.set_integration_time
        self.params["nbin"]           = self.set_nbin
        self.params["num_channels"]   = self.set_nchannels
        self.params["obs_frequency"]  = self.set_obs_frequency
        # self.params["overlap"]        = self.set_overlap
        self.params["obs_mode"]       = self.set_obs_mode
        self.params["par_file"]       = self.set_par_file
        self.params["scale_p0"]       = self.set_scale_P0
        self.params["scale_p1"]       = self.set_scale_P1
        self.params["tfold"       ]   = self.set_tfold
        self.params["only_i"  ]       = self.set_only_i
        
        # Fill-in defaults if they exist
        if 'OBS_MODE' in self.mode.shmkvpairs.keys():
            self.set_param('obs_mode',   int(self.mode.shmkvpairs['OBS_MODE']))
        if 'ONLY_I' in self.mode.shmkvpairs.keys():
            self.set_param('only_i',   int(self.mode.shmkvpairs['ONLY_I']))
        
        if 'SCALE_P0' in self.mode.roach_kvpairs.keys():
            self.set_param('scale_p0', int(self.mode.roach_kvpairs['SCALE_P0']))
        if 'SCALE_P1' in self.mode.roach_kvpairs.keys():    
            self.set_param('scale_p1', int(self.mode.roach_kvpairs['SCALE_P1']))

    def cdd_master(self):
        """
        Returns 'True' if this is a CoDD backend and it is master. False otherwise.
        """
        return self.bank.name == self.mode.cdd_master_hpc

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

    def set_bandwidth(self, bw):
        """
        Sets the bandwidth in MHz. This value should match the valon output frequency.
        (The sampling rate being twice the valon frequency.)
        """
        self.bandwidth = bw

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
        COHERENT_SEARCH, COHERENT_FOLD, or COHERENT_CAL
        """
        # Only coherent modes. Incoherent modes handled by 'GuppiBackend' class.
        legalmodes = ["COHERENT_SEARCH", "COHERENT_FOLD", "COHERENT_CAL"]
        m = mode.upper()
        if m in legalmodes:
            self.obs_mode = m
        else:
            Exception("set_obs_mode: mode must be one of %s" % str(legalmodes))

    def set_obs_frequency(self, f):
        """
        Sets the center frequency of the observing band.
        """
        self.rf_frequency = f


    def set_nchannels(self, nchan):
        """
        This probably comes from config file, via the Bank
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

    def prepare(self):
        """
        A place to hang the dependency methods.
        """

        self.hw_nchan_dep()
        self.acc_len_dep()
        self.node_bandwidth_dep()
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
        self.node_rf_frequency_dep()
        self.fft_params_dep()

        self.set_status_keys()
        self.set_registers()

    def start(self):
        """
        An coherent mode start routine.
        """
        if self.hpc_process is None:
            self.start_hpc()
            time.sleep(5)
        self.hpc_cmd("start")
        time.sleep(3)
        self.arm_roach()
        scan_running = True

        while scan_running:
            time.sleep(3)
            if self.hpc_process is None:
                scan_running = False
                Exception("HPC Process was stopped or failed");
            if self.get_status('DISKSTAT') == "exiting":
                scan_running = False
            elif self.bank.get_status('NETSTAT') == "exiting":
                scan_running = False
            elif self.check_keypress('q') == True:
                print 'User terminated scan'
                self.hpc_cmd('stop')




    # Algorithmic dependency methods, not normally called by users

    def acc_len_dep(self):
        """
        In CODD mode, acc_len is always 1
        """
        self.acc_len = 1

    def chan_bw_dep(self):
        """
        Calculates the CHAN_BW status keyword
        Result is bandwidth of each PFM channel in MHz
        """
        self.obsnchan = self.hw_nchan

        chan_bw = self.node_bandwidth / float(self.hw_nchan)
        self.chan_bw = chan_bw

    def ds_time_dep(self):
        """
        Calculate the down-sampling time status keyword
        """
        if 'SEARCH' in self.obs_mode.upper():
            dst = self.integration_time * self.node_bandwidth * 1E6 / self.nchan
            power_of_two = 2 ** int(math.log(dst)/math.log(2) + 0.5)
            self.ds_time = power_of_two
        else:
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
        if 'COHERENT' in self.obs_mode.upper():
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
            self.node_bandwidth = self.bandwidth / self.num_nodes
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

    def node_bandwidth_dep(self):
        """
        Calculates the bandwidth seen by this HPC node
        """
        self.node_bandwidth =  self.bandwidth/self.num_nodes

    def node_rf_frequency_dep(self):
        """
        The band is divided amoung the various nodes like so:
         ^       ^^       ^^     ctr freq    ^^
         |       ||       ||        ^        ||
         +-------++-------++-----------------++--------- ...
             c0       c1            c2             c3

         So to mark each node's ctr freq c0...cn:

         where:
             rf_frequency is the center band center at the rx
             total_bandwidth is the number of nodes * bandwidth of each node
             chan_bw is the calculated number from the node_bandwidth and
             number of node channels
        """
        i = self.node_number
        self.node_rf_frequency = self.rf_frequency - self.bandwidth/2.0 - \
                  self.chan_bw/2.0 + (i-1.0+0.5)*self.node_bandwidth

    def fft_params_dep(self):
        """
        Calculate the OVERLAP, FFTLEN, and BLOCSIZE status keywords
        """
        if 'COHERENT' in self.obs_mode:
            (fftlen, overlap_r, blocsize) = self.fft_size_params(self.node_rf_frequency,
                                                             self.node_bandwidth,
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


    def set_status_keys(self):
        """
        Collect and set the status memory keywords
        """
        statusdata = {}
        statusdata['ACC_LEN' ] = self.acc_len
        statusdata['BLOCSIZE'] = self.blocsize
        statusdata['BANKNUM' ] = self.node_number
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

        statusdata['OBSFREQ' ] = self.node_rf_frequency
        statusdata['OBSBW'   ] = self.node_bandwidth
        statusdata['OBSNCHAN'] = repr(self.hw_nchan)
        statusdata['OBS_MODE'] = self.obs_mode
        statusdata['OFFSET0' ] = '0.0'
        statusdata['OFFSET1' ] = '0.0'
        statusdata['OFFSET2' ] = '0.0'
        statusdata['OFFSET3' ] = '0.0'
        statusdata['ONLY_I'  ] = self.only_i
        statusdata['OVERLAP' ] = self.overlap

        if self.parfile is not None:
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
        if self.valon:
            self.valon.set_frequency(0, self.bandwidth)
        regs = {}
        regs['SCALE_P0'] = int(self.scale_p0 * 65536)
        regs['SCALE_P1'] = int(self.scale_p1 * 65536)
        regs['N_CHAN'   ] = int(math.log(self.nchan*self.num_nodes)/math.log(2))
        #regs['FFT_SHIFT'] = 0xaaaaaaaa (Set by config file)

        self.set_register(**regs)


    # Straight out of guppi2_utils.py massaged to fit in:
    def fft_size_params(self,rf,bw,nchan,dm,max_databuf_mb=128):
        """
        fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):
            Returns a tuple of size parameters (fftlen, overlap, blocsize)
            given the input rf (center of band) in MHz, bw, nchan,
            DM, and optional max databuf size in MB.
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
                    # gbe = "tGX1_tGv2%i" % i
                    ip = self._ip_string_to_int(ips[i])
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
                dest_ip = self.banks[hpcs[i]].dest_ip
                dest_port = self.banks[hpcs[i]].dest_port
                self.roach.write_int(ip_reg, dest_ip)
                self.roach.write_int(pt_reg, dest_port)

        return 'ok'
