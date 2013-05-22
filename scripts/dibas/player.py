######################################################################
#
#  player.py - A ZMQ server that controls the operations of a ROACH2.
#
#  Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Correspondence concerning GBT software should be addressed as follows:
#  GBT Operations
#  National Radio Astronomy Observatory
#  P. O. Box 2
#  Green Bank, WV 24944-0002 USA
#
######################################################################

import zmq
import ConfigParser
import inspect
import subprocess
import signal
import sys
import traceback
import time

from vegas_utils import vegas_status
from corr import katcp_wrapper

class AutoVivification(dict):
    """
    Implementation of perl's autovivification feature.
    (see http://stackoverflow.com/questions/651794/whats-the-best-way-to-initialize-a-dict-of-dicts-in-python)
    """
    def __getitem__(self, item):
        try:
            return dict.__getitem__(self, item)
        except KeyError:
            value = self[item] = type(self)()
            return value

class BankData(object):
    """
    Container for all Bank specific data:
    datahost   : The 10Gbs IP address for the roach
    dataport   : The 10Gbs port for the roach
    dest_ip    : The 10Gbs HPC IP address
    dest_port  : The 10Gbs HPC port
    katcp_ip   : The KATCP host, on the 1Gbs network
    katcp_port : The KATCP port on the 1Gbs network
    synth_port : The Valon synthesizer serial port
    """
    def __init__(self):
        self.datahost = None
        self.dataport = None
        self.dest_ip = None
        self.dest_port = None
        self.katcp_ip = None
        self.katcp_port = None
        self.synth = None
        self.synth_port = None
        self.synth_ref = None
        self.synth_ref_freq = None
        self.synth_vco_range = None
        self.synth_options = None
        self.mac_base = (2 << 40) + (2 << 32)

    def __repr__(self):
        return "BankData (datahost=%s, dataport=%i, dest_ip=%s, dest_port=%i, " \
               "katcp_ip=%s, katcp_port=%i, synth=%s, synth_port=%s, synth_ref=%i, " \
               "synth_ref_freq=%i, synth_vco_range=(%i,%i), synth_options=(%i,%i,%i,%i), " \
               "mac_base=%i)" \
            % (self.datahost,
               self.dataport,
               self.dest_ip,
               self.dest_port,
               self.katcp_ip,
               self.katcp_port,
               self.synth,
               self.synth_port,
               self.synth_ref,
               self.synth_ref_freq,
               self.synth_vco_range[0],
               self.synth_vco_range[1],
               self.synth_options[0],
               self.synth_options[1],
               self.synth_options[2],
               self.synth_options[3],
               self.mac_base)

class ModeData(object):
    """
    Container for all Mode specific data:
    acc_len           :
    filter_bw         :
    frequency         : The Valon frequency
    bof               : The ROACH bof file for this mode
    sg_period         :
    obs_mode          : Key sets vegas_hpc modes
    reset_phase       : The sequence of commands,data that reset the roach
    arm_phase         : The sequence of commands,data that arm the roach
    postarm_phase     : The sequence of commands,data sent after arming
    master_slave_sels : The map of master/slave select values, keyed to
                        [master/slave][int/ext switching signal][int/ext blanking]
    """

    def __init__(self):
        self.acc_len = None
        self.filter_bw = None
        self.frequency = None
        self.bof = None
        self.sg_period = None
        self.obs_mode = None
        self.reset_phase = []
        self.arm_phase = []
        self.postarm_phase = []
        self.master_slave_sels = AutoVivification()

    def __repr__(self):
        return "ModeData (acc_len=%i, filter_bw=%i, frequency=%f, bof=%s, " \
            "sg_period=%i, obs_mode=%s, reset_phase=%s, arm_phase=%s, " \
            "postarm_phase=%s, master_slave_sels=%s)" % \
            (self.acc_len,
             self.filter_bw,
             self.frequency,
             self.bof,
             self.sg_period,
             self.obs_mode,
             self.reset_phase,
             self.arm_phase,
             self.postarm_phase,
             self.master_slave_sels)

def print_doc(obj):
    print obj.__doc__

class Bank(object):
    """
    A roach bank manager class.
    """

    def __init__(self, bank_name):
        self.bank_name = bank_name.upper()
        self.roach_data = BankData()
        self.mode_data = {}
        self.current_mode = None
        self.hpc_process = None
        self.vegas_devel_path = None
        self.vegas_hpc = None
        self.fifo_name = None
        self.status = vegas_status()
        self.read_config_file('./dibas.conf')

    def hpc_cmd(self, cmd):
        """
        Opens the named pipe to the HPC program, sends 'cmd', and closes
        the pipe.
        """
        fh = open(self.fifo_name, 'w')
        fh.write(cmd)
        fh.close()

    def start_hpc(self):
        """
        Starts the HPC program running. Stops any previously running instance.
        """

        self.stop_hpc()
        sp_path = self.vegas_devel_path + '/src/vegas_hpc/bin/' + self.vegas_hpc
        self.hpc_process = subprocess.Popen((sp_path, ))


    def stop_hpc(self):
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

        return killed

    def read_config_file(self, filename):
        """
        read_config_file(filename)

        Reads the config file 'filename' and loads the values into data
        structures in memory. 'filename' should be a fully qualified
        filename. The config file contains a 'bank' section of interest to
        this bank; in addition, it contains any number of 'MODEX' sections,
        where 'X' is a mode name/number.
        """

        try:
            bank = self.bank_name
            print "bank =", bank, "filename =", filename
            config = ConfigParser.ConfigParser()
            config.readfp(open(filename))

            # Get config info for subprocess
            self.vegas_devel_path = config.get('DEFAULTS', 'vegas_devel_path').lstrip('"').rstrip('"')
            self.vegas_hpc = config.get('DEFAULTS', 'vegas-hpc').lstrip('"').rstrip('"')
            self.fifo_name = config.get('DEFAULTS', 'fifo_name').lstrip('"').rstrip('"')

            # Get config info on this bank's ROACH2
            self.roach_data.datahost = config.get(bank, 'datahost').lstrip('"').rstrip('"')
            self.roach_data.dataport = config.getint(bank, 'dataport')
            self.roach_data.dest_ip = int(config.get(bank, 'dest_ip'), 0)
            self.roach_data.dest_port = config.getint(bank, 'dest_port')
            self.roach_data.katcp_ip = config.get(bank, 'katcp_ip').lstrip('"').rstrip('"')
            self.roach_data.katcp_port = config.getint(bank, 'katcp_port')
            self.roach_data.synth = config.get(bank, 'synth')
            self.roach_data.synth_port = config.get(bank, 'synth_port').lstrip('"').rstrip('"')
            self.roach_data.synth_ref = 1 if config.get(bank, 'synth_ref') == 'external' else 0
            self.roach_data.synth_ref_freq = config.getint(bank, 'synth_ref_freq')
            self.roach_data.synth_vco_range = [int(i) for i in config.get(bank, 'synth_vco_range').split(',')]
            self.roach_data.synth_options = [int(i) for i in config.get(bank, 'synth_options').split(',')]

            # Get config info on all modes
            modes = [s for s in config.sections() if 'MODE' in s]

            for mode in modes:
                m = ModeData()
                m.acc_len                    = config.getint(mode, 'acc_len')
                m.filter_bw                  = config.getint(mode, 'filter_bw')
                m.frequency                  = config.getfloat(mode, 'frequency')
                m.bof                        = config.get(mode, 'bof_file')
                m.sg_period                  = config.getint(mode, 'sg_period')
                m.obs_mode                   = config.get(mode, 'obs_mode')
                mssel                        = config.get(mode, 'master_slave_sel').split(',')
                m.master_slave_sels[1][0][0] = int(mssel[0], 0)
                m.master_slave_sels[1][0][1] = int(mssel[1], 0)
                m.master_slave_sels[1][1][1] = int(mssel[2], 0)
                m.master_slave_sels[0][0][0] = int(mssel[3], 0)
                m.master_slave_sels[0][0][1] = int(mssel[4], 0)
                m.master_slave_sels[0][1][1] = int(mssel[5], 0)

                # reset, arm and postarm phases; for ease of use, they
                # should be read, then the commands should be paired
                # with their parameters, eg ["sg_sync","0x12",
                # "wait","0.5", ...] should become [("sg_sync", "0x12"),
                # ("wait", "0.5"), ...]
                reset_phase     = config.get(mode, 'reset_phase').split(',')
                arm_phase       = config.get(mode, 'arm_phase').split(',')
                postarm_phase   = config.get(mode, 'postarm_phase').split(',')
                m.reset_phase   = zip(reset_phase[0::2], reset_phase[1::2])
                m.arm_phase     = zip(arm_phase[0::2], arm_phase[1::2])
                m.postarm_phase = zip(postarm_phase[0::2], postarm_phase[1::2])

                self.mode_data[mode] = m
        except ConfigParser.NoSectionError as e:
            print str(e)
            return str(e)

        self.roach = katcp_wrapper.FpgaClient(self.roach_data.katcp_ip, self.roach_data.katcp_port)
        time.sleep(1)

        # The Valon can be on this host ('local') or on the ROACH ('katcp'). Create accordingly.
        if self.roach_data.synth == 'local':
            import valon_synth
            self.valon = valon_synth.Synthesizer(self.roach_data.synth_port)
        elif self.roach_data.synth == 'katcp':
            from valon_katcp import ValonKATCP
            self.valon = ValonKATCP(self.roach, self.roach_data.synth_port)
        else:
            raise ValonException("Unrecognized option %s for valon synthesizer" % self.roach_data.synth)

        # Valon is now assumed to be working
        self.valon.set_ref_select(self.roach_data.synth_ref)
        self.valon.set_reference(self.roach_data.synth_ref_freq)
        self.valon.set_vco_range(0, *self.roach_data.synth_vco_range)
        self.valon.set_options(0, *self.roach_data.synth_options)

        print "connecting to %s, port %i" % (self.roach_data.katcp_ip, self.roach_data.katcp_port)
        print self.roach_data
        return "config file loaded."

    def set_mode(self, mode, force = False):
        """
        set_mode(mode, force=False)

        mode: mode name

        Sets the operating mode for the roach.  Does this by programming
        the roach.

        mode: A string; A keyword which is one of the '[MODEX]'
        sections of the configuration file, which must have been loaded
        earlier.

        force: A boolean flag; if 'True' and the new mode is the same as
        the current mode, the mode will be reloaded. It is set to
        'False' by default, in which case the new mode will not be
        reloaded if it is already the current mode.

        Returns a tuple consisting of (status, 'msg') where 'status' is
        a boolean, 'True' if the mode was loaded, 'False' otherwise; and
        'msg' explains the error if any.

        Example: s, msg = f.set_mode('MODE1')
                 s, msg = f.set_mode(mode='MODE1', force=True)
        """
        if mode:
            if mode in self.mode_data:
                if force or mode != self.current_mode:
                    self.current_mode = mode
                    print "New mode specified!"
                    # self.roach.valon_frequency(self.mode_data[mode].frequency) ?? don't know yet.
                    self.set_status(FPGACLK = self.mode_data[mode].frequency)
                    self.progdev()
                    self.net_config()
                    self.reset_roach()
                    self.set_status(OBS_MODE = self.mode_data[mode].obs_mode)
                    self.roach.write_int('acc_len', self.mode_data[mode].acc_len - 1)
                    self.roach.write_int('sg_period', self.mode_data[mode].sg_period)
                    self.valon.set_frequency(0, self.mode_data[mode].frequency)

                    return (True, 'New mode %s set!' % mode)
                else:
                    return (False, 'Mode %s is already set! Use \'force=True\' to force.' % mode)
            else:
                return (False, 'Mode %s is not supported.' % mode)
        else:
            return (False, "Mode is 'None', doing nothing.")

    def get_mode(self):
        """
        get_mode()

        Returns the current operating mode for the bank.
        """
        return self.current_mode

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

        self.status.read()
        kv = dict(self.status.items())

        if type(keys) == list or type(keys) == tuple:
            return {key: kv[str(key)] for key in keys if str(key) in kv}
        elif keys == None:
            return dict(self.status.items())
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
        self.status.read()

        for k,v in kwargs.items():
            self.status.update(str(k), str(v))
        self.status.write()

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
            bof = self.mode_data[self.current_mode].bof

        print "progdev programming bof", str(bof)
        return self.roach.progdev(str(bof))

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
        if data_ip == None:
            data_ip = self._ip_string_to_int(self.roach_data.datahost)
        else:
            data_ip = self._ip_string_to_int(data_ip)

        if data_port == None:
            data_port = self.roach_data.dataport

        if dest_ip == None:
            dest_ip = self._ip_string_to_int(self.roach_data.dest_ip)
        else:
            dest_ip = self._ip_string_to_int(dest_ip)

        if dest_port == None:
            dest_port = self.roach_data.dest_port

        if type(data_ip) != int or type(data_port) != int \
                or type (dest_ip) != int or type (dest_port) != int \
                or data_port > 65535 or dest_port > 65535:
            raise Exception("Improperly formatted IP addresses and/or ports. "
                            "IP must be integer values or dotted quad strings. "
                            "Ports must be integer values > 65535.")

        self.roach.tap_start("tap0", "gbe0", self.roach_data.mac_base + data_ip, data_ip, data_port)
        self.roach.write_int('dest_ip', dest_ip)
        self.roach.write_int('dest_port', dest_port)
        return 'ok'

    def reset_roach(self):
        """
        reset_roach(self):

        Sends a sequence of commands to reset the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        self._execute_phase(self.mode_data[self.current_mode].reset_phase)

    def arm_roach(self):
        """
        arm_roach(self):

        Sends a sequence of commands to arm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        self._execute_phase(self.mode_data[self.current_mode].arm_phase)

    def disarm_roach(self):
        """
        disarm_roach(self):

        Sends a sequence of commands to disarm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        self._execute_phase(self.mode_data[self.current_mode].postarm_phase)

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

        def arm(op):
            op = int(op, 0)
            self.roach.write_int('arm', op)

        def sg_sync(op):
            op = int(op, 0)
            self.roach.write_int('sg_sync', op)

        def wait(op):
            op = float(op)
            time.sleep(op)

        doit = {arm.__name__: arm, sg_sync.__name__: sg_sync, wait.__name__: wait}

        for cmd, param in phase:
            doit[cmd](param)


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


def dispatch(f_data, f_dict):
    try:
        proc = f_dict[str(f_data['proc'])]
        args = f_data['args']
        kwargs = f_data['kwargs']
        return proc(*args, **kwargs)
    except:
        return formatExceptionInfo(10)

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

def main_loop(bank_name, url):
    global not_done
    bank = Bank(bank_name)
    bank_methods = {'bank.' + p[0]: p[1] \
                        for p in inspect.getmembers(bank, predicate=inspect.ismethod)}
    katcp_methods= {'katcp.' + p[0]: p[1] \
                        for p in inspect.getmembers(bank.roach, predicate=inspect.ismethod)}
    f_dict = dict(bank_methods.items() + katcp_methods.items())
    exported_funcs = [(ef, f_dict[ef].__doc__) \
                          for ef in filter(lambda x:x[0] != '_', f_dict.keys())]
    ctx = zmq.Context()
    s = ctx.socket(zmq.REP)
    s.bind(url)
    poller = zmq.Poller()
    poller.register(s, zmq.POLLIN)

    while not_done:
        try:
            socks = dict(poller.poll(500))

            if s in socks and socks[s] == zmq.POLLIN:
                message = s.recv_json()

                if message['proc'] == 'list_methods':
                    s.send_json(exported_funcs)
                else:
                    ret_msg = dispatch(message, f_dict)
                    s.send_json(ret_msg)
        except zmq.core.ZMQError as e:
            print "zmq.core.ZMQError:", str(e)

    print "exiting main_loop()..."

def signal_handler(signal, frame):
    global not_done
    not_done = False

not_done = True


if __name__ == '__main__':
    if len(sys.argv) > 1:
        bank_name = sys.argv[1]
        if len(sys.argv) > 2:
            url = sys.argv[2]
        else:
            url = 'tcp://0.0.0.0:6667'

        signal.signal(signal.SIGINT, signal_handler)
        main_loop(bank_name, url)
