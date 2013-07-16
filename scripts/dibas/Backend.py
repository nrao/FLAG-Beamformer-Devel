import struct
import ctypes
import binascii
import player
from vegas_utils import vegas_status

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
# Some backends
#
# @param
#
# @return
#
######################################################################

class Backend:
    """
    A base class which implements some of the common backend calculations (e.g switching).
    """
    def __init__(self, theBank, theMode, theRoach = None, theValon = None, unit_test = False):
        """
        Creates an instance of the vegas internals.
        Backend(theMode, theRoach)
        Where:

        theBank: the BankData object for this mode, containing all the
        data required by this backend.
        theMode: The ModeData object for this mode, containing all the
        data required for this mode.

        theRoach: The KATCP object to speak to this Backend's
        roach. This parameter is optional, both for testing and for
        modes that don't require communications with a roach.

        theValon: The Valon synth object.
        """

        # Save a reference to the bank
        self.test_mode = unit_test
        if self.test_mode:
            print "UNIT TEST MODE!!!"
            self.roach = None
            self.valon = None
            self.mock_status_mem = {}
            self.mock_roach_registers = {}
        else:
            self.roach = theRoach
            self.valon = theValon
            self.status = vegas_status()

        self.mode = theMode
        self.bank = theBank
        self.hpc_process = None
        self.obs_mode = 'SEARCH'
        self.max_databuf_size = 128 # in MBytes

        self.params = {}
        self.params["frequency"]      = self.setValonFrequency

    def __del__(self):
        """
        Perform cleanup activities for a Bank object.
        """
        # Stop the HPC program if it is running
        if self.test_mode:
            return

        if self.hpc_process is not None:
            self.stop_hpc()

    def hpc_cmd(self, cmd):
        """
        Opens the named pipe to the HPC program, sends 'cmd', and closes
        the pipe.
        """

        if self.test_mode:
            return

        if self.hpc_process is None:
            raise Exception( "HPC program has not been started" )

        fifo_name = self.mode.hpc_fifo_name
        if fifo_name is None:
            raise Exception("Configuration error: no field hpc_fifo_name specified in "
                            "MODE section of %s " % (self.current_mode))

        fh = open(fifo_name, 'w')
        fh.write(cmd)
        fh.close()

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

        sp_path = self.dibas_dir + '/exec/x86_64-linux/' + hpc_program
        print sp_path
        self.hpc_process = subprocess.Popen((sp_path, ))


    def stop_hpc(self):
        """
        stop_hpc()
        Stop the hpc program and make it exit.
        To stop an observation use 'stop()' instead.
        """

        if self.test_mode:
            return

        if self.hpc_process is None:
            return False # Nothing to do

        # First ask nicely
        self.hpc_cmd('stop')
        self.hpc_cmd('quit')
        time.sleep(1)
        # Kill if necessary
        if self.hpc_process and self.hpc_process.poll() == None: # running...
            self.hpc_process.send_signal(signal.SIGINT)
            time.sleep(1)

            if self.hpc_process.poll() == 0:
                killed = True
            else:
                self.hpc_process.kill()
                killed = True;
        else:
            killed = False

        self.hpc_process = None

        return killed

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


    def get_status(self, keys = None):
        """
        get_status(keys=None)

        Returns the specified key's value, or the values of several
        keys, or the entire contents of the shared memory status buffer.

        'keys' == None: The entire buffer is returned, as a
        dictionary containing the key/value pairs.

        'keys' is a list of keys, which are strings: returns a dictionary
        containing the requested subset of key/value pairs.

        'keys' is a single string: a single value will be looked up and
        returned using 'keys' as the single key.
        """


        if self.test_mode: # TBF could find a way to return what is in memory.
            kv = self.mock_status_mem
        else:
            self.status.read()
            kv = dict(self.status.items())

        if type(keys) == list or type(keys) == tuple:
            return {key: kv[str(key)] for key in keys if str(key) in kv}
        elif keys == None:
            return kv
        else:
            return kv[str(keys)]

    def set_status(self, **kwargs):
        """
        set_status(self, **kwargs)

        Updates the values for the keys specified in the parameter list
        as keyword value pairs. So:

          set_status(PROJID='JUNK', OBS_MODE='HBW')

        would set those two parameters.
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
        in the parameter list as keyword value pairs. ex:

          set_register(FFT_SHIFT=0xaaaaaaaa, N_CHAN=6)

        would set the FFT_SHIFT and N_CHAN registers.
        Note: Only integer values are supported.
        """

        for k,v in kwargs.items():
            if self.test_mode:
                self.mock_roach_registers[str(k)] = int(str(v),0)
            else:
                # print str(k), '<-', str(v), int(str(v),0)
                self.roach.write_int(str(k), int(str(v),0))

    def progdev(self, bof = None):
        """
        progdev(self, bof):

        Programs the ROACH2 with boffile 'bof'.

        bof: A string, the name of the bof file.  This parameter
        defaults to 'None'; if no bof file is specified, the function
        will load the bof file specified for the current mode.  A
        'KeyError' will result if the current mode is not set.
        """

        if not bof:
            bof = self.mode.bof

        if self.test_mode:
            return "Programming", bof

        # Some modes will not have roach set.
        if self.roach:
            reply, informs = self.roach._request("progdev") # deprogram roach first

            if reply.arguments[0] != 'ok':
                print "Warning, FPGA was not deprogrammed."

            print "progdev programming bof", str(bof)
            return self.roach.progdev(str(bof))

    def setValonFrequency(self, vfreq):
        """
        reflects the value of the valon clock, read from the Bank Mode section
        of the config file.
        """
        self.frequency = vfreq

    def cdd_master(self):
        """
        Returns 'True' if this is a CoDD backend and it is master. False otherwise.
        """
        return False # self.bank_name == self.mode_data[self.current_mode].cdd_master_hpc


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

        data_ip: The IP address. Can be a string (ie '10.17.0.71'), or
        an integer of the same value (for that IP, it would be
        168886343) If given as a string, is converted to the integer
        representation.

        data_port: The ROACH 10Gb/s port number. An integer with a
        16-bit value; that is, not to exceed 65535.

        dest_ip: the IP address to send packets to; this is the 10Gb/s
        IP address of the HPC computer. Same format as 'data_ip'.

        dest_port: The 10Gb/s port on the HPC machine to send data
        to.
        """

        if self.test_mode:
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

        gigbit_name = self.mode_data[self.current_mode].gigabit_interface_name
        dest_ip_register_name = self.mode_data[self.current_mode].dest_ip_register_name
        dest_port_register_name = self.mode_data[self.current_mode].dest_port_register_name

        self.roach.tap_start("tap0", gigbit_name, self.bank_data.mac_base + data_ip, data_ip, data_port)
        #self.roach.tap_start("tap0", "gbe0", self.bank_data.mac_base + data_ip, data_ip, data_port)
        self.roach.write_int(dest_ip_register_name, dest_ip)
        self.roach.write_int(dest_port_register_name, dest_port)
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
            value = self.get_status(reg)

            if value == expected:
                return (True,wait)

            time.sleep(0.1)
            wait += increment

        return (False,wait) #timed out


    def start(self, starttime):
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
        pass


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
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        if self.test_mode:
            return

        # All banks have roaches if they are incoherent, VEGAS, or coherent masters.
        if self.roach:
            self._execute_phase(self.mode.reset_phase)

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
                write_to_roach(cmd, val)


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
