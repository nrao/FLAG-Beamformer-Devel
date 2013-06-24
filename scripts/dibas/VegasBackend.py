
import struct
import ctypes
import binascii 
import player


class VegasBackend:
    """
    A class which implements some of the VEGAS specific parameter calculations.
    """
    def __init__(self, theBank):
        """
        Creates an instance of the vegas internals.
        VegasBackend( bank )
        Where bank is the instance of the player's Bank.
        """
        self.bank = theBank
                
        self.nPhases = 0
        self.phase_start = []
        self.blanking = []
        self.sig_ref_state = []
        self.cal_state = []
        self.switch_period = None
        self.mode_number = None
        self.frequency = None
        self.numStokes = None
        self.frequency_resolution = 0.0
        self.setFilterBandwidth(800)
        self.setPolarization("SELF")
        self.setNumberChannels(1024)
        self.nspectra = 1
        self.mode_number = 1 
        self.setIntegrationTime(10.0)
        self.fpga_clock = None
        self.status_dict = {}
        
        self.frequency = self.bank.mode_data[self.bank.current_mode].frequency
        self.frequency = self.bank.mode_data[self.bank.current_mode].filter_bw
        self.acc_len   = self.bank.mode_data[self.bank.current_mode].acc_len
        
        mode_number = 1
        for i in range(14):
            if "MODE%i" % i in self.bank.current_mode:
                self.mode_number = i
        

    ### Methods to set user or mode specified parameters
    ###        

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
        for i in range(self.nPhases):
            if self.sig_ref_state[i]:
                srline = srline +  srSigSym
            else:
                srline = srline +  srRefSym
            if self.cal_state[i]:
                clline = clline  +  calOnSym
            else:
                clline = clline  +  calOffSym
            if self.blanking[i] > 0.0:
                blline = blline  +  blnkSym % self.blanking[i]
            else:
                blline = blline  +  noBlkSym
                
        print "CAL    :", clline
        print "SIG/REF:", srline 
        print "BLANK  :", blline
        
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
        
    def setPolarization(self, polar):
        """
        setPolarization(x)
        where x is a string 'CROSS', 'SELF1', 'SELF2', or 'SELF'
        """
        self.polarization = polar
        if polar == "CROSS":
            self.numStokes = 4
        elif polar == "SELF1":
            self.numStokes = 1
        elif polar == "SELF2":
            self.numStokes = 1
        elif polar == "SELF":
            self.numStokes = 2
        else:
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
        A place to hang the dependency methods.
        """
        # calculate the fpga_clock and sampler frequency
        self.sampler_frequency_dep()
        self.chan_bw_dep()        
        # calculate the phase_durations and blanking in terms of clocks
        self.phase_duration_dep()
        self.calculate_switching()
        # Now set the appropriate status memory keywords
        self.setBlankingkeys()
        self.setCalStatekeys()
        self.setPhaseStartkeys()
        self.setSigRefStatekeys()
        self.set_state_table_keywords()
                
    # Algorithmic dependency methods, not normally called by a users

    def sampler_frequency_dep(self):
        """
        Computes the effective frequency of the A/D sampler based on mode
        """
        if None in [self.mode_number, self.frequency]:
            raise Exception("mode_number or frequency not set")
        
        if self.mode_number < 13:
            self.sampler_frequency = self.frequency * 2
            self.nsubband = 1
        else:
            self.sampler_frequency = self.frequency / 64
            self.nsubband = 8
        # calculate the fpga frequency
        self.fpga_clock = self.frequency / 8
        
           
    def phase_duration_dep(self):
        """
        Implements a portion of the calculations for switching
        requires: nchan, fpga_clock, phase_start, switch_period and blanking
        produces: clocks_per_granule, clocks_per_accumulation, s_per_accumulation,
                  s_per_granule, requested_phase_start, requested_blanking,
                  actual_blanking, phase_duration, actual_switch_period
        """
    
        if None in [self.nchan, self.fpga_clock, self.phase_start, self.switch_period, self.blanking]:
            raise Exception("one of nchan, fpga_clock, phase_start, switch_period or blanking is not set")
            
        # Calculate some intermediate required values
        self.clocks_per_granule = self.nchan / 8
        self.clocks_per_accumulation = self.acc_len * self.clocks_per_granule
        self.s_per_accumulation = self.clocks_per_accumulation / self.fpga_clock
        self.s_per_granule = self.clocks_per_granule / self.fpga_clock

        # now compute the phases in terms of seconds
        self.requested_phase_start=[]
        self.requested_blanking=[]
        for i in range(0, self.nPhases):
            self.requested_phase_start.append(self.switch_period * self.phase_start[i])
            self.requested_blanking.append(self.blanking[i])
                   
        # now compute the phases in terms of SSG clocks
        self.phase_duration = []
        self.actual_blanking= []
        total_duration = 0
        for i in range(0, self.nPhases):
            if i+1 == self.nPhases:
                p_end = self.switch_period
            else:
                p_end = self.requested_phase_start[i+1]
                
            p_start = self.requested_phase_start[i]
            self.phase_duration.append(int((p_end - p_start)/self.s_per_accumulation + 0.5)*self.acc_len)
            self.actual_blanking.append(int(self.requested_blanking[i]/self.s_per_accumulation + 0.5) * self.acc_len) 
            total_duration += self.phase_duration[i]
            
        # The real switch period based on clock ticks        
        self.actual_switch_period = total_duration
        

    def calculate_switching(self):
        """
        Calculate the switching for the SSG
        """

        if self.nPhases < 1:
            raise Exception("No switching phases have been specified. See add_state() method")
        
        switching_source = "internal"
        
        # now create the state vectors for the switching signal generator (SSG) 
        self.switchbitlist = []      
        if self.nPhases == 1:
            # Set S/R and CAL states to those specified, one phase, no blanking
            swbits = SWbits(self.phase_duration[0],
                            self.sig_ref_state[0], 
                            0, 
                            self.cal_state[0],
                            0)
            self.switchbitlist.append(swbits)
            self.sw_period = self.requested_integration_time
        else:
            # Create a 2 entrys for each switching state to account for blanking
            self.sw_period = self.switch_period
            tot_duration = 0
            for i in range(0, self.nPhases):
                # Blanking on
                swbits = SWbits(self.actual_blanking[i],
                                self.sig_ref_state[i], 
                                0, 
                                self.cal_state[i],
                                1)
                self.switchbitlist.append(swbits)
                tot_duration = tot_duration + self.actual_blanking[i]
                
                # Blanking off, stealing the blanking time from the phase duration
                swbits = SWbits(self.phase_duration[i] - self.actual_blanking[i],
                                self.sig_ref_state[i], 
                                0, 
                                self.cal_state[i],
                                0)
                self.switchbitlist.append(swbits)                 
                tot_duration = tot_duration + self.phase_duration[i]
        # debug code        
        for ssb in self.switchbitlist:
            print "DEBUG ", binascii.hexlify(ssb.get_as_word())


                                
        self.setEcal(self.cal_state)
        self.setIcal(self.cal_state)
        self.setEsigRef1(self.sig_ref_state)
        self.setIsigRef1(self.sig_ref_state)
        # sig ref 2 always zero
        notused=[]
        for i in range(self.nPhases):
            notused.append(0)
        self.setEsigRef2(notused)
        self.setIsigRef2(notused)
                                                                         
                                                                                      
    def setEcal(self, ecals):
        """
        ecals is a list of integers where
        1 indicates the external cal is ON
        0 indicates the external cal is OFF
        """
        for i in range(len(ecals)):
            self.set_status_str('_AECL_%02d' % (i+1), str(ecals[i]))

    def setEsigRef1(self, esr):
        """
        esr is a list of integers where
        1 indicates REF
        0 indicates SIG
        """        
        for i in range(len(esr)):
            self.set_status_str('_AESA_%02d' % (i+1), str(esr[i]))

    def setEsigRef2(self, esr):
        """
        Same as above
        """
        for i in range(len(esr)):
            self.set_status_str('_AESB_%02d' % (i+1), str(esr[i]))

    def setIcal(self, icals):
        """
        Same as Ical above
        """
        for i in range(len(icals)):
            self.set_status_str('_AICL_%02d' % (i+1), str(icals[i]))

    def setIsigRef1(self, isr):
        """
        Same as for IsigRef1 above
        """
        for i in range(len(isr)):
            self.set_status_str('_AISA_%02d' % (i+1), str(isr[i]))

    def setIsigRef2(self, isr):
        """
        Same as for IsigRef2 above
        """
        for i in range(len(isr)):
            self.set_status_str('_AISB_%02d' % (i+1), str(isr[i]))

    def chan_bw_dep(self):
        self.chan_bw = self.sampler_frequency / (self.nchan * 2)
        self.frequency_resolution = abs(self.chan_bw)
        

    def set_status_str(self, x, y):
        """
        """
        self.status_dict[x] = y
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
        statusdata["M_STTMJD" ] = DEFAULT_VALUE;
        statusdata["M_STTOFF" ] = DEFAULT_VALUE;
        statusdata["NBITS"    ] = DEFAULT_VALUE;
        statusdata["NBITSADC" ] = DEFAULT_VALUE;
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
        for x,y in self.status_dict.items():
            statusdata[x] = y
        
        statusdata["BW_MODE"  ] = "HBW" ##??
        statusdata["CHAN_BW"  ] = str(self.chan_bw)
        statusdata["EFSAMPFR" ] = str(self.sampler_frequency)
        statusdata["EXPOSURE" ] = str(self.requested_integration_time)
        statusdata["FPGACLK"  ] = str(self.fpga_clock)
        statusdata["OBSNCHAN" ] = str(self.nchan)
        statusdata["OBS_MODE" ] = "HBW" ##??
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
        statusdata["BANKNAM"  ] = self.bank.bank_name
        statusdata["MODENUM"  ] = str(self.mode_number) # from MODE
        statusdata["NOISESRC" ] = "OFF"  # TBD??
        statusdata["NUMPHASE" ] = str(self.nPhases)
        statusdata["SWPERIOD" ] = str(self.switch_period)
        statusdata["SWMASTER" ] = "VEGAS" # TBD
        statusdata["POLARIZE" ] = self.polarization
        statusdata["CRPIX1"   ] = str(self.nchan/2 + 1)
        statusdata["SWPERINT" ] = str(int(self.requested_integration_time/self.sw_period))
        statusdata["NMSTOKES" ] = str(self.numStokes)
        
        for i in range(8):
            statusdata["_MCR1_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MCDL_%02d" % (i+1)] = str(self.chan_bw)
            statusdata["_MFQR_%02d" % (i+1)] = str(self.frequency_resolution)
            
            

        if self.bank is not None:
            self.bank.set_status(**statusdata)
        else:
            for i in statusdata.keys():
                print "%s = %s" % (i, statusdata[i])
        
            
def testCase1():
    """
    An example test case FWIW.
    """
    b = VegasBackend(None)
    # A few things which should come from the conf file via the bank
    b.bank_name='BankH'
    b.mode_number = 1 ## get this from bank?
    b.acc_len = 256 ## from MODE config

    b.clear_switching_states()                
    b.set_switching_period(0.005)
    b.add_switching_state(0.0,  SWbits.SIG, SWbits.CALON, 0.0)
    b.add_switching_state(0.25, SWbits.SIG, SWbits.CALOFF, 0.0)
    b.add_switching_state(0.5,  SWbits.REF, SWbits.CALON, 0.0)
    b.add_switching_state(0.75, SWbits.REF, SWbits.CALOFF, 0.0)

    b.setValonFrequency(1E9)
    b.setPolarization('SELF')
    b.setNumberChannels(1024) # mode 1
    b.setFilterBandwidth(800E6)
    b.setIntegrationTime(0.005*4)
    
    # call dependency methods and update shared memory
    b.prepare()
        
def testCase2():
    """
    An example test case from configtool setup.
    """
    b = VegasBackend(None)
    # A few things which should come from the conf file via the bank
    b.bank_name='BankH'
    b.mode_number = 1 ## get this from bank?
    b.acc_len = 256 ## from MODE config

    b.clear_switching_states()                
    b.set_switching_period(0.1)
    b.add_switching_state(0.0,  SWbits.SIG, SWbits.CALON, 0.002)
    b.add_switching_state(0.25, SWbits.SIG, SWbits.CALOFF, 0.002)
    b.add_switching_state(0.5,  SWbits.REF, SWbits.CALON, 0.002)
    b.add_switching_state(0.75, SWbits.REF, SWbits.CALOFF, 0.002)

    b.setValonFrequency(1E9)    # config file
    b.setPolarization('SELF')
    b.setNumberChannels(1024)   # mode 1 (config file)
    b.setFilterBandwidth(800E6) # config file?
    b.setIntegrationTime(10.0)
        
    # call dependency methods and update shared memory
    b.prepare()

            
class SWbits:
    """
    A class to hold and encode the bits of a single phase of a switching signal generator phase
    """
    
    SIG=1
    REF=0
    CALON=1
    CALOFF=0
    
    def __init__(self, duration, sr1, sr2, cal, blank):
        self.duration = duration
        self.sig_ref1 = sr1
        self.sig_ref2 = sr2
        self.cal      = cal
        self.blanking = blank
        self.asr      = 0
        self.word = ctypes.create_string_buffer(4)
        
    def get_as_word(self):
        """
        Format into a big-endian number
        """
        
        # Python doesn't have bit fields, so first form the native numeric form
        word = (self.duration & 0x7ffffff) * 0x2**5 + \
               self.asr      * 0x2**4   + \
               self.sig_ref2 * 0x2**3   + \
               self.sig_ref1 * 0x2**2   + \
               self.cal      * 0x2**1   + \
               self.blanking * 0x2**0
               
        # now pack it into a binary buffer in big-endian form
        struct.pack_into('>I', self.word, 0, word)
        return self.word
                                         
         

        
if __name__ == "__main__":
    sw=SWbits(0xA, 1, 0, 1, 0)
    print binascii.hexlify(sw.get_as_word())
    sw=SWbits(0xA, 1, 0, 0, 0)
    print binascii.hexlify(sw.get_as_word())
    sw=SWbits(0xA, 0, 0, 1, 0)
    print binascii.hexlify(sw.get_as_word())
    sw=SWbits(0xA, 0, 0, 0, 0)
    print binascii.hexlify(sw.get_as_word())
    print " "
    testCase1()
    testCase2()
