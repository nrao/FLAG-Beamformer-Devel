DIBAS Configuration File
========================

Players obtain all their configuration information via the DIBAS configuration file ``dibas.conf``.

Location
--------

Players look for ``dibas.conf`` in the directory ``$DIBAS_DIR/etc/config``

Structure
---------

``dibas.conf`` is organized in sections. The sections are:

DEFAULTS:
^^^^^^^^^
Any value that is global to DIBAS goes in this section

Example::

  [DEFAULTS]
  telescope = SHAO
  who_is_master = BANKA
  player_port = 6667

HPCMACS:
^^^^^^^^
This section contains all the MAC addresses of the HPC computer 10Gb/s
ethernet interfaces. It is important that these addresses be kept
current with the actual hardware. Failure to do so will result in lost
packets!

Example::

   [HPCMACS]
   hpc1 = 0x0002C9EBC940
   hpc2 = 0x0002C9EBC900
   hpc3 = 0x0002C9EBC910
   # ... etc.

DEALER:
^^^^^^^
The dealer looks in this section to find out which players are available::

   [DEALER]
   players = BANKA, BANKB, BANKC, BANKD, BANKE, BANKF, BANKG, BANKH

The remaining sections consist of two categories of sections:

  * BANK (i.e. ``[BANKA]``, ``[BANKB]``
  * MODE (i.e. ``[MODE1]``, ``[CODD_MODE_64]`` etc.)

BANK:
^^^^^
This category of section contails all the hardware information needed
by one DIBAS bank, which consists of an HPC computer and a
ROACH. Information in these sections is not specific to any mode, but
to the physical configuration of the bank. This, for example, is the
configuration of Bank A::

    [BANKA]
    # HPC / Player host & port
    hpchost = hpc1
    player_port = 6667
    # ROACH Control:
    has_roach = true
    katcp_ip = dibasr2-1
    katcp_port = 7147
    # Data flow
    data_source_host = dibasr2-1-10-0
    data_source_port = 60000
    data_destination_host = hpc1-10
    data_destination_port = 60000
    # Synthesizer:
    synth = katcp
    synth_port = /dev/ttyS1
    synth_ref = external
    synth_ref_freq = 10000000
    synth_vco_range = 2200, 4400
    synth_rf_level = 5
    synth_options = 0,0,1,0

Keys:

  hpchost:
    The HPC that will host this bank's player
  player_port:
    The TCP port that the player will use. This is specific to this
    bank. If no bank information is provided to the player, the player
    will use the ``player_port`` in the DEFAULTS section.
  has_roach:
    A boolean flag. Some Players have roaches, some don't. Set to 'false', 'False', 'FALSE', or 0 if this player does not have a roach.
  katcp_ip:
    The 1Gb/s hostname of the roach. Used by katcp to control the roach
  katcp_port:
    The 1Gb/s TCP port used by the roach for control. Used by katcp to control the roach.
  data_source_host:
    The hostname for the 10Gb/s interface on the fpga for INCO and Spectral Line modes
  data_source_port:
    The TCP port used by the data source host.
  data_destination_host:
    The hostname for the HPC 10Gb/s interface
  data_destination_port:
    The 10Gb/s TCP port used by the HPC.
  synth:
    Location of the Valon serial control line: set to ``local`` if the Valon is connected to the HPC serial port; set to ``katcp`` if the Valon is connected to the ROACH. (DIBAS configuration is ``katcp``)
  synth_port:
    The serial port used by HPC or ROACH to control the Valon (i.e. ``/dev/ttyS1``)
  synth_ref:
    Flag, ``internal`` or ``external``. Determines whether the Valon uses internal or external frequency reference.
  synth_ref_freq:
    The frequency of the reference frequency source
  synth_vco_range:
    (see Valon documentation)
  synth_rf_level:
    The Valon output power level, in dBm. Must be one of 5, 2, -1, -4.
  synth_options:
    (see Valon documentation)

Many of these keys are specific to controlling a roach, and are therefore not needed by Players who do not have a roach. An example Bank section for such a player looks like this::

  [BANKF]
  # HPC / Player host & port
  hpchost = hpc6
  player_port = 6667
  has_roach = false
  # Data flow:
  data_source_port = 60000
  data_destination_host = hpc6-10
  data_destination_port = 60000

Any other keys may be ommitted for these Players; alternatively the key may be included, but with a value of 'N/A'. It is recommended for clarity that only the above keys be included for these Players.

MODE:
^^^^^

The various DIBAS modes are defined in the MODE sections. These sections are characterized by having the string 'MODE' embedded somewhere in the section header, i.e. 'CODD_MODE_64', 'MODE1', etc.

There are currently 3 major mode categories:

  * Spectral Line (``MODE1``, ``MODE2``, etc.)
  * Incoherent Guppi modes (``INCO_MODE_XXX`` where ``XXX`` is the number of channels used by that mode);
  * Coherent Guppi modes (``CODD_MODE_XXX`` where ``XXX`` is the number of channels used by that mode).

Unlike the bank modes there are some significant differences between the modes.  The example ``dibas.conf`` file included in this repository is extensively annotated and may be consulted for the structure of each mode.
