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
import os

from corr import katcp_wrapper
from datetime import datetime, timedelta
from ConfigData import ModeData, BankData, AutoVivification
import VegasBackend
import GuppiBackend
import GuppiCODDBackend


def print_doc(obj):
    print obj.__doc__

class Bank(object):
    """
    A roach bank manager class.
    """

    def __init__(self, bank_name):
        self.dibas_dir = os.getenv('DIBAS_DIR')

        if self.dibas_dir == None:
            raise Exception("'DIBAS_DIR' is not set!")

        self.bank_name = bank_name.upper()
        self.bank_data = BankData()
        self.mode_data = {}
        self.banks = {}
        self.current_mode = None
        self.check_shared_memory()
        self.read_config_file(self.dibas_dir + '/etc/config/dibas.conf')
        self.scan_number = 1
        self.backend = None

    def __del__(self):
        """
        Perform cleanup activities for a Bank object.
        """
        pass

    def check_shared_memory(self):
        """
        This method creates the status shared memory segment if necessary.
        If the segment exists, the state of the status memory is not modified.
        """
        sts_path = self.dibas_dir + '/bin/init_status_memory'
        sts_process = subprocess.Popen((sts_path,))
        waits=0
        # wait 10 seconds for the script to complete
        ret = None
        while waits < 5 and  ret == None:
            ret = sts_process.poll()
            waits = waits+1
            time.sleep(1)

        if ret is None or ret < 0:
            raise Exception("Status memory buffer re-create failed " \
                            " status = %s" % (ret))


    def reformat_data_buffers(self, mode):
        """
        Since the vegas and guppi HPC programs require different data buffer
        layouts, remove and re-create the databuffers as appropriate for the
        HPC program in use.
        """

        if self.current_mode is None:
            raise Exception( "No current mode is selected" )

        fmt_path = self.dibas_dir + '/bin/re_create_data_buffers'

        hpc_program = self.mode_data[self.current_mode].hpc_program
        if hpc_program is None:
            raise Exception("Configuration error: no field hpc_program specified in "
                            "MODE section of %s " % (self.current_mode))

        mem_fmt_process = subprocess.Popen((fmt_path,hpc_program))
        waits=0
        # wait 10 seconds for the script to complete
        ret = None
        while waits < 10 and  ret == None:
            ret = mem_fmt_process.poll()
            waits = waits+1
            time.sleep(1)

        if ret is None or ret < 0:
            raise Exception("data buffer re-create failed %s when changing " \
                            "to mode %s" % (ret, mode))

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

            # Get all bank data and store it. This is needed by any mode
            # where there is 1 ROACH and N Players & HPC programs
            banks = [s for s in config.sections() if 'BANK' in s]

            for bank in banks:
                b = BankData()
                b.load_config(config, bank)
                self.banks[bank] = b

            # Get config info on this bank's ROACH2. Normally there is 1
            # ROACH per Player/HPC node, so this is it.
            self.bank_data = self.banks[self.bank_name]

            # Get config info on all modes
            modes = [s for s in config.sections() if 'MODE' in s]

            for mode in modes:
                m = ModeData()
                m.load_config(config, mode)
                self.mode_data[mode] = m

        except ConfigParser.NoSectionError as e:
            print str(e)
            return str(e)

        # Now that all the configuration data is loaded, set up some basic things: KATCP, Valon, etc.
        # KATCP:
        self.roach = katcp_wrapper.FpgaClient(self.bank_data.katcp_ip, self.bank_data.katcp_port, timeout = 30.0)
        time.sleep(1) # It takes the KATCP interface a little while to get ready. It's used below
                      # by the Valon interface, so we must wait a little.

        # The Valon can be on this host ('local') or on the ROACH ('katcp'). Create accordingly.
        if self.bank_data.synth == 'local':
            import valon_synth
            self.valon = valon_synth.Synthesizer(self.bank_data.synth_port)
        elif self.bank_data.synth == 'katcp':
            from valon_katcp import ValonKATCP
            self.valon = ValonKATCP(self.roach, self.bank_data.synth_port)
        else:
            raise ValonException("Unrecognized option %s for valon synthesizer" % self.bank_data.synth)

        # Valon is now assumed to be working
        self.valon.set_ref_select(self.bank_data.synth_ref)
        self.valon.set_reference(self.bank_data.synth_ref_freq)
        self.valon.set_vco_range(0, *self.bank_data.synth_vco_range)
        self.valon.set_rf_level(0, self.bank_data.synth_rf_level)
        self.valon.set_options(0, *self.bank_data.synth_options)

        print "connecting to %s, port %i" % (self.bank_data.katcp_ip, self.bank_data.katcp_port)
        print self.bank_data
        return "config file loaded."

    def set_scan_number(self, num):
        """
        set_scan_number(scan_number)

        Sets the scan number to the value specified
        """
        self.scan_number = num
        self.set_status(SCANNUM=num)

    def increment_scan_number(self):
        """
        increment_scan_number()
        Increments the current scan number
        """
        self.scan_number = self.scan_number+1
        self.set_scan_number(self.scan_number)

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
                    if self.hpc_process is not None:
                        print "stopping HPC program"
                        self.stop_hpc()
                    #self.reformat_data_buffers(mode)
                    print 'Not reformatting buffers'

                    # Two different kinds of mode: Coherent Dedispersion
                    # (CDD) and everything else. CDD modes are
                    # characterised by having only 1 ROACH sending data
                    # to 8 different HPC servers. The ROACH has 8
                    # network adapters for the purpose. Because of the
                    # 1->8 arrangement, one of the Players is Master and
                    # programs the ROACH. The others set up everything
                    # except their ROACH; in fact the others deprogram
                    # their ROACH so that the IP addresses may be used
                    # in the one CDD ROACH.
                    #
                    # The other kind of mode has 1 ROACH -> 1 HPC
                    # server, so each Player programs its ROACH.

                    # based upon the mode's backend config setting, create the appropriate
                    # parameter calculator 'backend'
                    if self.backend is not None:
                        del(self.backend)
                        self.backend = None

                    backend_type = self.mode_data[mode].backend_type.upper()
                    if backend_type in ["VEGAS"]:
                        self.backend = VegasBackend.VegasBackend(self.bank_data,
                                                                 self.mode_data[mode],
                                                                 self.roach,
                                                                 self.valon)
                    elif backend_type in ["GUPPI"]:
                        if self.mode_data[mode].cdd_mode:
                            self.backend = GuppiCODDBackend.GuppiCODDBackend(self.bank_data,
                                                                             self.mode_data[mode],
                                                                             self.roach,
                                                                             self.valon)
                        else:
                            self.backend = GuppiBackend.GuppiBackend(self.bank_data,
                                                                     self.mode_data[mode],
                                                                     self.roach,
                                                                     self.valon)
                    else:
                        Exception("Unknown backend type, or missing 'BACKEND' setting in config mode section")

                    # set by backend: f = self.mode_data[mode].frequency / 1e6

                    # All this handle in backend:
                    # if self.mode_data[mode].cdd_mode:
                    #     print "CoDD mode!!!!!"
                    #     if self.cdd_master():
                    #         print 'CoDD Master!!!!!'
                    #         # If master, do all the roach stuff. Everyone else skip.
                    #         print "Valon frequency:", f
                    #         self.valon.set_frequency(0, f)
                    #         self.progdev()
                    #         self.net_config()  # TBF!!!! program the 8 network adapters.
                    #         self.reset_roach() # TBF!!!! Must consider case where Master's roach is not THE ROACH.
                    #         print 'BUG: Not setting acc_len or sg_period due to naming conflicts'
                    #         #if self.mode_data[mode].acc_len is not None:
                    #         #    self.roach.write_int('acc_len', self.mode_data[mode].acc_len - 1)
                    #         #if self.mode_data[mode].sg_period is not None:
                    #         #    self.roach.write_int('sg_period', self.mode_data[mode].sg_period)
                    #     else:
                    #         # Deprogram the roach. Every player will
                    #         # deprogram its roach; only the master Player in
                    #         # CDD mode will then program its roach.
                    #         reply, informs = self.roach._request("progdev")
                    # else:
                    #     self.valon.set_frequency(0, f)
                    #     self.progdev()
                    #     self.net_config()
                    #     self.reset_roach()
                    #     print "NOT setting acc_len or sg_period due to name conflicts (Vegas bof vs Guppi Incoherent bof)"
                    #     #self.roach.write_int('acc_len', self.mode_data[mode].acc_len - 1)
                    #     #self.roach.write_int('sg_period', self.mode_data[mode].sg_period)
                    # self.set_param(frequency=f)

                    # self.set_status(FPGACLK = self.mode_data[mode].frequency / 8)
                    # #load any shared-mem keys found in the bank section:
                    # if self.mode_data[mode].shmkvpairs:
                    #     self.set_status(**self.mode_data[mode].shmkvpairs)
                    # #load any Bank-based register settings from the BANK section:
                    # if self.mode_data[mode].roach_kvpairs:
                    #     self.set_register(**self.mode_data[mode].roach_kvpairs)
                    # else:
                    #     print 'DEBUG no extra Bank register settings'

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

    def start(self):
        """
        start()

        Instructs the backend to start tacking data.
        """

        if self.backend:
            self.backend.start()

    def exposure(self, x):
        """
        exposure(x)

        x: Floating point value, integration time in seconds

        Sets the integration time, in seconds.
        """
        if self.backend:
            return self.backend.exposure(x)

    def nsubband(self, x):
        """
        nsubband(x)

        x: The number of subbands, either 1 or 8

        Sets the number of subbands.
        """
        if self.backend:
            return self.backend.nsubband(x)

    def npol(self, x):
        """
        """
        if self.backend:
            return self.backend.npol(x)

    def nchan(self, x):
        """
        """
        if self.backend:
            return self.backend.nchan(x)

    def chan_bw(self, x):
        """
        """
        if self.backend:
            return self.backend.chan_bw(x)

    def frequency(self, x):
        """
        """
        if self.backend:
            return self.backend.frequency(x)

    def set_observer(self, person):
        """
        Sets the observer ID in status memory.
        """
        if self.backend:
            self.backend.set_observer(person)

    def set_obsid(self, id):
        """
        Sets the observation ID in status memory.
        """
        if self.backend:
            self.backend.set_obsid(id)

    def set_param(self, **kvpairs):
        """
        A pass-thru method which conveys a backend specific parameter to the modes parameter engine.

        Example usage:
        set_param(exposure=x,switch_period=1.0, ...)
        """

        if self.backend is not None:
            for k,v in kvpairs.items():
                self.backend.set_param(str(k), v)
        else:
            raise Exception("Cannot set parameters until a mode is selected")

    def prepare(self):
        """
        Perform calculations for the current set of parameter settings
        """
        if self.backend is not None:
            self.backend.prepare()
        else:
            raise Exception("Cannot prepare until a mode is selected")

    def reset_roach(self):
        """
        reset_roach(self):

        Sends a sequence of commands to reset the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """

        if self.backend:
            self.backend.reset_roach()


    def arm_roach(self):
        """
        arm_roach(self):

        Sends a sequence of commands to arm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        if self.backend:
            self.backend.arm_roach()

    def disarm_roach(self):
        """
        disarm_roach(self):

        Sends a sequence of commands to disarm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        if self.backend:
            self.backend.disarm_roach()

def _testCaseVegas1():

    # Not sure why I can't import SWbits from VegasBackend ???
    SIG=1
    REF=0
    CALON=1
    CALOFF=0

    b=Bank('BANKH')
    print "Setting Mode1"
    b.set_mode('MODE1')
    b.backend.set_switching_period(10.0)
    b.backend.add_switching_state(0.0,  SIG, CALON, 0.0)
    b.backend.add_switching_state(0.25, SIG, CALOFF, 0.0)
    b.backend.add_switching_state(0.5,  REF, CALON, 0.0)
    b.backend.add_switching_state(0.75, REF, CALOFF, 0.0)
    b.backend.prepare()



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
