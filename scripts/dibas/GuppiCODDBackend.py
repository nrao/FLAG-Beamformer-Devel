
import struct
import ctypes
import binascii 
import player
import math
from Backend import Backend
import os

class GuppiCODDBackend(Backend):
    """
    A class which implements some of the GUPPI specific parameter calculations.
    """
    def __init__(self, theBank):
        """
        Creates an instance of the vegas internals.
        GuppiBackend( bank )
        Where bank is the instance of the player's Bank.
        """
        Backend.__init__(self, theBank)       
        # The default switching in the Backend ctor is a static SIG, NOCAL, and no blanking
                 
        self.setObsMode('COHERENT_SEARCH')
        self.max_databuf_size = 128 # in MBytes
        self.scale_p0 = 128
        self.scale_p1 = 128
        self.setBandwidth(100)
        self.dm = 0.0
        self.rf_frequency = 350.0
        self.nchan = 64 # Needs to be a config value?
        self.integration_time = 1 # TBD JJB
        dibas_dir = os.getenv("DIBAS_DIR")
        if dibas_dir is not None:
            self.pardir = dibas_dir + '/etc/config'
        else:
            self.pardir = '/tmp'
        self.parfile = 'example.par' 
        self.datadir = '/lustre/gbtdata/JUNK' # Needs integration with projectid
        
        # register set methods        
        self.params["scale_p0"]        = self.setScaleP0
        self.params["scale_p1"]        = self.setScaleP1
        
        self.params["bandwidth"]      = self.setBandwidth
        self.params["dm"]             = self.setDM
        self.params["rf_frequency"]   = self.setRFfrequency
        self.params["frequency"]      = self.setValonFrequency
        self.params["obs_mode"]       = self.setObsMode
        self.params["par_file"]       = self.setParFile
        
        # Is this fixed for a given mode? config value?
        self.params["num_channels"]   = self.set_nchannels
        

    def setParFile(self, file):
        self.parfile = file
                                
    def setScaleP0(self, p):
        self.scale_p0 = p
        
    def setScaleP1(self, p):
        self.scale_p1 = p
        
    def setBandwidth(self, bw):
        self.bandwidth = bw
        
    def setDM(self, dm):
        self.dm = dm
        
    def setObsMode(self, mode):
        # Only coherent modes. Incoherent modes handled by 'GuppiBackend' class.
        legalmodes = ["COHERENT_SEARCH", "COHERENT_FOLD", "COHERENT_CAL"]
        m = mode.upper()
        if m in legalmodes: 
            self.obs_mode = m
        else:
            Exception("setObsMode: mode must be one of %s" % str(legalmodes))
        
    def setRFfrequency(self, f):
        self.rf_frequency = f
                          
    def set_nchannels(self, nchan):
        """
        This probably comes from config file, via the Bank
        """
        self.nchan = nchan        
        
    def setBandwidth(self, bandwidth):
        legal_bandwidths = [100, 200, 400, 800]
        if  abs(bandwidth) in legal_bandwidths:
            self.bandwidth = bandwidth
        else:
            raise Exception("Bandwidth of %d is not a legal bandwidth setting" % (bandwidth))
        
    def setIntegrationTime(self, int_time):
        """
        Sets the integration time
        """
        self.integration_time = int_time


    def prepare(self):
        """
        A place to hang the dependency methods.
        """

        self.hw_nchan_dep()
        self.acc_len_dep()
        self.chan_bw_dep()
        self.node_bandwidth_dep()
        self.ds_time_dep()
        self.pfb_overlap_dep()
        self.pol_type_dep()
        self.tbin_dep()
        self.only_I_dep()
        self.tfold_dep()
        self.fft_params_dep()
        
        self.set_status_keys()
        self.set_registers()
                
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
        
        chan_bw = self.bandwidth / self.hw_nchan
        if self.bandwidth < 800:
            chan_bw = -1.0 * chan_bw
        self.chan_bw = chan_bw
        
    def ds_time_dep(self):
        """
        Calculate the down-sampling time status keyword
        """
        
        if 'SEARCH' in self.obs_mode:
            dst = self.integration_time * self.bandwidth / self.nchan
            power_of_two = 2 ** int(math.log(dst)/math.log(2))
            self.ds_time = power_of_two
        else:
            self.ds_time = 1
            
    def ds_freq_dep(self):
        """
        Calculate the DS_FREQ status keyword
        """
        self.ds_freq = self.hw_nchan / self.nchan
        
    def hw_nchan_dep(self):
        """
        Can't find direct evidence for this, but seemed logical ...
        """
        if 'COHERENT' in self.obs_mode:
            self.hw_nchan = self.nchan / 8 # number of nodes
        else:
            self.hw_nchan = self.nchan
                
    def pfb_overlap_dep(self):
        """
        Paul's guppi document does not list this parameter, however
        the Guppi manager calculates PFB_OVER which is used in the HPC server.
        Also see fft_params_dep
        """
        if 'COHERENT' in self.obs_mode and self.nchan in [128, 512]:
            self.overlap = 12
        else:
            self.overlap = 4
            
    def pol_type_dep(self):
        """
        Calculates the POL_TYPE status keyword.
        Depends upon a synthetic mode name having FAST4K for that mode, otherwise 
        non-4k coherent mode is assumed. (Is FAST4K  ever coherent?)
        """
       
        if 'FAST4K' in self.bank.current_mode:
            self.pol_type = 'AA+BB'
        else:
            self.pol_type = 'AABBCRCI'
            
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
        self.tbin = self.acc_len * self.hw_nchan / self.bandwidth
        
    def tfold_dep(self):
        if 'COHERENT' == self.obs_mode:
            self.fold_time = 1
            
    
        
    def only_I_dep(self):
        """
        Calculates the ONLY_I and PKTFMT status keywords
        """
        # Not the best way to handle this, but if the mode name has 'FAST4K'
        # in the name, assume FAST4K mode ...
        if 'FAST4K' in self.bank.current_mode:
            self.only_I = 1
            self.packet_format = 'FAST4K'
        else:
            self.only_I = 0
            self.packet_format = '1SFA'

    def set_status_keys(self):
        """
        Collect the status keywords
        """
        statusdata = {}
        
        statusdata['PKTFMT'  ] = self.packet_format
        statusdata['ACC_LEN' ] = self.acc_len
        #node_rf = rf - bw/2.0 - chan_bw/2.0 + (i-1.0+0.5)*node_bw
        
        statusdata['OBSFREQ' ] = self.rf_frequency
        statusdata['OBSBW'   ] = self.node_bandwidth
        statusdata['OBSNCHAN'] = repr(self.hw_nchan)
        statusdata['OBS_MODE'] = self.obs_mode
        statusdata['TBIN'    ] = self.tbin
        statusdata['DATADIR' ] = self.datadir
        statusdata['POL_TYPE'] = self.pol_type
        statusdata['SCALE0'  ] = '1.0'
        statusdata['SCALE1'  ] = '1.0'
        statusdata['SCALE2'  ] = '1.0'
        statusdata['SCALE3'  ] = '1.0'
        statusdata['OFFSET0' ] = '0.0'
        statusdata['OFFSET1' ] = '0.0'
        statusdata['OFFSET2' ] = '0.0'
        statusdata['OFFSET3' ] = '0.0'
        if self.parfile is not None:
            statusdata['PARFILE'] = '%s/%s' % (self.pardir, self.parfile)
            
        statusdata['CHAN_DM' ] = self.dm
        statusdata['FFTLEN'  ] = self.fft_len
        statusdata['OVERLAP' ] = self.overlap
        statusdata['BLOCSIZE'] = self.blocsize
        
        self.bank.set_status(**statusdata)
        
    def set_registers(self):
        regs = {}
        regs['SCALE_P0'] = int(self.scale_p0 * 65536)
        regs['SCALE_P1'] = int(self.scale_p1 * 65536)
        self.bank.set_register(**regs)
                
    def fft_params_dep(self):
        """
        Calculate the PFB_OVERLAP, FFTLEN, and BLOCSIZE status keywords
        """
        if 'COHERENT' in self.obs_mode:
            (fftlen, overlap_r, blocsize) = self.fft_size_params(self.rf_frequency, 
                                                             self.bandwidth, 
                                                             self.nchan, 
                                                             self.dm, 
                                                             self.max_databuf_size)
            self.fft_len = fftlen
            self.pfb_overlap = overlap_r
            self.blocsize = blocsize
        else:
            self.fft_len = 16384
            self.pfb_overlap = 512
            self.blocsize = 33554432 # defaults
                
    # Straight out of guppi2_utils.py massaged to fit in:            
    def fft_size_params(self,rf,bw,nchan,dm,max_databuf_mb=128):
        """
        fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):
            Returns a tuple of size parameters (fftlen, overlap, blocsize)
            given the input rf (center of band), bw, nchan, 
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

       
              
    def net_config(self):
        """
        Configure the network interfaces, IP_ and PT_ registers.
        PLACE HOLDER
        """
        pass               
      
if __name__ == "__main__":
    testCase1()
    
def testCase1():
    g = GuppiCODDBackend(None)
    g.setRFcenterFrequency(350.0)
    g.set_nchannels(64)                                          
