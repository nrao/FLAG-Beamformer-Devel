
import struct
import ctypes
import binascii 
import player


class Backend:
    """
    A base class which implements some of the common backend calculations (e.g switching).
    """
    def __init__(self, theBank):
        """
        Creates an instance of the vegas internals.
        Backend( bank )
        Where bank is the instance of the player's Bank.
        """
        
        # Save a reference to the bank
        self.bank = theBank
                        
        # Set the switching into a static SIG, NOCAL, no blanking state
        self.nPhases = 1
        self.phase_start = [0.0]
        self.blanking = [0.0]
        self.sig_ref_state = [1]
        self.cal_state = [0]
        self.switch_period = 0.0
                
        self.obs_mode = 'SEARCH' 
        self.max_databuf_size = 128 # in MBytes
        
        self.params = {}
        self.params["switch_period"]  = self.set_switching_period
        self.params["sig_ref_states"] = self.setSigRefStates
        self.params["cal_states"]     = self.setCalStates
        self.params["blanking"]       = self.setBlanking
        self.params["phase_start"]    = self.setPhaseStart
        self.params["num_phases"]     = self.setNumPhases
        self.params["frequency"]      = self.setValonFrequency        
        
                
    # generic set method
    def set_param(self, param, value):
        if param in self.params:
            set_method=self.params[param]
            set_method(value)
            return True
        else:
            print 'No such parameter %s' % param
            print 'Legal parameters in this mode are:'
            for k in self.params.keys():
                print k
        

    ### Methods to set user or mode specified parameters
    ### Not sure how these map for GUPPI
    
    def setSigRefStates(self, srstates):
        self.sig_ref_state = srstates
      
    def setCalStates(self, calstates):
        self.cal_state = calstates
        
    def setPhaseStart(self, phasestarts):
        self.phase_start = phasestarts
        
    def setNumPhases(self, nphases):
        self.nPhases = nphases

    def setBlanking(self, blanking):
        self.blanking = blanking
                          
    def set_switching_period(self, period):
        """
        sets the period in seconds of the requested switching period
        """
        self.switch_period = period
        
    def setValonFrequency(self, vfreq):
        """
        reflects the value of the valon clock, read from the Bank Mode section
        of the config file.
        """
        self.frequency = vfreq
        
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
        
