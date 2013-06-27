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

from vegas_utils import vegas_status
from corr import katcp_wrapper
from datetime import datetime, timedelta
import VegasBackend
import GuppiBackend
import GuppiCODDBackend

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

class ConfigData(object):
    """
    A common base class for data read out of a config file using
    ConfigParser. It's main purpose is to serve as a common area for
    helper functions.
    """

    def read_kv_pairs(self, config, section, kvkey):
        """
        read_kv_pairs(self, config, section, kvkey)

        config: an open ConfigParser object
        section: the name of a section in the config file
        kvkey: the key of keys

        returns: A dictionary of kv pairs, empty if 'kvkey' is not
        there, or if it doesn't have any value, or if any of the values
        are not themselves keys in the section.

        Looks in the ConfigParser object for a key-of-keys 'kvkey' which
        list a set of arbitrary keys to be read that the program does
        not know about. The value associated with this key is a comma
        delimited list of keys. The function parses this, then iterates
        over the keys to obtain the values, returning a dictionary of
        key/value pairs:

        shmkeys = foo,bar,baz
        foo = frog
        bar = cat
        baz = dog

        -> {'bar': 'cat', 'baz': 'dog', 'foo': 'frog'}

        In the example above, 'shmkeys' is the key-of-keys, and
        'foo,bar,baz' are the arbitrary keys. The function will then
        know to read these keys. This allows any arbitrary key/value
        pair to be stored in the configuration and have it be read.  An
        example application is kv pairs to be stored in the DIBAS shared
        status memory.
        """
        try:
            # get the keys, stripping out any spaces
            keys = [key.lstrip().rstrip() for key in config.get(section, kvkey).split(',')]
        except ConfigParser.NoOptionError:
            return {}

        # now iterate over the keys, obtaining the values. Store in dictionary.
        kvpairs = {}
        for key in keys:
            try:
                val=config.get(section, key)
                kvpairs[key]=val
            except:
                print 'Warning no such key %s found, but was specified in the keys list of section %s' % (key,section)

        return kvpairs


class BankData(ConfigData):
    """
    Container for all Bank specific data:
    datahost        : The 10Gbs IP address for the roach
    dataport        : The 10Gbs port for the roach
    dest_ip         : The 10Gbs HPC IP address
    dest_port       : The 10Gbs HPC port
    katcp_ip        : The KATCP host, on the 1Gbs network
    katcp_port      : The KATCP port on the 1Gbs network
    synth_port      : The Valon synthesizer serial port
    synth_ref       : Valon internal/external reference setting
    synth_ref_freq  : Valon reference frequency
    synth_vco_range : Valon VCO range
    synth_rf_level  : Valon RF ouput level
    synth_options   : Valon option flags/values
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
        self.synth_rf_level = None
        self.synth_options = None
        self.mac_base = (2 << 40) + (2 << 32)
        self.shmkvpairs = None
        self.roach_kvpairs = None

    def __repr__(self):
        return "BankData (datahost=%s, dataport=%i, dest_ip=%s, dest_port=%i, " \
               "katcp_ip=%s, katcp_port=%i, synth=%s, synth_port=%s, synth_ref=%i, " \
               "synth_ref_freq=%i, synth_vco_range=%s, synth_rf_level=%i, " \
               "synth_options=%s, mac_base=%i, shmkvpairs=%s, roach_kvpairs=%s)" \
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
               str(self.synth_vco_range),
               self.synth_rf_level,
               str(self.synth_options),
               self.mac_base,
               str(self.shmkvpairs),
               str(self.roach_kvpairs))

    def load_config(self, config, bank):
        self.datahost = config.get(bank, 'datahost').lstrip('"').rstrip('"')
        self.dataport = config.getint(bank, 'dataport')
        self.dest_ip = int(config.get(bank, 'dest_ip'), 0)
        self.dest_port = config.getint(bank, 'dest_port')
        self.katcp_ip = config.get(bank, 'katcp_ip').lstrip('"').rstrip('"')
        self.katcp_port = config.getint(bank, 'katcp_port')
        self.synth = config.get(bank, 'synth')
        self.synth_port = config.get(bank, 'synth_port').lstrip('"').rstrip('"')
        self.synth_ref = 1 if config.get(bank, 'synth_ref') == 'external' else 0
        self.synth_ref_freq = config.getint(bank, 'synth_ref_freq')
        self.synth_vco_range = [int(i) for i in config.get(bank, 'synth_vco_range').split(',')]
        self.synth_rf_level = config.getint(bank, "synth_rf_level")
        self.synth_options = [int(i) for i in config.get(bank, 'synth_options').split(',')]
        self.shmkvpairs = self.read_kv_pairs(config, bank, 'shmkeys')
        self.roach_kvpairs = self.read_kv_pairs(config, bank, 'roach_reg_keys')

class ModeData(ConfigData):
    """
    Container for all Mode specific data:
    acc_len           :
    filter_bw         :
    frequency         : The Valon frequency
    bof               : The ROACH bof file for this mode
    sg_period         :
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
        self.reset_phase = []
        self.arm_phase = []
        self.postarm_phase = []
        self.master_slave_sels = AutoVivification()
        self.needed_arm_delay = None
        self.cdd_mode = None
        self.cdd_roach = None
        self.cdd_roach_ips = []
        self.cdd_hpcs = []
        self.cdd_master_hpc = None
        self.shmkvpairs = None
        self.roach_kvpairs = None


    def __repr__(self):
        return "ModeData (acc_len=%i, filter_bw=%i, frequency=%f, bof=%s, " \
            "sg_period=%i, reset_phase=%s, arm_phase=%s, " \
            "postarm_phase=%s, needed_arm_delay=%i, master_slave_sels=%s, " \
            "shmkvpairs=%s, roach_kvpairs=%s)" % \
            (self.acc_len,
             self.filter_bw,
             self.frequency,
             self.bof,
             self.sg_period,
             self.reset_phase,
             self.arm_phase,
             self.postarm_phase,
             str(self.needed_arm_delay),
             self.master_slave_sels,
             str(self.shmkvpairs),
             str(self.roach_kvpairs))

    def load_config(self, config, mode):
        try:
            self.acc_len                    = config.getint(mode, 'acc_len')
        except:
            self.acc_len=None
        self.filter_bw                  = config.getint(mode, 'filter_bw')
        self.frequency                  = config.getfloat(mode, 'frequency')
        self.bof                        = config.get(mode, 'bof_file')
        # Get config info for subprocess
        self.hpc_program   = config.get(mode, 'hpc_program').lstrip('"').rstrip('"')
        self.hpc_fifo_name = config.get(mode, 'hpc_fifo_name').lstrip('"').rstrip('"')
        self.backend_type  = config.get(mode, 'BACKEND')

        try:
            self.sg_period                  = config.getint(mode, 'sg_period')
        except:
            self.sg_period=None
        mssel                           = config.get(mode, 'master_slave_sel').split(',')
        self.master_slave_sels[1][0][0] = int(mssel[0], 0)
        self.master_slave_sels[1][0][1] = int(mssel[1], 0)
        self.master_slave_sels[1][1][1] = int(mssel[2], 0)
        self.master_slave_sels[0][0][0] = int(mssel[3], 0)
        self.master_slave_sels[0][0][1] = int(mssel[4], 0)
        self.master_slave_sels[0][1][1] = int(mssel[5], 0)

        # reset, arm and postarm phases; for ease of use, they
        # should be read, then the commands should be paired
        # with their parameters, eg ["sg_sync","0x12",
        # "wait","0.5", ...] should become [("sg_sync", "0x12"),
        # ("wait", "0.5"), ...]
        reset_phase     = config.get(mode, 'reset_phase').split(',')
        arm_phase       = config.get(mode, 'arm_phase').split(',')
        postarm_phase   = config.get(mode, 'postarm_phase').split(',')
        self.reset_phase   = zip(reset_phase[0::2], reset_phase[1::2])
        self.arm_phase     = zip(arm_phase[0::2], arm_phase[1::2])
        self.postarm_phase = zip(postarm_phase[0::2], postarm_phase[1::2])
        self.shmkvpairs = self.read_kv_pairs(config, mode, 'shmkeys')
        self.roach_kvpairs = self.read_kv_pairs(config, mode, 'roach_reg_keys')
        arm_delay =  config.getint(mode, 'needed_arm_delay')
        self.needed_arm_delay = timedelta(seconds = arm_delay)

        # These are optional, for the Coherent Dedispersion Modes
        self.cdd_mode = True;

        try:
            self.cdd_roach = config.get(mode, 'cdd_roach')
            self.cdd_roach_ips = config.get(mode, 'cdd_roach_ips').split(',')
            self.cdd_hpcs = config.get(mode, 'cdd_hpcs').split(',')
            self.cdd_master_hpc = config.get(mode, 'cdd_master_hpc')
        except ConfigParser.NoOptionError:
            self.cdd_mode = False

        try:
            self.gigabit_interface_name = config.get(mode, 'gigabit_interface_name')
            self.dest_ip_register_name = config.get(mode, 'dest_ip_register_name')
            self.dest_port_register_name = config.get(mode, 'dest_port_register_name')
        except:
            raise Exception("""One or more of gigabit_interface_name, dest_ip_register_name,
                               or dest_port_register_name is not defined in the mode section""")
        try:
            self.shmkvpairs = self.read_kv_pairs(config, mode, 'shmkeys')
        except:
            pass
        try:
            self.roach_kvpairs = self.read_kv_pairs(config, mode, 'roach_reg_keys')
        except:
            pass


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
        self.roach_data = BankData()
        self.mode_data = {}
        self.bank_data = {}
        self.current_mode = None
        self.hpc_process = None
        self.exec_path = None
        self.check_shared_memory()
        self.status = vegas_status()
        self.read_config_file(self.dibas_dir + '/etc/config/dibas.conf')
        self.scan_number = 1
        self.backend = None

    def __del__(self):
        """
        Perform cleanup activities for a Bank object.
        """
        # Stop the HPC program if it is running
        if self.hpc_process is not None:
            self.stop_hpc()

    def hpc_cmd(self, cmd):
        """
        Opens the named pipe to the HPC program, sends 'cmd', and closes
        the pipe.
        """

        if self.current_mode is None:
            raise Exception( "No current mode is selected" )

        if self.hpc_process is None:
            raise Exception( "HPC program has not been started" )

        fifo_name = self.mode_data[self.current_mode].hpc_fifo_name
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
        if self.current_mode is None:
            raise Exception( "No current mode is selected" )

        self.stop_hpc()
        hpc_program = self.mode_data[self.current_mode].hpc_program
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
                self.bank_data[bank] = b

            # Get config info on this bank's ROACH2. Normally there is 1
            # ROACH per Player/HPC node, so this is it.
            self.roach_data = self.bank_data[self.bank_name]

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
        self.roach = katcp_wrapper.FpgaClient(self.roach_data.katcp_ip, self.roach_data.katcp_port, timeout = 30.0)
        time.sleep(1) # It takes the KATCP interface a little while to get ready. It's used below
                      # by the Valon interface, so we must wait a little.

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
        self.valon.set_rf_level(0, self.roach_data.synth_rf_level)
        self.valon.set_options(0, *self.roach_data.synth_options)

        #load any shared-mem keys found in the BANK section:
        if self.roach_data.shmkvpairs:
            self.set_status(**self.roach_data.shmkvpairs)
        else:
            pass
            #print 'DEBUG no extra status memory setting in Bank section'
        #load any 'extra' register default settings for this bof also from the BANK section:
        if self.roach_data.roach_kvpairs:
            self.set_register(**self.roach_data.roach_kvpairs)
        else:
            pass
            #print 'DEBUG no extra register settings in Bank section'

        print "connecting to %s, port %i" % (self.roach_data.katcp_ip, self.roach_data.katcp_port)
        print self.roach_data
        return "config file loaded."

    def cdd_master(self):
        return self.bank_name == self.mode_data[self.current_mode].cdd_master_hpc

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
                        self.backend = VegasBackend.VegasBackend(self)
                    elif backend_type in ["GUPPI"]:
                        if self.mode_data[mode].cdd_mode:
                            self.backend = GuppiCODDBackend.GuppiCODDBackend(self)
                        else:
                            self.backend = GuppiBackend.GuppiBackend(self)
                    else:
                        Exception("Unknown backend type, or missing 'BACKEND' setting in config mode section")

                    f = self.mode_data[mode].frequency / 1e6

                    if self.mode_data[mode].cdd_mode:
                        print "CoDD mode!!!!!"
                        if self.cdd_master():
                            print 'CoDD Master!!!!!'
                            # If master, do all the roach stuff. Everyone else skip.
                            print "Valon frequency:", f
                            self.valon.set_frequency(0, f)
                            self.progdev()
                            self.net_config()  # TBF!!!! program the 8 network adapters.
                            self.reset_roach() # TBF!!!! Must consider case where Master's roach is not THE ROACH.
                            print 'BUG: Not setting acc_len or sg_period due to naming conflicts'
                            #if self.mode_data[mode].acc_len is not None:
                            #    self.roach.write_int('acc_len', self.mode_data[mode].acc_len - 1)
                            #if self.mode_data[mode].sg_period is not None:
                            #    self.roach.write_int('sg_period', self.mode_data[mode].sg_period)
                        else:
                            # Deprogram the roach. Every player will
                            # deprogram its roach; only the master Player in
                            # CDD mode will then program its roach.
                            reply, informs = self.roach._request("progdev")
                    else:
                        self.valon.set_frequency(0, f)
                        self.progdev()
                        self.net_config()
                        self.reset_roach()
                        print "NOT setting acc_len or sg_period due to name conflicts (Vegas bof vs Guppi Incoherent bof)"
                        #self.roach.write_int('acc_len', self.mode_data[mode].acc_len - 1)
                        #self.roach.write_int('sg_period', self.mode_data[mode].sg_period)
                    self.set_param(frequency=f)

                    self.set_status(FPGACLK = self.mode_data[mode].frequency / 8)
                    #load any shared-mem keys found in the bank section:
                    if self.mode_data[mode].shmkvpairs:
                        self.set_status(**self.mode_data[mode].shmkvpairs)
                    #load any Bank-based register settings from the BANK section:
                    if self.mode_data[mode].roach_kvpairs:
                        self.set_register(**self.mode_data[mode].roach_kvpairs)
                    else:
                        print 'DEBUG no extra Bank register settings'

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
            bof = self.mode_data[self.current_mode].bof

        reply, informs = self.roach._request("progdev") # deprogram roach first

        if reply.arguments[0] != 'ok':
            print "Warning, FPGA was not deprogrammed."

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
                            "Ports must be integer values < 65535.")

        def tap_data(ips, gigbit_name):
            rvals = []

            for i in range(0, len(ips)):
                tap = "tap%i" % i
                gbe = gigbit_name + '%i' % i
                # gbe = "tGX1_tGv2%i" % i
                ip = self._ip_string_to_int(ips[i])
                mac = self.roach_data.mac_base + ip
                port = self.roach_data.dataport
                rvals.append((tap, gbe, mac, ip, port))
            return rvals

        gigbit_name = self.mode_data[self.current_mode].gigabit_interface_name
        dest_ip_register_name = self.mode_data[self.current_mode].dest_ip_register_name
        dest_port_register_name = self.mode_data[self.current_mode].dest_port_register_name

        if self.mode_data[self.current_mode].cdd_mode:
            taps = tap_data(self.mode_data[self.current_mode].cdd_roach_ips, gigbit_name)

            for tap in taps:
                self.roach.tap_start(*tap)

            hpcs = self.mode_data[self.current_mode].cdd_hpcs

            for i in range(0, len(hpcs)):
                ip_reg = dest_ip_register_name + '%i' % i
                pt_reg = dest_port_register_name + '%i' % i
                dest_ip = self.bank_data[hpcs[i]].dest_ip
                dest_port = self.bank_data[hpcs[i]].dest_port
                self.roach.write_int(ip_reg, dest_ip)
                self.roach.write_int(pt_reg, dest_port)
        else:
            self.roach.tap_start("tap0", gigbit_name, self.roach_data.mac_base + data_ip, data_ip, data_port)
            #self.roach.tap_start("tap0", "gbe0", self.roach_data.mac_base + data_ip, data_ip, data_port)
            self.roach.write_int(dest_ip_register_name, dest_ip)
            self.roach.write_int(dest_port_register_name, dest_port)
        return 'ok'

    def write_hpc_ip_register(self, dest_ip, dest_port):
        """
        write the destination IP and destination Port to the appropriate registers
        """
        interface_name = self.mode_data[self.current_mode].gigabit_interface_name
        dest_ip_name   = self.mode_data[self.current_mode].dest_ip_reg_name
        dest_port_name = self.mode_data[self.current_mode].dest_port_name
        self.roach.write_int(dest_ip_name,   dest_ip)
        self.roach.write_int(dest_port_name, dest_port)

    def _wait_for_status(self, reg, expected, max_delay):
        """
        _wait_for_status(self, reg, expected, max_delay)

        Waits for the shared memory status register 'reg' to read value
        'expected'. Returns True if that value appears within
        'max_delay' (milliseconds), False if not. 'wait' returns the
        actual time waited (mS), within 100 mS.
        """
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

    def start(self, starttime = None):
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
        if not self.current_mode:
            raise Exception("No mode currently set!")

        def round_second_up(the_datetime):
            if the_datetime.microsecond != 0:
                sec = the_datetime.second + 1
                the_datetime = the_datetime.replace(second = sec).replace(microsecond = 0)
            return the_datetime

        now = datetime.now()
        earliest_start = round_second_up(now + self.mode_data[self.current_mode].needed_arm_delay)

        if starttime:
            if type(starttime) == tuple or type(starttime) == list:
                starttime = datetime(*starttime)

            if type(starttime) != datatime:
                raise Exception("starttime must be a datetime or datetime compatible tuple or list.")

            # Force the start time to the next 1-second boundary. The
            # ROACH is triggered by a 1PPS signal.
            starttime = round_second_up(starttime)
            # starttime must be 'needed_arm_delay' seconds from now.
            if starttime < earliest_start:
                raise Exception("Not enough time to arm ROACH.")
        else: # No start time provided
            starttime = earliest_start

        # everything OK now, starttime is valid, go through the start procedure.
        max_delay = self.mode_data[self.current_mode].needed_arm_delay - timedelta(microseconds = 1500000)

        # The CODD bof's don't have a status register
        if not self.cdd_mode:
            val = self.roach.read_int('status')
            if val & 0x01:
                self.reset_roach()

        self.hpc_cmd('START')
        status,wait = self._wait_for_status('NETSTAT', 'receiving', max_delay)

        if not status:
            self.hpc_cmd('STOP')
            raise Exception("start(): timed out waiting for 'NETSTAT=receiving'")

        print "start(): waited %s for HPC program to be ready." % str(wait)

        # now sleep until arm_time
        #        PPS        PPS
        # ________|__________|_____
        #          ^         ^
        #       arm_time  start_time
        arm_time = starttime - timedelta(microseconds = 900000)
        now = datetime.now()

        if now > arm_time:
            self.hpc_cmd('STOP')
            raise Exception("start(): deadline missed, arm time is in the past.")

        tdelta = arm_time - now
        sleep_time = tdelta.seconds + tdelta.microseconds / 1e6
        time.sleep(sleep_time)
        # We're now within a second of the desired start time. Arm:
        self.arm_roach()


    def stop(self):
        self.hpc_cmd('STOP')

    def exposure(self, x):
        """
        exposure(x)

        x: Floating point value, integration time in seconds

        Sets the integration time, in seconds.
        """
        self.set_status(EXPOSURE=x)
        return (True, "EXPOSURE=%i" % x)

    def nsubband(self, x):
        """
        nsubband(x)

        x: The number of subbands, either 1 or 8

        Sets the number of subbands.
        """
        if x in (1, 8):
            self.set_status(NSUBBAND=x)
            return (True, "NSUBBAND=%i" % x)
        else:
            return (False, "NSUBBAND must be set to 1 or 8")

    def npol(self, x):
        """
        """
        self.set_status(NPOL=x)
        return (True, "NPOL=%i" % x)

    def nchan(self, x):
        """
        """
        self.set_status(NCHAN=x)
        return (True, "NCHAN=%i" % x)

    def chan_bw(self, x):
        """
        """
        self.set_status(CHAN_BW=x)
        self.vegas.chan_bw_dep(x)
        return (True, "CHAN_BW=%i" % x)

    def frequency(self, x):
        """
        """
        self.valon.set_frequency(0, x / 1e6)
        self.set_status(FPGACLK=x / 8)
        return (True, "frequency=%i" % x)

    def set_observer(self, person):
        self.set_status(OBSERVER=person)

    def set_obsid(self, id):
        self.set_status(OBSID=id)


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

        if self.mode_data[self.current_mode].cdd_mode:
            if self.cdd_master():
                self._execute_phase(self.mode_data[self.current_mode].reset_phase)
        else:
            self._execute_phase(self.mode_data[self.current_mode].reset_phase)

    def arm_roach(self):
        """
        arm_roach(self):

        Sends a sequence of commands to arm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        if self.mode_data[self.current_mode].cdd_mode:
            if self.cdd_master():
                self._execute_phase(self.mode_data[self.current_mode].arm_phase)
        else:
            self._execute_phase(self.mode_data[self.current_mode].arm_phase)

    def disarm_roach(self):
        """
        disarm_roach(self):

        Sends a sequence of commands to disarm the ROACH. This is mode
        dependent and mode should have been specified in advance, as the
        sequence of commands is obtained from the 'MODEX' section of the
        configuration file.
        """
        if self.mode_data[self.current_mode].cdd_mode:
            if self.cdd_master():
                self._execute_phase(self.mode_data[self.current_mode].postarm_phase)
        else:
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

        def write_reg(op):
            """ write_reg(self,op)

            Perform a write operation to a named register. The config file syntax for this operator
            should be:
                arm_phase=write_reg,myregistername=2,wait,0.2,write_reg,anotherregistername=42 etc.
            """
            s = op.split('=')
            if len(s) != 2 or len(s[0]) == 0 or len(s[1]) == 0:
                print 'write_reg syntax error %s -- no value written to register' % str(op)
            else:
                reg=s[0]
                val = int(s[1], 0)
                self.roach.write_int(reg, val)

        def sg_sync(op):
            op = int(op, 0)
            self.roach.write_int('sg_sync', op)

        def wait(op):
            op = float(op)
            time.sleep(op)

        doit = {arm.__name__: arm, sg_sync.__name__: sg_sync, wait.__name__: wait,
                write_reg.__name__:write_reg}

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
