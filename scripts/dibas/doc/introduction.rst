Introduction
============

The DIBAS spectrometer control software is a distributed system written in Python that consists of several backend servers, each running on an HPC computer, designated as *Players*, controlled and coordinated by a simple client library called the *Dealer* using a ZeroMQ RPC mechanism with JSON encoding.

.. figure:: dealer-player.png
   :scale: 75 %
   :alt: Block diagram of Dealer and Players

   *Figure 1: Block diagram of Dealer and Players*

Things to note when thinking about Dealer and Players:

   * The Dealer process and the Player processes are not the same, nor do they need to be on the same host. All the Dealer needs is network access to the HPC computers running the Players.
   * The Dealer and Player processes do not need the same Python installation (though they may of course use the same installation)
   * The Dealer does not import the Player, katcp_wrapper, etc. modules. Nor does it need access to any of the specific pulsar coefficient files etc. needed by the Guppi modes. It only requires one module, PyZMQ, and the URLs to the players. Thus it may be run on a much simpler and more modest system than those needed by the Players.
   * The Player does however need the full installation in :code:`/opt/dibas`.

In other words the Dealer is a thin client to the Players, and serves as a means to control and coordinate the Players.

The Player
==========

Players may either be run interactively (useful for debugging), or be run as daemons and may remain running indefinitely. Players must know a great deal about the DIBAS configuration, and must have access to the DIBAS configuration file, VEGAS and GUPPI libraries and DAQ programs, and all needed 3d party modules.

Running the Player
------------------

The player may be run interactively. As user `dibas` on the desired HPC machine::

   $ source /opt/dibas/dibas.bash
   $ cd /opt/dibas/lib/python
   $ ipython -i player.py
   In [1]: p = Bank('BANKA') # 'Bank' is the main Player class
   In [2]: p.get_status() # etc...

This is very useful to debug specific problems in Player configuration. (The use of ipython for this purpose is highly recommended as it provides discovery of object attributes and full access to all attribute docstrings.)

Configuring a Player for an observation is generally done via values called *parameters*. Thus the steps needed are:

  * Set the mode
  * Set all parameters
  * call the :code:`prepare()` function
  * call the :code:`start()` function

So, continuing the interactive session above, configuring and running a scan would look like::

   In [3]: d.set_mode('MODE1')
   In [4]: d.set_param(observer='John Smith')
   In [5]: d.set_param(project_id='TGBT17B_504_02')
   In [6]: d.set_param(exposure=1.0)
   In [7]: d.set_param(scan_length=30.0)
   # set switching states, simple cal on/cal off for 1 second
   In [8]: d.clear_switching_states()
   In [9]: d.add_switching_state(0.002, blank=True, cal=True)
   In [10]: d.add_switching_state(0.498, blank=False, cal=True)
   In [11]: d.add_switching_state(0.002, blank=True, cal=False)
   In [12]: d.add_switching_state(0.498, blank=False, cal=False)
   In [13]: d.prepare()
   In [14]: d.start()

**NOTE:** All these functions provide return values. See the class documentation for Player and Backends.

  * At  '3' above, we set a mode, in this case the wide-band spectral-line mode 'MODE1'.
  * At '4' through '7', we set some parameter values. At this point we are only setting values in the backend, but not sending anything to hardware or shared memory.
  * At '8' through '12' we specify a switching signal, in this case a simple cal on/cal off cycle lasting 1 second. '8' clears any previous switching signal. '9' sets Cal on, with a brief blanking period. '11' sets Cal off with a brief blanking period.
  * After all these values are entered, we call :code:`prepare()`. This function takes all the values entered above and any defaults set via the configuration file, computes any derivative values that it needs, and writes to status memory and configures the roach.

Help for parameters may be easily obtained by calling::

   In [15]: d.help_param()

A dictionary of parameters for that mode will be returned. The key is the parameter name, the value a brief explanation of what that parameter is.

While running interactively is an excellent way to experiment and debug, for everyday use the Players should be run as daemons, one on each HPC machine. Again, as user `dibas`, on the desired HPC machine::

   $ source /opt/dibas/dibas.bash
   $ player

The Player may be provided with an optional Bank identifier, which must have a matching section header in the configuration file. If not provided the Player will look at the configuration file and find the section that corresponds to the host it is running on and use that section header as a name.

**NOTE:** Managing multiple players on DIBAS is tedious, since there are up to 8 players involved. For this reason it is recommended that a package such as `Supervisor <http://supervisord.org/>`_ be used to manage the daemons.

The Dealer
==========

The Dealer library is lightweight in comparison to the Player. A Dealer only needs to know the URL(s) of one or more player(s), and will obtain all the information it needs from those Players. In addition to the Dealer code itself (``dealer.py`` & ``ZMQJSONProxy.py``), the Python installation for a Dealer needs only one specialized library, PyZMQ.

The Dealer may either be run interactively, or may be imported into scripts or the telescope control system as needed.

Dealers may come and go as needed. Further, there may be more than one Dealer running at any given time, each with a designated function. For example a status display program may run the Dealer interface to obtain status information for each Player and display it in one convenient location. Another Dealer may be included in the telescope control system and may be tasked with starting and stopping scans under telescope control. Yet another Dealer may be used in a user script to set up observations.

**NOTE**: In case of multiple Dealers care must be taken that Dealers do not issue conflicting instructions to Players.

Using the Dealer
----------------

The Dealer is not meant to be a stand-alone program. It is designed instead to be imported as a module in a Python program or script, or used interactively::

   $ source /opt/dibas/dibas.bash
   $ ipython
   In [1]: import dealer
   In [2]: d = dealer.Dealer()

In this example the Dealer is given no configuration information, so it fetches Player information from the DIBAS configuration file ``dibas.conf``. If this is not practical or desirable Player information may be supplied directly to the constructor in the form of a dictionary::

   $ ipython # no need to source dibas.bash
   In [1]: import dealer
   In [2]: players = {'BANKA':'tcp://172.18.0.1:6667', 'BANKB':'tcp://172.18.0.2:6667'}
   In [3]: d = dealer.Dealer(players)

In this case the URLs provided to the Dealer provide it with the information it needs to connect to the Players.

The Dealer may be dynamically configured to control any number of available Players; when it is first created it is configured to control the single player at 'BANKA'. So the next step after creating a Dealer is to set it up to control the desired Players::

   In [4]: d.list_available_players()
   Out[4]: ['BANKH', 'BANKC', 'BANKB', 'BANKA', 'BANKG', 'BANKF', 'BANKE', 'BANKD']
   In [5]: d.list_active_players()
   Out[5]: ['BANKA']
   In [6]: d.add_active_player('BANKB', 'BANKC')
   Out[6]: ['BANKA', 'BANKB', 'BANKC']

Thus may any of the players in the list of available players be added or removed (use :code:`remove_active_player()` in the same way to remove Players).
From this point on the same code examples in the 'Using the Player' section above can be run here--using the :code:`d` Dealer object in place of the :code:`p` Player object--and the dealer will simultaneously apply these calls to the three players added in '5' in the example above.

If needed the players may be controlled individually while in the Dealer library::

   In [7]: p = d.players['BANKA']
   In [8]: p.valon.get_frequency(0)
   Out[8]: 1500.0

etc.


Restarting Players
------------------

Players may be restared for any reason (updates, etc.) without the need to restart the process that contains the Dealer. The only consideration is that the restarted Player will need to be reconfigured by the Dealer.


Further reading
---------------

`VEGAS FPGA mode documentation <https://docs.google.com/document/d/1C_it02j8yqu_VZcYnN6aVC-jrgP4ClAxbj8KP4cH4EQ/edit?pli=1>`_
