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
import socket
from datetime import datetime, timedelta

def _hostname_to_ip(hostname):
    """
    _hostname_to_ip(hostname)

    Takes a hostname string and returns an IP address string::

    ip = _hostname_to_ip('vegasr2-1')
    print(ip)
    10.17.0.64
    """
    return socket.gethostbyaddr(hostname)[2][0]

def _ip_string_to_int(ip):
    """
    _ip_string_to_int(ip)

    Takes an IP address in string representation and returns an unsigned integer representation:

    iip = _ip_string_to_int('10.17.0.51')
    print(hex(iip))
    0x0A110040
    """
    return sum(map(lambda x, y: x << y, [int(p) for p in ip.split('.')], [24, 16, 8, 0]))

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

        *config:* an open ConfigParser object
        *section:* the name of a section in the config file
        *kvkey:* the key of keys

        returns: A dictionary of kv pairs, empty if *kvkey* is not
        there, or if it doesn't have any value, or if any of the values
        are not themselves keys in the section.

        Looks in the ConfigParser object for a key-of-keys *kvkey* which
        list a set of arbitrary keys to be read that the program does
        not know about. The value associated with this key is a comma
        delimited list of keys. The function parses this, then iterates
        over the keys to obtain the values, returning a dictionary of
        key/value pairs. Given an entry in a configuration file's
        section [MODE1] as follows::

         shmkeys = foo,bar,baz
         foo = frog
         bar = cat
         baz = dog

        then::

         cf.read_kv_pairs(config, 'MODE1', 'shmkeys')
         -> {'bar': 'cat', 'baz': 'dog', 'foo': 'frog'}

        These may then be used in any way by the Player code. One use,
        as implied in this example, is to store these values in shared
        status memory. Another use is to read register/value pairs to be
        directly written to the FPGA.
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
    Container for all Bank specific data.
    """
    def __init__(self):
        self.name = None
        """The bank name"""
        self.datahost = None
        """The 10Gbs IP address for the roach"""
        self.dataport = None
        """The 10Gbs port for the roach"""
        self.dest_ip = None
        """The 10Gbs HPC IP address"""
        self.dest_port = None
        """The KATCP host, on the 1Gbs network"""
        self.katcp_ip = None
        """The KATCP host, on the 1Gbs network"""
        self.katcp_port = None
        """The KATCP port on the 1Gbs network"""
        self.synth = None
        """The location of the Valon serial port: 'katcp' (on roach) or 'local' (on hpc machine)"""
        self.synth_port = None
        """The Valon serial port device (i.e. /dev/ttyS1)"""
        self.synth_ref = None
        """The reference frequency: 'internal' or 'external'"""
        self.synth_ref_freq = None
        """The frequency of the Valon's external reference"""
        self.synth_vco_range = None
        """Valon VCO range"""
        self.synth_rf_level = None
        """The Valon RF level, in dBm. Legal values are -4, -1, 2, and 5"""
        self.synth_options = None
        """
        List of Valon options. With the exception of the reference
        frequency multiplier, all of these are flags which either are
        clear (0) or set (1): doubler, halver, multiplier, low-spur
        """
        self.mac_base = (2 << 40) + (2 << 32)
        """
        The base mac address used to compute individual mac addresses
        for the roaches.
        """
        self.shmkvpairs = None
        """
        Arbitrary shared memory key/value pairs. Read from the config
        file and placed in shared status memory.
        """
        self.roach_kvpairs = None
        """
        Arbitrary FPGA register key/value pairs. Read from the config
        file and written directly to the FPGA.
        """
        self.i_am_master = None
        """
        Switching Signals master flag. *True* if this bank is the master.
        """

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
        """
        Given the open *ConfigFile* object *config*, loads data for
        *bank*. *config* normally is opened with the config file at
        ``$DIBAS_DIR/etc/config/dibas.conf``
        """
        self.name = bank
        self.datahost = _hostname_to_ip(config.get(bank, 'data_source_host').lstrip().rstrip())
        self.dataport = config.getint(bank, 'data_source_port')
        self.dest_ip = _ip_string_to_int(
            _hostname_to_ip(
                config.get(bank, 'data_destination_host').lstrip().rstrip()))
        self.dest_port = config.getint(bank, 'data_destination_port')
        self.katcp_ip = config.get(bank, 'katcp_ip').lstrip().rstrip()
        self.katcp_port = config.getint(bank, 'katcp_port')
        self.synth = config.get(bank, 'synth')
        self.synth_port = config.get(bank, 'synth_port').lstrip().rstrip()
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
    Container for all Mode specific data.
    """

    def __init__(self):
        self.mode = None
        """Mode name"""
        self.acc_len = None
        """BOF file specific value"""
        self.filter_bw = None
        """Filter bandwidth"""
        self.frequency = None
        """Valon frequency"""
        self.nchan = None
        """Number of channels, BOF specific value"""
        self.bof = None
        """BOF file for this mode"""
        self.sg_period = None
        """BOF specific value"""
        self.reset_phase = []
        """Sequence of FPGA register writes necessary to reset the ROACH."""
        self.arm_phase = []
        """Sequence of FPGA register writes necessary to arm the ROACH."""
        self.postarm_phase = []
        """Sequence of FPGA register writes required post-arm."""
        self.master_slave_sels = AutoVivification()
        """
        This dictionary contains the master/slave select values.

        The value chosen depends on whether the backend is master, what
        the switching signal source is (internal/external), and what the
        blanking source is (internal/external).

        Typical config file entry::

          0x00,0x00,0x00,0x0E,0x00,0x00

        The order of the elements is as follows, where 'm' is master,
        's' is slave, 'int' is internal, and 'ext' is external:

          m/int/int, m/int/ext, m/ext/ext, s/int/int, s/int/ext, s/ext/ext

        A typical use would be: ``ssg_ms_sel =
        self.mode.master_slave_sels[master][ss_source][bl_source]``
        where *master* is the master flag (0=slave, 1=master),
        *ss_source* is the switching signal source (0=internal or
        1=external), and *bl_source* is the blanking source (0=internal
        or 1=external)
        """
        self.needed_arm_delay = 0
        """
        The time needed by the backend to arm in this mode.
        """
        self.cdd_mode = None
        """
        Flag, whether this mode is a coherent de-dispersion mode.
        """
        self.cdd_roach = None
        """
        The roach that takes data for the coherent de-dispersion modes.
        """
        self.cdd_roach_ips = []
        """
        The IP addresses of the onboard network adapter for the CoDD
        roach. The CoDD roach has 8 of these.
        """
        self.cdd_hpcs = []
        """
        The IP addresses of the HPC computers that are the end-points
        for each the CoDD ROACH's network adapters. There are 8 of
        these.
        """
        self.cdd_master_hpc = None
        """
        In CoDD mode, the HPC that will control the ROACH. Since theCoDD
        modes only use 1 ROACH but 8 HPC machines, one must be
        designated to be the controller.
        """
        self.shmkvpairs = None
        """
        Arbitrary shared memory keys to be placed into status shared memory for this mode.
        """
        self.roach_kvpairs = None
        """
        Arbitrary FPGA register/value pairs that are to be written to the FPGA for this mode.
        """


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
        """
        Given the open *ConfigFile* object *config*, loads data for
        *mode*. *config* normally is opened with the config file at
        ``$DIBAS_DIR/etc/config/dibas.conf``
        """
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
            self.cdd_roach_ips = [_hostname_to_ip(hn.lstrip().rstrip())
                                  for hn in
                                  config.get(mode, 'cdd_data_interfaces').split(',')]
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
                    d_ip = _ip_string_to_int(
                        _hostname_to_ip(
                            config.get(i, 'data_destination_host').lstrip().rstrip()))
                    dprt = config.getint(i, 'data_destination_port')
                    self.cdd_hpc_ip_info.append( (d_ip, dprt) )
                except:
                    print "No dest_ip/dest_port information for cdd Bank %s" % i
                    pass
                    #raise Exception("No dest_ip/dest_port information for cdd Bank %s" % i)
