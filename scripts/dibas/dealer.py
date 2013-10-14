######################################################################
#
#  dealer.py -- Bank controller.  One Dealer, many Players.
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
import os
import ConfigParser
import time
import threading

from datetime import datetime, timedelta
from ZMQJSONProxy import ZMQJSONProxyClient


def datetime_to_tuple(dt):
    return (dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.microsecond)

class Executor(threading.Thread):
    def __init__(self, player_name, method, args = (), kwargs = {}):
        threading.Thread.__init__(self)
        self.player = player_name
        self.method = method
        self.args = args
        self.kwargs = kwargs
        self.return_val = None

    def run(self):
        self.return_val = self.method(*self.args, **self.kwargs)

class BankProxy(ZMQJSONProxyClient):
    """
    This class remotely provides an interface to a Player 'Bank' object
    running on another computer.
    """
    def __init__(self, ctx, name, url = None):
        if url:
            self.url = url
        else:
            dibas_dir = os.getenv('DIBAS_DIR')

            if dibas_dir == None:
                raise Exception("'DIBAS_DIR' is not set!")

            config_file = dibas_dir + '/etc/config/dibas.conf'
            config = ConfigParser.ConfigParser()
            config.readfp(open(config_file))
            playerhost = config.get(name.upper(), 'hpchost').lstrip('"').rstrip('"')
            playerport = config.getint(name.upper(), 'player_port')
            self.url = "tcp://%s:%i" % (playerhost, playerport)

        ZMQJSONProxyClient.__init__(self, ctx, 'bank', self.url)
        self.roach = ZMQJSONProxyClient(ctx, 'bank.roach', self.url)
        self.valon = ZMQJSONProxyClient(ctx, 'bank.valon', self.url)


class Dealer(object):
    """
    Dealer brings together all Player Bank objects in one script,
    allowing them to be coordinated and to operate as one instrument.
    """
    def __init__(self, players = None):
        """
        Initializes a Dealer object. It does this by reading
        'dibas.conf' to determine how many BankProxy objects to create,
        then stores them in a dictionary for later use by the class.
        """
        self.ctx = zmq.Context()
        self.available_players = {}
        self.players = {}

        if players == None:
            dibas_dir = os.getenv('DIBAS_DIR')

            if dibas_dir == None:
                raise Exception("'DIBAS_DIR' is not set!")

            config_file = dibas_dir + '/etc/config/dibas.conf'
            config = ConfigParser.ConfigParser()
            config.readfp(open(config_file))
            player_list = [i.lstrip('" ,').rstrip('" ,') \
                               for i in config.get('DEALER', 'players').lstrip('"').rstrip('"').split()]

            for name in player_list:
                self.available_players[name] = BankProxy(self.ctx, name)
        else:
            if type(players) != dict:
                raise Exception('Players must be in form of dict: {"Bank":"URL"}')
            for p in players:
                self.available_players[p] = BankProxy(self.ctx, p, players[p])

        if "BANKA" in self.list_available_players():
            self.add_active_player("BANKA")

    def _execute(self, function, args = (), kwargs = {}):
        """Executes player functions serially, in the order they are fetched
        from the self.players dictionary. Do not use for any functions
        that take an appreciable length of time to execute.

        """
        rval = {}
        for p in self.players:
            try:
                method = self.players[p].__dict__[function]
                rval[p] = method(*args, **kwargs)
            except AttributeError as e:
                rval[p] = (False, "Lost connection to server for %s." % (p))

        return rval

    def _pexecute(self, function, args = (), kwargs = {}):
        """Executes player functions concurrently. Use for any functions that
        must be executed at a specific time, or functions that take a
        long time to complete.

        """
        rval = {}
        threads = []

        for p in self.players:
            try:
                method = self.players[p].__dict__[function]
                ex = Executor(p, method, args, kwargs)
                threads.append(ex)
                ex.start()
            except AttributeError as e:
                rval[p] = (False, "Lost connection to server for %s." % (p))

        # These all ran. AttributeError above causes the thread not to
        # be created and included in 'threads'.
        for t in threads:
            t.join()
            rval[t.player] = t.return_val

        return rval

    def list_available_players(self):
        """
        Lists the available players.
        """
        return self.available_players.keys()

    def list_active_players(self):
        """
        Lists the players selected for use from the available player pool.
        """
        return self.players.keys()

    def add_active_player(self, *args):
        """
        Adds the player(s) specified in in the argument list to the
        active list. The arguments must be strings, the names of the
        players:

        d.add_active_player('BANKA', 'BANKB')
        """
        try:
            for p in args:
                self.players[p] = self.available_players[p]
        except KeyError as e:
            print e, "not in list of available players"
        finally:
            return self.list_active_players()

    def remove_active_player(self, *args):
        """
        Removes the named player(s) from the active player list. The
        player arguments must be strings:

        d.remove_active_player('BANKA', 'BANKB')
        """
        try:
            for p in args:
                self.players.pop(p)
        except KeyError as e:
            print e, "not in list of active players."
        finally:
            return self.list_active_players()

    def set_scan_number(self, num):
        """
        set_scan_number(scan_number)

        Sets the scan number to 'num'
        """
        self.scan_number = num
        self._execute("set_scan_number", [num])

    def increment_scan_number(self):
        """
        increment_scan_number()

        Increments the current scan number
        """
        self.scan_number = self.scan_number+1
        return self._execute("increment_scan_number")

    def set_status(self, **kwargs):
        """
        set_status(self, **kwargs)

        Updates the values for the keys specified in the parameter list
        as keyword value pairs. So::

            d.set_status(PROJID='JUNK', OBS_MODE='HBW')

        would set those two parameters.
        """
        return self._execute("set_status", kwargs = kwargs)

    def get_status(self, keys = None):
        """
        get_status(keys=None)

        Returns the specified key's value, or the values of several
        keys, or the entire contents of the shared memory status
        buffer. Which operation is performed depends on the type of
        *keys*:

        * *keys is None:* The entire buffer is returned, as a
          dictionary containing the key/value pairs.

        * *keys is a list of strings:* returns a dictionary containing
          the requested subset of key/value pairs.

        * *keys is a single string:* a single value will be looked up
          and returned using 'keys' as the single key.
        """
        return self._execute("get_status", [keys])

    def list_modes(self):
        """
        list_modes():

        Returns a list of modes available.
        """
        players = self.players.keys()
        return self.players[players[0]].list_modes()

    def set_mode(self, mode, frequency = False, force = False):
        """
        set_mode(mode, frequency = False, force=False)

        Sets the operating mode for the roach.  Does this by programming
        the roach.

        *mode:* The mode name, a string; A keyword which is one of the
        '[MODEX]' sections of the configuration file, which must have
        been loaded earlier.

        *frequency:* The valon frequency for this mode. If not
         provided, last one used on this mode will be reused. The
         value is originally specified in the config file.

        *force:* A boolean flag; if 'True' and the new mode is the same
        as the current mode, the mode will be reloaded. It is set to
        'False' by default, in which case the new mode will not be
        reloaded if it is already the current mode.

        Returns a dictionary of tuples, where the keys are the Player
        names, and the values consists of (status, 'msg') where 'status'
        is a boolean, 'True' if the mode was loaded, 'False' otherwise;
        and 'msg' explains the error if any.

        Example::

          rval = d.set_mode('MODE1')
          rval = d.set_mode(mode='MODE1', force=True)
        """
        return self._pexecute("set_mode", [mode, frequency, force])

    def _all_same(self, m):
        """
        Given a map 'm', returns (True, val) if 'val' is the common
        value of every key in the map, or (False, m) if not.
        """
        if len(m) == 1 or reduce(lambda x, y: x[1] == y[1], m.items()):
            return (True, m.items()[0][1]) # True and value
        else:
            return (Fales, m) # false and dict.

    def get_mode(self):
        """
        get_mode(self):

        Returns the current mode, if all players agree. If not, returns
        a tuple consisting of (False, {bank:mode, bank:mode...})
        """
        m = self._execute("get_mode")
        return self._all_same(m)

    def earliest_start(self):
        """
        earliest_start(self):

        Returns the earliest time that all backends can be safely
        started. This is done by querying all the backends and selecting
        the furthest starttime in the future.
        """
        # TBF: player's 'earliest_start()' returns (True, (time tuple))
        # We want just the time tuple. Should throw if any player
        # returns 'False'.
        rval = _pexecute("earliest_start")
        player_starts = []

        for p in rval:
            if rval[p][0]: # if returned True...
                player_starts.append(rval[p][1])

        player_starts.sort() # once sorted the last element is the one we seek.
        earliest_start = player_starts[-1]
        return datetime(*earliest_start)

    def start(self, starttime = None):
        """
        start(self, starttime = None)

        *starttime:* a datetime with the desired start time, which should
        be in UTC, as that is how the player will interpret it. Default
        is None, in which case the start time will be negotiated with
        the players.
        """

        # 1. Negotiate earliest start time (UTC) with players:
        earliest_start = self.earliest_start()
        # 2. Check to see if given start time is reasonable
        if starttime:
             if earliest_start > starttime:
                return (False, "Start time %s is earlier that earliest possible start time %s" % \
                            (str(starttime), str(earliset_start)))
        else:
            starttime = earliest_start

        # 3. Tell them to go!
        st = datetime_to_tuple(starttime,)
        return self._pexecute("start", [st])

    def stop(self):
        """
        Stops a running scan, or exits monitor mode.
        """
        return self._execute("stop")

    def monitor(self):
        """
        monitor(self)

        monitor() requests that the DAQ program go into monitor
        mode. This is handy for troubleshooting issues like no data. In
        monitor mode the DAQ's net thread starts receiving data but does
        not do anything with that data. However the thread's status may
        be read in the status memory: NETSTAT will say 'receiving' if
        packets are arriving, 'waiting' if not.
        """
        return self._execute("monitor")

    def scan_status(self):
        """
        scan_status(self):

        Returns the state of currently running scan. The return type is
        a dictionary of tuples, backend dependent. Each dictionary key
        is the Player's name.
        """
        return self._execute("scan_status")

    def wait_for_scan(self, verbose = False):
        """
        Blocks while a scan is in progress, returning only when the scan
        is over, or on user input.

        *verbose:* If *True* will print status information every 3
         seconds.
        """

        scan_running = True

        while scan_running:
            player_states = self.scan_status()
            scan_states = [player_states[p][0] for p in self.players]
            scan_running = all(scan_states)

            if verbose:
                for p in self.players:
                    print p, "Scanning" if player_states[p][0] else "Stopped", \
                        player_states[p][1], player_states[p][2]

            if scan_running: # exit right away if not.
                time.sleep(3)

            if self._check_keypress('q') == True:
                print "Exiting 'wait_for_scan()'. Check scan state manually using 'scan_status()'," \
                    "or stop the scan using 'stop()'"
                break


    def prepare(self):
        """
        Perform calculations for the current set of parameter settings
        """
        return self._execute("prepare")

    def set_param(self, **kvpairs):
        """
        A pass-thru method which conveys a backend specific parameter to the modes parameter engine.

        Example usage::
          d.set_param(exposure=x,switch_period=1.0, ...)
        """
        return self._execute("set_param", kwargs = kvpairs)

    def help_param(self, param = None):
        """
        Returns the help doc string for a specified parameters, or a
        dictionary of parameters with their doc strings if *param* is
        None.

        *param:* A valid parameter name.  Should be *None* if help for
         all parameters is desired.
        """
        m = self._execute("help_param", [param])
        return m[m.keys()[0]]

    def get_param(self, param):
        """
        Returns the value a specified parameters, or a dictionary of
        parameters with their values if *param* is None.

        *param:* A valid parameter name.  Should be *None* if values for
         all parameters is desired.
        """
        return self._execute("get_param", [param])

    def _check_keypress(self, expected_ch):
        """
        Detect a user keystoke. If the keystroke matches the expected key,
        this returns *True*, otherwise *False*
        """
        import termios, fcntl, sys, os
        fd = sys.stdin.fileno()

        oldterm = termios.tcgetattr(fd)
        newattr = termios.tcgetattr(fd)
        newattr[3] = newattr[3] & ~termios.ICANON & ~termios.ECHO
        termios.tcsetattr(fd, termios.TCSANOW, newattr)

        oldflags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, oldflags | os.O_NONBLOCK)
        got_keypress = False
        try:
            c = sys.stdin.read(1)
            #print "Got character", repr(c)
            if c == expected_ch:
                got_keypress = True
        except IOError:
            got_keypress = False

        finally:
            termios.tcsetattr(fd, termios.TCSAFLUSH, oldterm)
            fcntl.fcntl(fd, fcntl.F_SETFL, oldflags)
        return got_keypress

    def clear_switching_states(self):
        """
        resets/deletes the switching_states (backend dependent)
        """
        return  self._execute("clear_switching_states")

    def add_switching_state(self, duration, blank = False, cal = False, sig_ref_1 = False):
        """
        add_switching_state(duration, blank, cal, sig_ref_1):

        Add a description of one switching phase (backend dependent).

        Where:

        * *duration* is the length of this phase in seconds,
        * *blank* is the state of the blanking signal (True = blank, False = no blank)
        * *cal* is the state of the cal signal (True = cal, False = no cal)
        * *sig_ref_1* is the state of the sig_ref_1 signal (True = ref, false = sig)

        Example to set up a 8 phase signal (4-phase if blanking is not
        considered) with blanking, cal, and sig/ref, total of 400 mS::

          d.clear_switching_states()                                                 # Bl Cal SR1
          d.add_switching_state(0.01, blank = True,  cal = True,  sig_ref_1 = True)  # --   |   |
          d.add_switching_state(0.09, blank = False, cal = True,  sig_ref_1 = True)  # |    |   |
          d.add_switching_state(0.01, blank = True,  cal = True,  sig_ref_1 = False) # --   | |
          d.add_switching_state(0.09, blank = False, cal = True,  sig_ref_1 = False) # |    | |
          d.add_switching_state(0.01, blank = True,  cal = False, sig_ref_1 = True)  # -- |     |
          d.add_switching_state(0.09, blank = False, cal = False, sig_ref_1 = True)  # |  |     |
          d.add_switching_state(0.01, blank = True,  cal = False, sig_ref_1 = False) # -- |   |
          d.add_switching_state(0.09, blank = False, cal = False, sig_ref_1 = False) # |  |   |

        """
        return self_execute("add_switching_state", [duration, blank, cal, sig_ref_1])

    def set_gbt_ss(self, period, ss_list):
        """
        set_gbt_ss(period, ss_list):

        adds a complete GBT style switching signal description.

        period: The complete period length of the switching signal.
        ss_list: A list of GBT phase components. Each component is a tuple:
        (phase_start, sig_ref, cal, blanking_time)
        There is one of these tuples per GBT style phase.

        Example::

            d.set_gbt_ss(period = 0.1,
                         ss_list = ((0.0, SWbits.SIG, SWbits.CALON, 0.025),
                                    (0.25, SWbits.SIG, SWbits.CALOFF, 0.025),
                                    (0.5, SWbits.REF, SWbits.CALON, 0.025),
                                    (0.75, SWbits.REF, SWbits.CALOFF, 0.025))
                        )

        """
        return self._execute("set_gbt_ss", [period, ss_list])
