######################################################################
#
#  ConfigData.py -- configuration data classes for DIBAS. Loads mode and
#  bank configuration data from specified configuration sections of a
#  config file and stores them in these classes.
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

import ConfigParser
from datetime import datetime, timedelta

class AutoVivification(dict):
    """
    Implementation of perl's autovivification feature. This allows a
    blank dictionary (one with value '{}') to be initialized using
    multiple keys at once:

    d = {}
    d['foo']['bar'] = 'baz'

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
        self.name = None
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
        self.i_am_master = None

    def __repr__(self):
        return "BankData (name=%s, datahost=%s, dataport=%i, dest_ip=%s, dest_port=%i, " \
               "katcp_ip=%s, katcp_port=%i, synth=%s, synth_port=%s, synth_ref=%i, " \
               "synth_ref_freq=%i, synth_vco_range=%s, synth_rf_level=%i, " \
               "synth_options=%s, mac_base=%i, shmkvpairs=%s, roach_kvpairs=%s, " \
               "i_am_master=%s)" \
            % (self.name,
               self.datahost,
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
               str(self.roach_kvpairs),
               str(self.i_am_master))

    def load_config(self, config, bank):
        self.name = bank
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
        self.i_am_master = True if config.get('DEFAULTS', 'who_is_master') == bank else False

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
        self.mode = None
        self.acc_len = None
        self.filter_bw = None
        self.frequency = None
        self.nchan = None
        self.bof = None
        self.sg_period = None
        self.reset_phase = []
        self.arm_phase = []
        self.postarm_phase = []
        self.master_slave_sels = AutoVivification()
        self.needed_arm_delay = 0
        self.cdd_mode = None
        self.cdd_roach = None
        self.cdd_roach_ips = []
        self.cdd_hpcs = []
        self.cdd_master_hpc = None
        self.shmkvpairs = None
        self.roach_kvpairs = None


    def __repr__(self):
        return "ModeData (mode=%s, acc_len=%s, filter_bw=%s, frequency=%s, nchan=%s, bof=%s, " \
            "sg_period=%s, reset_phase=%s, arm_phase=%s, " \
            "postarm_phase=%s, needed_arm_delay=%s, master_slave_sels=%s, " \
            "shmkvpairs=%s, roach_kvpairs=%s)" % \
            (str(self.mode),
             str(self.acc_len),
             str(self.filter_bw),
             str(self.frequency),
             str(self.nchan),
             str(self.bof),
             str(self.sg_period),
             str(self.reset_phase),
             str(self.arm_phase),
             str(self.postarm_phase),
             str(self.needed_arm_delay),
             str(self.master_slave_sels),
             str(self.shmkvpairs),
             str(self.roach_kvpairs))

    def load_config(self, config, mode):
        try:
            self.acc_len = config.getint(mode, 'acc_len')
        except:
            self.acc_len = None

        self.mode = mode
        self.filter_bw  = config.getint(mode, 'filter_bw')
        self.frequency  = config.getfloat(mode, 'frequency')
        self.nchan      = config.getint(mode, 'nchan')
        self.bof        = config.get(mode, 'bof_file')
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

        # If this mode has a list of HPC machines for CODD operation, fetch
        # the dest_ip and dest_port entries from the corresponding Bank sections.
        # The resulting list is in the order of ports, i.e the first entry in the
        # cdd_hpcs list specifies the IP_0 and PT_0 registers of the CODD bof.            
        if self.cdd_hpcs is not None:
            self.cdd_hpc_ip_info = []
            for i in self.cdd_hpcs:
                try:
                    d_ip = int(config.get(i, 'dest_ip'),0)
                    dprt = int(config.get(i, 'dest_port'),0)
                    self.cdd_hpc_ip_info.append( (d_ip, dprt) )
                except:
                    print "No dest_ip/dest_port information for cdd Bank %s" % i
                    pass
                    #raise Exception("No dest_ip/dest_port information for cdd Bank %s" % i)
                    
