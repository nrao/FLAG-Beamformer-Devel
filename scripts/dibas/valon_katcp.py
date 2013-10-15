######################################################################
#
#  valon_katcp.py -- Valon over KATCP support. Duck typed after the
#  Synthesizer object in valon_synth.
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

class ValonException(Exception):
   def __init__(self, message):
        Exception.__init__(self, message)

class ValonKATCP(object):
    """
    A Valon synth class modeled after the 'valon_synth' module, but
    which communicates with a Valon installed on a ROACH via KATCP.

    ValonKATCP(r, serial_port)

    * *r:* corr.katcp_wrapper.FpgaClient object. This object is used for
      KATCP communications to the ROACH. It is expected to be
      initialized.
    * *serial_port:* A string, the device name of the serial port
        (i.e. ``/dev/ttyS0``)
    """

    def __init__(self, r, serial_port):
        """
        __init__(self, r, serial_port)

        Initializes the ValonKATCP class.
        """
        self.roach = r
        """The KATCP client object"""
        self.timeout = 5
        """The default timeout for katcp requests for this module"""
        self._set_serial_port(serial_port)


    def _get_synth_val(self, s):
        """
        _get_synth_val(self, s)

        For interchangeability with the valon_synth module this class
        assumes throughout that the parameter *s* is an integer, either
        0 for SYNTH_A or 8 for SYNTH_B. However, KATCP expects a case
        insensitive string 'synth_a' or 'synth_b' This function converts
        from 0 or 8 to 'synth_a' or 'synth_b'. It throws a *KeyError* if
        the input isn't 0 or 8.
        """
        try:
            return {0x00: 'synth_a', 0x08: 'synth_b'}[s]
        except KeyError:
            raise ValonException("The 'synth' parameter was given as %s. It must be an"
                                 " integer either 0x00 (SYNTH_A) or 0x08 (SYNTH_B)" % str(s))

    def _set_serial_port(self, port):
        """
        _set_serial_port(self, port)

        * *port:* a string denoting the serial device, i.e. ``/dev/ttyS0``
          Sets the serial port on the ROACH that the Valon is connected to.
        """
#        reply, informs = self.roach._request("valon-new-port", self.timeout, port)
        reply, informs = self.roach._request("valon-new-port", port)
        if reply.arguments[0] != 'ok':
            raise ValonException("Unable to set serial port %s" % port)

    def flash(self):
        """
        flash(self)

        Writes frequencies to valon non-volatile memory.
        """
#        reply, informs = self.roach._request("valon-flash", self.timeout)
        reply, informs = self.roach._request("valon-flash")

    def get_frequency(self, synth):
        """
        get_frequency(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns the current output frequency for the specified synthesizer.
        """
        # reply, informs = self.roach._request("valon-get-frequency", self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request("valon-get-frequency",
                                             self._get_synth_val(synth))

        if reply.arguments[0] != 'ok':
            raise ValonException("Unable to retrieve frequency for %s", self._get_synth_val(synth))
        else:
            return float(reply.arguments[2])

    def get_label(self, synth):
        """
        get_label(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns the currently set label for 'synth'.
        """
        # reply, informs = self.roach._request("valon-get-label", self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request("valon-get-label",
                                             self._get_synth_val(synth))

        return reply.arguments[1].rstrip()

    def get_options(self, synth):
        """
        get_options(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns the option flags/values for the specified
        synthesizer. They are returned as a tuple of 4 elements:

        0. Ref frequency doubler flag. When set, the reference frequency doubler is active.
        1. Ref frequency halver flag: When set, the reference frequency halver is active.
        2. Integer value, the reference frequency divider value.
        3. 'Low spur' mode flag. When not set, the synth is in 'low noise' mode.
        """
        # reply, informs = self.roach._request("valon-get-options", self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request("valon-get-options",
                                             self._get_synth_val(synth))
        return (int(reply.arguments[3]), int(reply.arguments[4]), int(reply.arguments[5]), int(reply.arguments[2]))

    def get_phase_lock(self, synth):
        """
        get_phase_lock(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns *True* if the specified synthesizer is phase locked to
        its reference, *False* if not.
        """
        # reply, informs = self.roach._request("valon-get-phase-lock", self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request("valon-get-phase-lock",
                                             self._get_synth_val(synth))

        return int(reply.arguments[1]) == 1

    def get_ref_select(self):
        """
        get_ref_select(self)

        Returns *1* if external reference frequency is selected, *0* if
        internal reference is selected.
        """
        # reply, informs = self.roach._request("valon-get-ref-select", self.timeout)
        reply, informs = self.roach._request("valon-get-ref-select")

        return int(reply.arguments[1])

    def get_reference(self):
        """
        get_reference(self)

        Returns the set reference frequency value. This is not the
        actual reference frequency, but the value that has been sent to
        the Valon as the current reference frequency.
        """
        # reply, informs = self.roach._request('valon-get-reference', self.timeout)
        reply, informs = self.roach._request('valon-get-reference')
        return int(reply.arguments[1])

    def get_rf_level(self, synth):
        """
        get_rf_level(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns the output RF level setting in dBm for the specified
        synthesizer.
        """
        # reply, informs = self.roach._request('valon-get-rf-level', self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request('valon-get-rf-level',
                                             self._get_synth_val(synth))
        return int(reply.arguments[2])

    def get_vco_range(self, synth):
        """
        get_vco_range(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        Returns a tuple of the VCO range, (min, max)
        """
        # reply, informs = self.roach._request('valon-get-vco-range', self.timeout,
        #                                      self._get_synth_val(synth))
        reply, informs = self.roach._request('valon-get-vco-range',
                                             self._get_synth_val(synth))
        return (int(reply.arguments[2]), int(reply.arguments[3]))

    def set_frequency(self, synth, frequency, chan_spacing = 10.0):
        """
        set_frequency(self, synth)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        * *frequency:* A floating point value, the frequency in MHz.

        * *chan_spacing:* A floating point value, the resolution of the
          setting, in Hz. This defaults to 10 Hz.

        Sets the output frequency of the specified synthesizer. Returns
        *True* on success.
        """
        # reply, informs = self.roach._request('valon-set-frequency', self.timeout,
        #                                      self._get_synth_val(synth), frequency, chan_spacing)
        reply, informs = self.roach._request('valon-set-frequency',
                                             self._get_synth_val(synth), frequency, chan_spacing)
        return reply.arguments[0] == 'ok'

    def set_label(self, synth, new_label):
        """
        set_label(self, synth, new_label)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        * *new_label:* A string; only the first 16 characters will be used.

        Gives a custom label to the specified synthesizer.
        """
        # reply, informs = self.roach._request('valon-set-label', self.timeout,
        #                                      self._get_synth_val(synth), new_label)
        reply, informs = self.roach._request('valon-set-label',
                                             self._get_synth_val(synth), new_label)
        return reply.arguments[0] == 'ok'

    def set_options(self, synth, double=0, half=0, r=1, low_spur=0):
        """
        set_options(self, synth, double=0, half=0, r=1, low_spur=0)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        * *double:* Set to 1 to double the reference frequency, default = 0

        * *half:* Set to 1 to halve the reference frequency, default = 0

        * *r:* Reference frequency multiplier, default = 1

        * *low_spur:* Set to 1 to use 'low spur' mode, clear to use 'low
          noise' mode. Default = 0

        Sets the synthesizer options.
        """
        # reply, informs = self.roach._request('valon-set-options', self.timeout,
        #                                      self._get_synth_val(synth),
        #                                      low_spur, double, half, r)
        reply, informs = self.roach._request('valon-set-options',
                                             self._get_synth_val(synth),
                                             low_spur, double, half, r)
        return reply.arguments[0] == 'ok'

    def set_ref_select(self, external):
        """
        set_ref_select(self, external)

        * *external:* External reference flag, an integer number. Set to 1
        for external ref, 0 for internal ref.

        Sets the reference frequency source to external or internal.
        """
        reply, informs = self.roach._request('valon-set-ref-select', 
                                             external)
        return reply.arguments[0] == 'ok'

    def set_reference(self, ref_freq):
        """
        set_reference(self, ref_freq)

        * *ref_freq:* the reference frequency being used, in Hz.

        Tells the Valon synthesizer module of the reference frequency
        being used.
        """
        reply, informs = self.roach._request('valon-set-reference', 
                                             ref_freq)
        return reply.arguments[0] == 'ok'

    def set_rf_level(self, synth, rf_level):
        """
        set_rf_level(self, synth, rf_level)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        * *rf_level:* The new level, in dBm. This must be one of -4, -1,
          2, and 5, or the function will return *False* and not set the
          level.

        Sets the output RF level in dBm for the specified synthesizer.
        """
        allowed_levels = (-4, -1, 2, 5)

        if rf_level in allowed_levels:
            reply, informs = self.roach._request('valon-set-rf-level', 
                                                 self._get_synth_val(synth), rf_level)
            return reply.arguments[0] == 'ok'

        return False

    def set_vco_range(self, synth, low, high):
        """
        set_vco_range(self, synth, low, high)

        * *synth:* An integer, 0x00 for SYNTH_A, and 0x08 for SYNTH_B

        * *low:* Minimum frequency the VCO is capable of producing.

        * *high:* Maximum frequency the VCO is capable of producing.

        Sets the specified synthesizer's VCO range.
        """
        reply, informs = self.roach._request('valon-set-vco-range', 
                                             self._get_synth_val(synth), low, high)
        return reply.arguments[0] == 'ok'
