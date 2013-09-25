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
    try:
        rval = socket.gethostbyaddr(hostname)[2][0]
    except (TypeError, socket.gaierror):
        rval = None

    return rval

def _ip_string_to_int(ip):
    """_ip_string_to_int(ip)

    Takes an IP address in string representation and returns an integer
    representation:

    iip = _ip_string_to_int('10.17.0.51')
    print(hex(iip))
    0x0A110040

    """
    try:
        rval = sum(map(lambda x, y: x << y, [int(p) for p in ip.split('.')], [24, 16, 8, 0]))
    except (TypeError, AttributeError):
        rval = None

    return rval

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

    def __init__(self):
        self._mandatory_mode = False

    def _mandatory(self):
        self._mandatory_mode = True

    def _optional(self):
        self._mandatory_mode = False

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
        keys_string = self._get_string(section, kvkey)

        if keys_string:
            keys = [key.lstrip().rstrip() for key in keys_string.split(',')]
        else:
            return {}

        # now iterate over the keys, obtaining the values. Store in dictionary.
        kvpairs = {}

        for key in keys:
            val = self._get_string(section, key)

            if val:
                kvpairs[key]=val
            else:
                print 'Warning no such key %s found, but was specified in the keys list of section %s' \
                    % (key,section)

        return kvpairs

    def _get_value(self, section, key, val_type):
        """
        _get_value(self, config, section, key, val_type)

        Helper function that looks for 'key' in 'config' and returns the
        requested value as type 'val_type', or None if that is not
        possible or the key or value doesn't exist.

        *section:* the name of a section in the config file
        *key:* the key
        *val_type:* The desired value type. May be str, int, or float.
        """
        try:
            if val_type == str:
                val = self.config.get(section, key).lstrip().rstrip().lstrip('"').rstrip('"')
            elif val_type == int:
                val = self.config.getint(section, key)
            elif val_type == float:
                val = self.config.getfloat(section, key)
            else:
                val = None

            if val == 'N/A':
                val = None
        except (ConfigParser.NoOptionError, TypeError, ValueError):
            val = None

        # only record errors if in mandatory mode
        if self._mandatory_mode:
            if not val:
                self.errors.append(key)

        return val

    def _get_string(self, section, key):
        """
        get_string(section, key)

        Returns a value of type string, or None if that is not
        possible. The string is cleaned up: leading and trailing '"'
        characters and whitespaces are removed.

        *section:* the name of a section in the config file
        *key:* the key

        """
        return self._get_value(section, key, str)

    def _get_int(self, section, key):
        """
        get_int(section, key)

        Returns a value of type int, or None if that is not possible.

        *section:* the name of a section in the config file
        *key:* the key
        """
        return self._get_value(section, key, int)

    def _get_float(self, section, key):
        """
        get_float(section, key)

        Returns a value of type float, or None if that is not possible.

        *config:* an open ConfigParser object
        *section:* the name of a section in the config file
        *key:* the key
        """
        return self._get_value(section, key, float)

    def _throw_on_error(self):
        if self.errors:
            raise Exception("Bad keys found in section %s: %s" % (self.name, self.errors))

    def _print_errors(self):
        if self.errors:
            for i in self.errors:
                print "Error: Value for key %s was not read" % (i)

class BankData(ConfigData):
    """
    Container for all Bank specific data.
    """
    def __init__(self):
        ConfigData.__init__(self)
        self.name = None
        """The bank name"""
        self.has_roach = False
        """whether this bank controls a roach or not."""
        self.hpchost = None
        """The host name of the HPC computer hosting the Player"""
        self.player_port = None
        """The port number for the Player, used by the Dealer interface"""
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
        return "BankData (name=%s, has_roach=%s, datahost=%s, dataport=%i, dest_ip=%s," \
               "dest_port=%i, " \
               "katcp_ip=%s, katcp_port=%i, synth=%s, synth_port=%s, synth_ref=%i, " \
               "synth_ref_freq=%i, synth_vco_range=%s, synth_rf_level=%i, " \
               "synth_options=%s, mac_base=%i, shmkvpairs=%s, roach_kvpairs=%s, " \
               "i_am_master=%s)" \
            % (self.name,
               self.has_roach,
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
        self.config = config
        self.errors = []
        self.name = bank

        # Mandatory keys:
        self._mandatory()
        self.hpchost = self._get_string(bank, 'hpchost')
        self.player_port = self._get_int(bank, 'player_port')
        has_roach = self._get_string(bank, 'has_roach')
        master = self._get_string('DEFAULTS', 'who_is_master')
        data_destination_host = self._get_string(bank, 'data_destination_host')
        self.dest_port = self._get_int(bank, 'data_destination_port')
        self.dataport = self._get_int(bank, 'data_source_port')

        # the following are mandatory only if 'self.has_roach' is True
        if not self.has_roach:
            self._optional()

        self.datahost = _hostname_to_ip(
        self._get_string(bank, 'data_source_host'))
        self.katcp_ip = self._get_string(bank, 'katcp_ip')
        self.katcp_port = self._get_int(bank, 'katcp_port')
        self.synth = self._get_string(bank, 'synth')
        self.synth_port = self._get_string(bank, 'synth_port')
        synth_ref = self._get_string(bank, 'synth_ref')
        self.synth_ref_freq = self._get_int(bank, 'synth_ref_freq')
        synth_vco_range = self._get_string(bank, 'synth_vco_range')
        self.synth_rf_level = self._get_int(bank, "synth_rf_level")
        synth_options = self._get_string(bank, 'synth_options')

        # These are optional in all cases:
        self._optional()
        self.shmkvpairs = self.read_kv_pairs(config, bank, 'shmkeys')
        self.roach_kvpairs = self.read_kv_pairs(config, bank, 'roach_reg_keys')

        self._throw_on_error()

        # Now we have everything we need to do some processing.

        # The mandatory stuff. If we're here, we have the values:
        self.has_roach = True if has_roach == 'true' else False
        self.i_am_master = True if master == bank else False
        self.dest_ip = _ip_string_to_int(_hostname_to_ip(data_destination_host))

        # The optional stuff. Might be 'None', ignore that. Print any
        # other exceptions:
        try:
            self.synth_vco_range = [int(i) for i in synth_vco_range.split(',')] \
                                   if synth_vco_range else None
            self.synth_options = [int(i) for i in synth_options.split(',')] \
                                 if synth_options else None
            self.synth_ref = (1 if synth_ref == 'external' else 0) \
                             if synth_ref else None
        except Exception, e:
            if self.has_roach:
                print 'synth_vco_range =', synth_vco_range
                print 'synth_options =', synth_options
                print type(e), e


class ModeData(ConfigData):
    """
    Container for all Mode specific data.
    """

    def __init__(self):
        ConfigData.__init__(self)
        self.name = None
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
        self.name.master_slave_sels[master][ss_source][bl_source]``
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
        self.cdd_roach_ips = {}
        """The IP addresses of the onboard network adapter for the CoDD
        roach. The CoDD roach has 8 of these. This is a dictionary,
        keyed by bank name::

        datahost = self.cdd_roach_ips[bankname]

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
        return "ModeData (name=%s, acc_len=%s, filter_bw=%s, frequency=%s, nchan=%s, bof=%s, " \
            "sg_period=%s, reset_phase=%s, arm_phase=%s, " \
            "postarm_phase=%s, needed_arm_delay=%s, master_slave_sels=%s, " \
            "shmkvpairs=%s, roach_kvpairs=%s)" % \
            (str(self.name),
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
        self.config = config
        self.errors = []
        self.name          = mode

        self._mandatory()

        self.filter_bw               = self._get_int(mode,    'filter_bw')
        self.frequency               = self._get_float(mode,  'frequency')
        self.nchan                   = self._get_int(mode,    'nchan')
        self.bof                     = self._get_string(mode, 'bof_file')
        self.backend_type            = self._get_string(mode, 'BACKEND')
        self.hpc_program             = self._get_string(mode, 'hpc_program')
        self.hpc_fifo_name           = self._get_string(mode, 'hpc_fifo_name')
        arm_delay                    = self._get_int(mode,    'needed_arm_delay')
        self.gigabit_interface_name  = self._get_string(mode, 'gigabit_interface_name')
        self.dest_ip_register_name   = self._get_string(mode, 'dest_ip_register_name')
        self.dest_port_register_name = self._get_string(mode, 'dest_port_register_name')
        arm_phase                    = self._get_string(mode, 'arm_phase')

        self._optional()

        reset_phase         = self._get_string(mode,           'reset_phase')
        postarm_phase       = self._get_string(mode,           'postarm_phase')
        self.shmkvpairs     = self.read_kv_pairs(config, mode, 'shmkeys')
        self.roach_kvpairs  = self.read_kv_pairs(config, mode, 'roach_reg_keys')
        self.acc_len        = self._get_int(mode,              'acc_len')
        self.sg_period      = self._get_int(mode,              'sg_period')
        mssel_string        = self._get_string(mode,           'master_slave_sel')
        cdd_data_interfaces = self._get_string(mode,           'cdd_data_interfaces')
        cdd_hpcs            = self._get_string(mode,           'cdd_hpcs')
        self.cdd_master_hpc = self._get_string(mode,           'cdd_master_hpc')

        # this will throw if any key listed under 'self._mandatory()' did not load.
        self._throw_on_error()

        ######### Do some processing & validation now that we have the values #########
        self.needed_arm_delay = timedelta(seconds = arm_delay)

        # reset, arm and postarm phases; for ease of use, they
        # should be read, then the commands should be paired
        # with their parameters, eg ["sg_sync","0x12",
        # "wait","0.5", ...] should become [("sg_sync", "0x12"),
        # ("wait", "0.5"), ...]
        ap = arm_phase.split(',')
        self.arm_phase = zip(ap[0::2], ap[1::2])

        if reset_phase: # optional
            rp = reset_phase.split(',')
            self.reset_phase   = zip(rp[0::2], rp[1::2])
        else:
            self.reset_phase = None

        if postarm_phase: # optional
            pap = postarm_phase.split(',')
            self.postarm_phase = zip(pap[0::2], pap[1::2])
        else:
            self.reset_phase = None

        mssel = mssel_string.split(',')

        if len(mssel) == 6:
            self.master_slave_sels[1][0][0] = int(mssel[0], 0)
            self.master_slave_sels[1][0][1] = int(mssel[1], 0)
            self.master_slave_sels[1][1][1] = int(mssel[2], 0)
            self.master_slave_sels[0][0][0] = int(mssel[3], 0)
            self.master_slave_sels[0][0][1] = int(mssel[4], 0)
            self.master_slave_sels[0][1][1] = int(mssel[5], 0)
        else:
            msg = \
            """WARNING: The Master/Slave Select must have 6 values.  Please check
            the configuration in section %s. All Master/Slave Select
            values will be set to 0.

            """
            print msg % (self.name)

            self.master_slave_sels[1][0][0] = 0
            self.master_slave_sels[1][0][1] = 0
            self.master_slave_sels[1][1][1] = 0
            self.master_slave_sels[0][0][0] = 0
            self.master_slave_sels[0][0][1] = 0
            self.master_slave_sels[0][1][1] = 0

        # Make a determination if this is a CODD mode. In these modes,
        # the values in 'codd_mode_keys' are all not None:
        codd_mode_keys = (cdd_data_interfaces,
                          cdd_hpcs,
                          self.cdd_master_hpc)

        if all(codd_mode_keys):   # All are True
            self.cdd_mode = True
        elif any(codd_mode_keys): # Some are, some are not. What was intended?
            msg = \
            """WARNING: Some but not all CODD mode keys detected in section
            %s. This mode will therefore not be considered a CODD mode.
            If this truly is a CODD mode it will not function
            correctly. Check that the following keys are all present:

            \t'cdd_data_interfaces'
            \t'cdd_hpcs'
            \t'cdd_master_hpc'

            """
            print msg % (self.name)
            self.cdd_mode = False
        else:                     # All are False. Not CODD mode
            self.cdd_mode = False

        if self.cdd_mode:
            roach_ips = [_hostname_to_ip(hn.lstrip().rstrip())
                         for hn in
                         cdd_data_interfaces.split(',')]
            self.cdd_hpcs      = cdd_hpcs.split(',')
            self.cdd_roach_ips = dict(zip(self.cdd_hpcs, roach_ips))

            # If this mode has a list of HPC machines for CODD operation, fetch
            # the dest_ip and dest_port entries from the corresponding Bank sections.
            # The resulting list is in the order of ports, i.e the first entry in the
            # cdd_hpcs list specifies the IP_0 and PT_0 registers of the CODD bof.

            self.cdd_hpc_ip_info = []
            for i in self.cdd_hpcs:
                d_ip = _ip_string_to_int(
                    _hostname_to_ip(
                        self._get_string(i, 'data_destination_host')))
                dprt = self._get_int(i, 'data_destination_port')

                if d_ip and dprt:
                    self.cdd_hpc_ip_info.append( (d_ip, dprt) )
                else:
                    print "No dest_ip/dest_port information for cdd Bank %s" % i
                    raise Exception("No dest_ip/dest_port information for cdd Bank %s" % i)
