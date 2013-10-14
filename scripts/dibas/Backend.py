import ctypes
import binascii

try:
    from vegas_hpc.vegas_utils import vegas_status
except ImportError:
    from vegas_utils import vegas_status

import os
import subprocess
import time
from datetime import datetime, timedelta
from i2c import I2C
from set_arp import set_arp

######################################################################
# Some constants
######################################################################

class NoiseTone:
    NOISE=0
    TONE=1

class NoiseSource:
    OFF=0
    ON=1

class SWbits:
    """
    A class to hold and encode the bits of a single phase of a switching signal generator phase
    """
    SIG=0
    REF=1
    CALON=1
    CALOFF=0

def convertToMHz(f):
    """
    Sometimes values are expressed in Hz instead of MHz.
    This routine assumes anything over 5000 to be in Hz.
    """
    f = abs(f)
    if f > 5000:
        return f/1E6 # Convert to MHz
    else:
        return f     # already in MHz

######################################################################
# class Backend
#
# A generic backend class, which will provide common backend
# functionality, and be the basis for specialized backends.
#
# All backends have something in common: they all access shared
# memory. They all manage a DAQ type process that actually does the data
# collection and FITS file writing.
#
# Some backends: GuppiBackend, GuppiCODDBackend, VegasBackend
#
# @param theBank: Bank configuration data
# @param theMode: Mode configuration data
# @param theRoach: katcp_wrapper, a KATCP client
# @param theValon: A module that controls the valon via KATCP
# @param unit_test: Set to true to unit test. When unit testing, won't
# use any hardware, or communicate to any other process.
#
######################################################################

class Backend:
    """
    A base class which implements some of the common backend calculations (e.g switching).

    Backend(theBank, theMode, theRoach, theValon, unit_test)
    Where:

    * *theBank:* the BankData object for this mode, containing all the data required by this backend.
    * *theMode:* The ModeData object for this mode, containing all the data required for this mode.
    * *theRoach:* (optional) This is a *katcp_wrapper* object that allows the bank to communicate with the ROACH.
    * *theValon:* (optional) The Valon synth object.
    * *unit_test:* (optional) True if the class was created for unit testing  purposes.
    """
    def __init__(self, theBank, theMode, theRoach = None,
                 theValon = None, hpc_macs = None, unit_test = False):
        """
        Creates an instance of the vegas internals.
        """

        # Save a reference to the bank
        self.test_mode = unit_test
        if self.test_mode:
            print "UNIT TEST MODE!!!"
            self.roach = None
            self.valon = None
            self.i2c = None
            self.mock_status_mem = {}
            self.mock_roach_registers = {}
        else:
            self.roach = theRoach
            self.valon = theValon
            self.i2c = I2C(theRoach)
            self.status = vegas_status()

        self.hpc_macs = hpc_macs
        # Bits used to set I2C for proper filter
        self.filter_bw_bits = {450: 0x00, 1450: 0x08, 1900: 0x18}

        # This is already checked by player.py, we won't get here if not set
        self.dibas_dir = os.getenv("DIBAS_DIR")

        self.dataroot  = os.getenv("DIBAS_DATA") # Example /lustre/gbtdata
        if self.dataroot is None:
            self.dataroot = "/tmp"
        self.mode = theMode
        self.bank = theBank
        self.start_time = None
        self.start_scan = False
        self.hpc_process = None
        self.obs_mode = 'SEARCH'
        self.max_databuf_size = 128 # in MBytes
        self.observer = "unspecified"
        self.source = "unspecified"
        self.telescope = "unspecified"
        self.projectid = "JUNK"
        self.datadir = self.dataroot + "/" + self.projectid
        self.scan_running = False
        self.monitor_mode = False
        print "Backend(): Setting self.frequency to", self.mode.frequency
        self.frequency = self.mode.frequency
        self.setFilterBandwidth(self.mode.filter_bw)
        self.setNoiseSource(NoiseSource.OFF)
        self.setNoiseTone1(NoiseTone.NOISE)
        self.setNoiseTone2(NoiseTone.NOISE)
        self.setScanLength(30.0)

        self.params = {}
        # TBF: Rething "frequency" & "bandwidth" parameters. If one or
#        both are parameters, then changing them must deprogram roach
#        first, set frequency, then reprogram roach. For now, these
#        are removed from the parameter list here and in derived
#        classes, and frequency is set when programming
#        roach. Frequency changes may be made via the Bank.set_mode()
#        function.  self.params["frequency" ] = self.setValonFrequency
        self.params["filter_bw"    ] = self.setFilterBandwidth
        self.params["observer"     ] = self.setObserver
        self.params["project_id"   ] = self.setProjectId
        self.params["noise_tone_1" ] = self.setNoiseTone1
        self.params["noise_tone_2" ] = self.setNoiseTone2
        self.params["noise_source" ] = self.setNoiseSource
        self.params["scan_length"  ] = self.setScanLength
        self.params["source"       ] = self.setSource
        self.params["telescope"    ] = self.setTelescope

        # CODD mode is special: One roach has up to 8 interfaces, and
        # each is the DATAHOST for one of the Players. This distribution
        # gets set here. If not a CODD mode, just use the one in the
        # BANK section.
        if self.mode.cdd_mode:
            self.datahost = self.mode.cdd_roach_ips[self.bank.name]
        else:
            self.datahost = self.bank.datahost

        self.dataport = self.bank.dataport
        self.bof_file = self.mode.bof

        self.progdev()
        self.net_config()

        if self.mode.roach_kvpairs:
            self.set_register(**self.mode.roach_kvpairs)
        self.reset_roach()


    def __del__(self):
        """
        Perform cleanup activities for a Bank object.
        """
        # Stop the HPC program if it is running
        if self.test_mode:
            return

        if self.hpc_process is not None:
            print "Stopping HPC program!"
            self.stop_hpc()

    def cleanup(self):
        """
        This explicitly cleans up any child processes. This will be called
        by the player before deleting the backend object.
        """
        # Note: If redefined, in a derived class, be careful to include the code below.
        self.stop_hpc()


    def getI2CValue(self, addr, nbytes):
        """
        getI2CValue(addr, nbytes, data):

        * *addr:* The I2C address
        * *nbytes:* the number of bytes to get

        Returns the IF bits used to set the input filter.

        Example::

          bits = self.getI2CValue(0x38, 1)
        """

        if self.i2c:
            return self.i2c.getI2CValue(addr, nbytes)
        return (True, 0)



    def setI2CValue(self, addr, nbytes, data):
        """
        setI2CValue(addr, nbytes, data):

        * *addr:* the I2C address
        * *nbytes:* the number of bytes to send
        * *data:* the data to send

        Sets the IF bits used to set the input filter.

        Example::

          self.setI2CValue(0x38, 1, 0x25)
        """

        if self.i2c:
            return self.i2c.setI2CValue(addr, nbytes, data)
        return True


    def hpc_cmd(self, cmd):
        """
        Opens the named pipe to the HPC program, sends 'cmd', and closes
        the pipe. This takes care to not block on the fifo.
        """

        if self.test_mode:
            return True

        if self.hpc_process is None:
            raise Exception( "HPC program has not been started" )

        fh=self.hpc_process.stdin.fileno()
        os.write(fh, cmd + '\n')
        return True

    def start_hpc(self):
        """
        start_hpc()

        Starts the HPC program running. Stops any previously running instance.
        """

        if self.test_mode:
            return

        self.stop_hpc()
        hpc_program = self.mode.hpc_program
        if hpc_program is None:
            raise Exception("Configuration error: no field hpc_program specified in "
                            "MODE section of %s " % (self.current_mode))

        process_list = [self.dibas_dir + '/exec/x86_64-linux/' + hpc_program]

        if self.mode.hpc_program_flags:
            process_list.append(self.mode.hpc_program_flags)

        self.hpc_process = subprocess.Popen(process_list, stdin=subprocess.PIPE)


    def stop_hpc(self):
        """
        stop_hpc()

        Stop the hpc program and make it exit.
        To stop an observation use 'stop()' instead.
        """

        print "stop_hpc()"

        if self.test_mode:
            return

        if self.hpc_process is None:
            print "stop_hpc(): self.hpc_process is None."
            return False # Nothing to do

        try:
            # First ask nicely
            # Kill and reclaim child
            print "stop_hpc(): sending 'quit'"
            self.hpc_process.communicate("quit\n")
            time.sleep(1)
            # Kill if necessary
            if self.hpc_process.poll() == None:
                # still running, try once more
                print "stop_hpc(): sending SIGTERM"
                self.hpc_process.terminate()
                time.sleep(1)

                if self.hpc_process.poll() is not None:
                    killed = True
                else:
                    print "stop_hpc(): sending SIGKILL"
                    self.hpc_process.kill()
                    killed = True;
            else:
                killed = False
        except OSError, e:
            print "While killing child process:", e
            killed = False
        finally:
            del self.hpc_process
            self.hpc_process = None

        return killed

    # generic set method
    def set_param(self, param, value):
        """
        set_param(self, param, value)

        The DIBAS backends are directly controlled by setting registers
        in the FPGA, setting key/value pairs in status memory, setting
        the Valon and I2C controllers, etc. Low level interfaces to do
        all these things exist. However, there are often time and value
        dependencies associated between these values. *set_param()*
        provides a way to set values at a higher level of abstraction so
        that when the low-level values are set, the dependencies are
        computed and the values are set in the proper order. This is
        therefore a high-level method of setting instrument parameters.

        Once all parameters are set a call to *prepare()* will cause the
        dependencies to be computed and the values sent to their
        respective destinations.

        If a parameter is given that does not exist, the function will
        throw an exception that includes a list of the available
        parameters (see *help_param()* as well).

        This function is normally called by the
        *Bank.set_param(**kvpairs)* member.

        * *param*: A keyword (string) representing a parameter.
        * *value*: The value associated with the parameter

        Example::

          self.set_param('exposure', 0.05)
        """
        if param in self.params:
            set_method=self.params[param]
            retval = set_method(value)

            if not retval:  # for those who don't return a value
                return True
            else:
                return retval
        else:
            msg = "No such parameter '%s'. Legal parameters in this mode are: %s" \
                % (param, str(self.params.keys()))
            print 'No such parameter %s' % param
            print 'Legal parameters in this mode are:'
            for k in self.params.keys():
                print k
            raise Exception(msg)

    def get_param(self,param):
        """
        get_param(self, param)

        Return the value of a parameter, if available, or None if the
        parameter does not exist.

        * *param:* The parameter, a string. If not provided, or if set
          to *None*, *help_param()* will return a dictionary of all
          parameters and their doc strings.
        """
        if param in self.param_values:
            return self.param_values[param]
        else:
            return None

    # generic help method
    def help_param(self, param = None):
        """
        help_param(self, param)

        Returns the doc string of the *Backend* member function that is
        responsible for setting the value of *param*, or, returns a list
        of all params with their doc string.

        * *param:* The parameter, a string. If not provided, or if set
          to *None*, *help_param()* will return a dictionary of all
          parameters and their doc strings.

        Returns a string, the doc string for the specified parameter, or
        a dictinary of all parameters with their doc strings.
        """

        def all_params():
            phelp = {}

            for k in self.params.keys():
                if self.params[k].__doc__:
                    phelp[k] = self.params[k].__doc__.lstrip().rstrip()
                else:
                    phelp[k] = '(No help for %s available)' % (k)
            return phelp

        if not param:
            return all_params()

        if param in self.params.keys():
            set_method=self.params[param]
            return set_method.__doc__.lstrip().rstrip() if set_method.__doc__ \
                else "No help for '%s' is available" % param
        else:
            msg = "No such parameter '%s'. Legal parameters in this mode are: %s" \
                % (param, str(self.params.keys()))
            print 'No such parameter %s' % param
            print 'Legal parameters in this mode are:'

            ps = all_params()

            for p in ps:
                print p, ':'
                print ps[p]
            raise Exception(msg)

    def get_status(self, keys = None):
        """
        get_status(keys=None)

        Returns the specified key's value, or the values of several
        keys, or the entire contents of the shared memory status buffer.

        *keys == None:* The entire buffer is returned, as a
        dictionary containing the key/value pairs.

        *keys is a list or tuple of keys*: returns a dictionary
        containing the requested subset of key/value pairs.

        *keys is a single string*: a single value will be looked up and
        returned using 'keys' as the single key.
        """


        if self.test_mode: # TBF could find a way to return what is in memory.
            kv = self.mock_status_mem
        else:
            self.status.read()
            kv = dict(self.status.items())

        if type(keys) == list or type(keys) == tuple:
            rd = {}
            for key in keys:
                if str(key) in kv:
                    rd[key] = kv[str(key)]
            return rd
        elif keys == None:
            return kv
        else:
            return kv[str(keys)]

    def set_status(self, **kwargs):
        """
        set_status(self, **kwargs)

        Modifies the status shared memory on the HPC. Updates the values
        for the keys specified in the parameter list as keyword value
        pairs.

        This is a low-level function that will set any arbitrary key to
        any value in status shared memory. Use *set_param()* where
        possible.

        Example::

          self.set_status(PROJID='JUNK', OBS_MODE='HBW')
        """

        if not self.test_mode:
            self.status.read()

        for k,v in kwargs.items():
            if self.test_mode:
                self.mock_status_mem[str(k)] = str(v)
            else:
                self.status.update(str(k), str(v))

        if not self.test_mode:
            self.status.write()

    def set_register(self, **kwargs):
        """
        set_register(self, **kwargs)

        Updates the named roach registers with the values for the keys specified
        in the parameter list as keyword value pairs.

        This is a low-level function that directly sets FPGA
        registers. Use *set_param()* where possible.

        Example::

          self.set_register(FFT_SHIFT=0xaaaaaaaa, N_CHAN=6)

        This sets the FFT_SHIFT and N_CHAN registers.

        **Note:** Only integer values are supported.
        """

        for k,v in kwargs.items():
            if self.test_mode:
                self.mock_roach_registers[str(k)] = int(str(v),0)
            else:
                # print str(k), '<-', str(v), int(str(v),0)
                if self.roach:
                    old = self.roach.read_int(str(k))
                    new = int(str(v),0)

                    if new != old:
                        self.roach.write_int(str(k), new)

    def progdev(self, bof = None):
        """
        progdev(self, bof, frequency):

        Programs the ROACH2 with boffile 'bof'.

        * *bof:* A string, the name of the bof file.  This parameter
          defaults to 'None'; if no bof file is specified, the function
          will load the bof file specified for the current mode, which
          is specified in that mode's section of the configuration
          file..  A 'KeyError' will result if the current mode is not
          set.
        """

        if not bof:
            bof = self.mode.bof

        if self.test_mode:
            return "Programming", bof

        # Some modes will not have roach set.
        if self.roach:
            self.valon.set_frequency(0, convertToMHz(self.frequency))
            # reply, informs = self.roach._request("progdev", 20) # deprogram roach first
            reply, informs = self.roach._request("progdev") # deprogram roach first

            if reply.arguments[0] != 'ok':
                print "Warning, FPGA was not deprogrammed."

            print "progdev programming bof", str(bof)
            return self.roach.progdev(str(bof))

    def setScanLength(self, length):
        """
        This parameter controls how long the scan will last in seconds.
        """
        self.scan_length = length

    def setSource(self, source):
        """
        This parameter sets the SRC_NAME shared memory value.
        """
        self.source = source

    def setTelescope(self, telescope):
        """
        This parameter sets the TELESCOPE shared memory value.
        """
        self.telescope = telescope

    def setFilterBandwidth(self, fbw):
        """
        Filter bandwidth. Must be a value in [950, 1150, 1400]
        """

        if fbw not in self.filter_bw_bits:
            return (False, "Filter bandwidth must be one of %s" % str(self.filter_bw_bits.keys()))

        self.filter_bw = fbw
        return True

    # def setValonFrequency(self, vfreq):
    #     """
    #     reflects the value of the valon clock, read from the Bank Mode section
    #     of the config file.
    #     """
    #     self.frequency = vfreq

    def setObserver(self, observer):
        """
        Sets the observer keyword in FITS headers and status memory.
        """
        self.observer = observer

    def setProjectId(self, project):
        """
        Sets the project id for the session. This becomes part of the directory
        path for the backend data in the form:

            $DIBAS_DATA/<projectid>/<backend>/<data file>
        """
        self.projectid = project
        self.datadir = self.dataroot + "/" + self.projectid

    def setNoiseTone1(self, noise_tone):
        """
        Selects the noise source or test tone for channel 1
        noise_tone: one of NoiseTone.NOISE (or 0), NoiseTone.TONE (or 1)
        """
        self.noise_tone_1 = noise_tone

    def setNoiseTone2(self, noise_tone):
        """
        Selects the noise source or test tone for channel 2
        noise_tone: one of NoiseTone.NOISE (or 0), NoiseTone.TONE (or 1)
        """
        self.noise_tone_2 = noise_tone

    def setNoiseSource(self, noise_source):
        """
        Turns the noise source on or off
        noise_source: one of NoiseSource.OFF (or 0), NoiseSource.ON (or 1)
        """
        self.noise_source = noise_source

    # TBF! Need to break this out to child Backends.
    def net_config(self, data_ip = None, data_port = None, dest_ip = None, dest_port = None):
        """
        net_config(self, roach_ip = None, port = None)

        Configures the 10Gb/s interface on the roach.  This consists of
        sending the tap-start katcp command to initialize the FPGA's
        10Gb/s interface, and updating a couple of registers on the
        ROACH2 with the destination IP address and port of the HPC
        computer.

        All the parameters to this function have 'None' as default
        values; if any is ommitted from the call the function will use
        the corresponding value loaded from the configuration file.

        * *data_ip:* The IP address. Can be a string (ie '10.17.0.71'), or
          an integer of the same value (for that IP, it would be
          168886343) If given as a string, is converted to the integer
          representation.

        * *data_port:* The ROACH 10Gb/s port number. An integer with a
          16-bit value; that is, not to exceed 65535.

        * *dest_ip:* the IP address to send packets to; this is the 10Gb/s
          IP address of the HPC computer. Same format as 'data_ip'.

        * *dest_port:* The 10Gb/s port on the HPC machine to send data
          to.
        """

        # don't do this if unit testing.
        if self.test_mode:
            return
        # don't do this if we don't control a roach, as can happen in
        # GUPPI CODD modes.
        if not self.roach:
            return

        if data_ip == None:
            data_ip = self._ip_string_to_int(self.bank.datahost)
        else:
            data_ip = self._ip_string_to_int(data_ip)

        if data_port == None:
            data_port = self.bank.dataport

        if dest_ip == None:
            dest_ip = self._ip_string_to_int(self.bank.dest_ip)
        else:
            dest_ip = self._ip_string_to_int(dest_ip)

        if dest_port == None:
            dest_port = self.bank.dest_port

        if type(data_ip) != int or type(data_port) != int \
                or type (dest_ip) != int or type (dest_port) != int \
                or data_port > 65535 or dest_port > 65535:
            raise Exception("Improperly formatted IP addresses and/or ports. "
                            "IP must be integer values or dotted quad strings. "
                            "Ports must be integer values < 65535.")

        gigbit_name = self.mode.gigabit_interface_name
        dest_ip_register_name = self.mode.dest_ip_register_name
        dest_port_register_name = self.mode.dest_port_register_name

        self.roach.tap_start("tap0", gigbit_name,
                             self.bank.mac_base + data_ip, data_ip, data_port)
        self.roach.write_int(dest_ip_register_name, dest_ip)
        self.roach.write_int(dest_port_register_name, dest_port)
        regs = [gigbit_name]
        set_arp(self.roach, regs, self.hpc_macs)

        return 'ok'

    def _wait_for_status(self, reg, expected, max_delay):
        """
        _wait_for_status(self, reg, expected, max_delay)

        Waits for the shared memory status register 'reg' to read value
        'expected'. Returns True if that value appears within
        'max_delay' (milliseconds), False if not. 'wait' returns the
        actual time waited (mS), within 100 mS.
        """
        if self.test_mode:
            return (True,0)

        value = ""
        wait = timedelta()
        increment = timedelta(microseconds=100000)

        while wait < max_delay:
            time.sleep(0.1)
            wait += increment

            try:
                value = self.get_status(reg)
            except:
                continue

            if value == expected:
                return (True,wait)

        return (False,wait) #timed out


    def start(self, starttime):
        """start(self, starttime = None)

        *starttime:* a datetime object

        --OR--

        *starttime:* a tuple or list(for ease of JSON serialization) of
        datetime compatible values: (year, month, day, hour, minute,
        second, microsecond).

        Sets up the system for a measurement and kicks it off at the
        appropriate time, based on 'starttime'.  If 'starttime' is not
        on a PPS boundary it is bumped up to the next PPS boundary.  If
        'starttime' is not given, the earliest possible start time is
        used.

        *start()* will require a needed arm delay time, which is
        specified in every mode section of the configuration file as
        *needed_arm_delay*. During this delay it tells the HPC program
        to start its net, accum and disk threads, and waits for the HPC
        program to report that it is receiving data. It then calculates
        the time it needs to sleep until just after the penultimate PPS
        signal. At that time it wakes up and arms the ROACH. The ROACH
        should then send the initial packet at that time.

        Because the child backend start procedure is time consuming,
        this start merely records the start time, sets a flag, and
        returns. This allows the Dealer to issue starts quickly to every
        Player. The player's watchdog will then pick up on the start
        flag, and perform the lengthy start procedure more-or-less in
        parallel.

        """
        pass

    def stop(self):
        """
        Stops a scan.
        """
        return (False, "stop() not implemented for this backend.")

    def monitor(self):
        """monitor(self)

        monitor() requests that the DAQ program go into monitor
        mode. This is handy for troubleshooting issues like no data. In
        monitor mode the DAQ's net thread starts receiving data but does
        not do anything with that data. However the thread's status may
        be read in the status memory: NETSTAT will say 'receiving' if
        packets are arriving, 'waiting' if not.

        """
        return (False, "monitor() not implemented for this backend.")

    def scan_status(self):
        """
        Returns the current state of a scan.
        """
        return (False, "scan_status() not implemented for this backend.")

    def earliest_start(self):
        return (False, "earliest_start() not implemented on this backend.")

    def stop(self):
        self.hpc_cmd('STOP')


    def prepare(self):
        """
        Perform calculations for the current set of parameter settings
        """
        pass

    def reset_roach(self):
        """
        reset_roach(self):

        Sends a sequence of commands to reset the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the *MODE* sections of the
        configuration file.
        """
        if self.test_mode:
            return

        # All banks have roaches if they are incoherent, VEGAS, or coherent masters.
        if self.roach:
            self._execute_phase(self.mode.reset_phase)


    def set_if_bits(self):
        """
        Programs the I2C based on bandwidth, noise source, and noise
        tones. Intended to be called from Backend's 'prepare' function.
        """
        # Set the IF bits based on the filter bandwidth, and also the
        # noise source & noise tone paramters.

        bits = self.filter_bw_bits[self.filter_bw] \
            | {NoiseSource.OFF: 0x01, NoiseSource.ON: 0x00}[self.noise_source] \
            | {NoiseTone.NOISE: 0x00, NoiseTone.TONE: 0x02}[self.noise_tone_1] \
            | {NoiseTone.NOISE: 0x00, NoiseTone.TONE: 0x04}[self.noise_tone_2]
        self.setI2CValue(0x38, 1, bits)

    def arm_roach(self):
        """
        arm_roach(self):

        Sends a sequence of commands to arm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """

        if self.test_mode:
            return

        # All banks have roaches if they are incoherent, VEGAS, or coherent masters.
        if self.roach:
            self._execute_phase(self.mode.arm_phase)

    def disarm_roach(self):
        """
        disarm_roach(self):

        Sends a sequence of commands to disarm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """

        if self.test_mode:
            return

        # All banks have roaches if they are incoherent, VEGAS, or coherent masters.
        if self.roach:
             self._execute_phase(self.mode.postarm_phase)

    def _execute_phase(self, phase):
        """
        execute_phase(self, phase)

        A super simple interpreter of commands to do things to the FPGA,
        such as resetting the ROACH, arming it, etc. By interpreting the
        string list 'phase' the specific sequence of commands can then
        be stored in a configuration file instead of being hard-coded in
        the code.

        self: This object.

        phase: a sequence of string tuples, where the first element of
        the tuple is the command, and the second element the parameter:
        [("sg_sync, "0x12"), ("wait", "0.5"), ("arm", "0"), ("arm",
        "1"), ...]
        """

        def write_to_roach(reg, val):
            if self.test_mode:
                print reg, "=", val
            else:
                val=int(val,0)
                self.roach.write_int(reg, val)

        def wait(op):
            op = float(op)
            time.sleep(op)

        for cmd, param in phase:
            if cmd == 'wait':
                wait(param)
            else:
                # in this case 'cmd' is really a roach register, fished
                # out of the config file, dependent on the mode it is
                # intended. For example: 'arm' for VEGAS bofs, 'ARM' for
                # GUPPI bofs, etc.
                write_to_roach(cmd, param)

    def _ip_string_to_int(self, ip):
        """
        _ip_string_to_int(self, ip)

        Converts an IP string, ie '170.0.0.1', and returns an
        integer. If 'ip' is already an int, returns it.
        """
        if type(ip) == str:
            quads = ip.split('.')

            if len(quads) != 4:
                raise Exception("IP string representation %s not a dotted quad!" % ip)

            return reduce(lambda x, y: x + y,
                          map(lambda x: int(x[0]) << x[1],
                              zip(quads, [24, 16, 8, 0])))
        elif type(ip) == int:
            return ip
        else:
            raise Exception("IP address must be a dotted quad string, or an integer value.")

    def clear_switching_states(self):
        """
        resets/deletes the switching_states (backend dependent)
        """
        raise Exception("This backend does not support switching signals.")

    def add_switching_state(self, duration, blank = False, cal = False, sig_ref_1 = False):
        """
        add_switching_state(duration, blank, cal, sig):

        Add a description of one switching phase (backend dependent).

        * *duration* is the length of this phase in seconds,
        * *blank* is the state of the blanking signal (True = blank, False = no blank)
        * *cal* is the state of the cal signal (True = cal, False = no cal)
        * *sig_ref_1* is the state of the sig_ref_1 signal (True = ref, false = sig)

        If any of the named parameters is not provided, that parameter
        defaults to *False*.

        Example to set up a 8 phase signal (4-phase if blanking is not
        considered) with blanking, cal, and sig/ref, total of 400 mS::

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
        raise Exception("This backend does not support switching signals.")

    def set_gbt_ss(self, period, ss_list):
        """
        set_gbt_ss(period, ss_list):

        adds a complete GBT style switching signal description.

        period: The complete period length of the switching signal.
        ss_list: A list of GBT phase components. Each component is a tuple:
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
        raise Exception("This backend does not support switching signals.")

    def round_second_up(self, the_datetime):
        """
        Round the provided time up to the nearest second.

        * *the_datetime:* A time as represented by a *datetime* object.
        """
        one_sec = timedelta(seconds = 1)
        if the_datetime.microsecond != 0:
            the_datetime += one_sec
            the_datetime = the_datetime.replace(microsecond = 0)
        return the_datetime
