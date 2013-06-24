
import struct
import ctypes
import binascii 
import player


class GuppiBackend:
    """
    A class which implements some of the GUPPI specific parameter calculations.
    """
    def __init__(self, theBank):
        """
        Creates an instance of the vegas internals.
        GuppiBackend( bank )
        Where bank is the instance of the player's Bank.
        """
        self.nPhases = 0
        self.phase_start = []
        self.blanking = []
        self.sig_ref_state = []
        self.cal_state = []
        self.switch_period = None
        self.bank = theBank
        
        self.bof_mode = 'SEARCH' 
        self.max_databuf_size = 128 # in MBytes


    ### Methods to set user or mode specified parameters
    ### Not sure how these map for GUPPI

    def set_switching_period(self, period):
        """
        sets the period in seconds of the requested switching period
        """
        self.switch_period = period
        
    def clear_switching_states(self):
        """
        resets/delets the switching_states
        """
        self.nPhases = 0
        self.phase_start = []
        self.blanking = []
        self.cal_state = []
        self.sig_ref_state = []
        
    def add_switching_state(self, start, sig_ref, cal, blank=0.0):
        """
        add_state(start, sig_ref, cal, blank=0.0):
        Add a description of one switching phase.
        Where:
            phase_start is the fraction of the switching period where this phase should begin in the range (0..1)
            sig_ref is 1 for SIG,    0 for REF
            cal     is 1 for CAL ON, 0 for CAL OFF
            blank   is the requested blanking at the beginning of the phase in seconds
        """
        if start in self.phase_start:
            raise Exception("switching phase start of %f already specified" % (start))
            
        self.nPhases = self.nPhases+1
        self.phase_start.append(start)
        self.blanking.append(blank)
        self.cal_state.append(cal)
        self.sig_ref_state.append(sig_ref)
        
    def setBlankingkeys(self):
        """
        blank should be a list of blanking interval values in seconds
        """
        for i in range(len(self.blanking)):
            self.set_status_str('_SBLK_%02d' % (i+1), str(self.blanking[i]))
    
    def setCalStatekeys(self):
        """
        calstate should be a list of integers with 
        1 indicating cal ON 
        0 indicating cal OFF
        """
        for i in range(len(self.cal_state)):
            self.set_status_str('_SCAL_%02d' % (i+1), str(self.cal_state[i]))
        
    def setPhaseStartkeys(self):
        """
        phstart is a list of switch period fractions in the range 0..1
        """
        for i in range(len(self.phase_start)):
            self.set_status_str('_SPHS_%02d' % (i+1), str(self.phase_start[i]))

    def setSigRefStatekeys(self):
        """
        srstate is a list of integers where
        1 indicates REF
        0 indicates SIG
        """
        for i in range(len(self.sig_ref_state)):
            self.set_status_str('_SSRF_%02d' % (i+1), str(self.sig_ref_state[i]))
        
    def setValonFrequency(self, vfreq):
        """
        reflects the value of the valon clock, read from the Bank Mode section
        of the config file.
        """
        self.frequency = vfreq
        
    def set_nchannels(self, nchan):
        """
        This probably comes from config file, via the Bank
        """
        self.nchan = nchan
        
    def setRFcenterFrequency(self, rf):
        """
        Sets the center frequency of the observing band
        """ 
        self.rf_frequency = rf
        
        
    def setBandwidth(self, bandwidth):
        legal_bandwidths = [100, 200, 400, 800]
        if  bandwidth in legal_bandwidths or \
           -bandwith in legal_bandwidths:
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
        self.ds_time_dep()
        self.pfb_overlap_dep()
        self.pol_type_dep()
        self.tbin_dep()
        self.only_I_dep()
        self.tfold_dep()
               

    # Inputs set directly by config file:
    #    nchan, bof_mode, ds_freq, chan_dm,
    
    # Inputs set/tweaked by user:
    #    bandwidth, obs_mode, integration_time
    
                
    # Algorithmic dependency methods, not normally called by users
    
    def acc_len_dep(self):
        """
        Calculates the ACC_LEN status keyword
        (as opposed to the similarly named ACC_LENGTH in the old guppi bofs)
        """
        if 'COHERENT' in self.obs_mode:
            self.acc_len = 1
        else:
            self.acc_len = int(self.integration_time * self.bandwidth / self.hw_nchan - 1 + 0.5) + 1
            
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
        
        if 'SEARCH' in self.bof_mode:
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
        if 'COHERENT' in self.bof_mode:
            self.hw_nchan = self.nchan / 8 # number of nodes
        else:
            self.hw_chan = self.nchan
                
    def pfb_overlap_dep(self):
        """
        Paul's guppi document does not list this parameter, however
        the Guppi manager calculates PFB_OVER which is used in the HPC server.
        Also see fft_params_dep
        """
        if 'COHERENT' in self.bof_mode and self.nchan in [128, 512]:
            self.overlap = 12
        else:
            self.overlap = 4
            
    def pol_type_dep(self):
        """
        Calculates the POL_TYPE status keyword.
        Depends upon a synthetic parameter 'bof_mode' TBD
        """
        
        if 'COHERENT' in self.bof_mode:
            self.pol_type = 'AABBCRCI'
        elif 'FAST4K' in self.bof_mode:
            self.pol_type = 'AA+BB'
        else:
            self.pol_type = 'IQUV'
            
    def node_bandwidth_dep(self):
        """
        Calculations the bandwidth seen by this HPC node
        """
        if 'COHERENT' in self.bof_mode:
            self.node_bandwidth = self.bandwidth / 8
        else:
            self.node_bandwidth = self.bandwidth
            
    def tbin_dep(self):
        """ 
        Calculates the TBIN status keyword
        """
        self.tbin = self.acc_len * self.hw_nchan / self.bandwidth
        
    def tfold_dep(self):
        if 'COHERENT' == self.bof_mode:
            self.fold_time = 1
            
    
        
    def only_I_dep(self):
        """
        Calculates the ONLY_I and PKTFMT status keywords
        """
        if 'FAST4K' in self.bof_mode:
            self.only_I = 1
            self.packet_format = 'FAST4K'
        else:
            self.only_I = 0
            self.packet_format = '1SFA'

    def set_status_keys(self):
        """
        Collect the status keywords
        """
        
        statuskeys['PKTFMT'  ] = self.packet_format
        statuskeys['ACC_LEN' ] = self.acc_len
        node_rf = rf - bw/2.0 - chan_bw/2.0 + (i-1.0+0.5)*node_bw
        statuskeys['OBSFREQ' ] = self.input_rf_frequency
        statuskeys['OBSBW'   ] = self.node_bandwidth
        statuskeys['OBSNCHAN'] = repr(node_nchan)
        statuskeys['TBIN'    ] = repr(abs(1e-6*nchan/bw))
        statuskeys['DATADIR' ] = '/data/gpu/partial/' + node
        statuskeys['POL_TYPE'] = 'AABBCRCI'
        statuskeys['SCALE0'  ] = '1.0'
        statuskeys['SCALE1'  ] = '1.0'
        statuskeys['SCALE2'  ] = '1.0'
        statuskeys['SCALE3'  ] = '1.0'
        statuskeys['OFFSET0' ] = '0.0'
        statuskeys['OFFSET1' ] = '0.0'
        statuskeys['OFFSET2' ] = '0.0'
        statuskeys['OFFSET3' ] = '0.0'
        if parfile=='':
            statuskeys[pfx+'PARFILE'] = ''
        else:
            statuskeys[pfx+'PARFILE'] = '%s/%s' % (pardir, parfile)
        statuskeys[pfx+'CHAN_DM'] = repr(dm)
        statuskeys[pfx+'FFTLEN'] = repr(fftlen)
        statuskeys[pfx+'OVERLAP'] = repr(overlap)
        statuskeys[pfx+'BLOCSIZE'] = repr(blocsize)

                
    def fft_params_dep(self):
        """
        Calculate the PFB_OVERLAP, FFTLEN, and BLOCSIZE status keywords
        """
        if 'COHERENT' in self.bof_mode:
            (fftlen, overlap_r, blocsize) = self.fft_size_params(self.input_rf_frequency, 
                                                             self.bandwidth, 
                                                             self.nchan, 
                                                             self.dm, 
                                                             self.max_databuf_size)
            self.fft_len = fftlen
            self.pfb_overlap = overlap_r
            self.blocsize = blocsize
        else:
            self.fft_len = 16384
            self.pfb_over = 512
            self.blocsize = 33554432 # defaults
                
    # Straight out of guppi2_utils.py massaged to fit in:            
    def fft_size_params(rf,bw,nchan,dm,max_databuf_mb=128):
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
        
      
if __name__ == "__main__":
    testCase1()
    
def testCase1():
    g = GuppiBackend(None)
    g.setRFcenterFrequency(350.0)
    g.set_nchannels(64)                                          
