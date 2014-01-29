######################################################################
#
#  vegas_ssg.py -- switching signals LUT generator for VEGAS bof files.
#
#  Copyright (C)  Associated Universities, Inc. Washington DC, USA.
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


import struct
import math

class SwitchingSignals(object):
    """
    A class that describes a full switching signals cycle in terms that
    the VEGAS BOFs can understand.

    The class requires two bof-specific values: The clock (Valon)
    frequency ('frequency'), and the number of channels ('nchan'). From
    these it computes the seconds value of one granule, the FPGA time
    unit.

    Signals are built by providing phases in the correct sequence:
    phase1, phase2, etc., with duration of phase in seconds and which
    signal values are high for that phase, as booleans:

    ss.add_phase(0.01, cal = True, bl = True) # cal & blanking high for 10 mS
    ss.add_phase(0.09, cal = True)            # cal high for 90 mS
    ...
    etc.

    The class keeps track of the number of phases and the total duration
    of the switching cycle. In addition it will generate the proper SSG
    LUT words that represent this cycle, and also provide the packed
    binary representation that can be given directly to KATCP to set the
    SSG LUT.
    """

    class SwitchingPhase(object):
        """
        A class that describes one phase of the switching signal:
        duration, and which signals are set (high)
        """

        def __init__(self, dur = 0, asr = 0, sr2 = 0, sr1 = 0, cal = 0, bl = 0):
            self._duration = dur
            self._adv_sig_ref = 0
            self._sig_ref_2 = 0
            self._sig_ref_1 = 0
            # NOTE! CAL on the GBT is inverse logic: 0 = CAL, 1 = No
            # CAL. This logic must be changed, as well as the
            # implementation of the 'cal()' & 'set_cal()' member
            # functions, if this is run on a system where this does not
            # apply. The FITS writer also must take this into account,
            # because the cal signal will not be in the expected accum
            # id.
            self._cal = 0
            self._blanking = 0;
            # This is an mask which is XOR'ed to optionally invert
            # the sense of the output switching signals.
            self.polarity_mask = 0x6

            if asr: self.set_adv_sig_ref()
            if sr2: self.set_sig_ref_2()
            if sr1: self.set_sig_ref_1()
            if cal: self.set_cal()
            if bl: self.set_blanking()

        def __repr__(self):
            return "%i--%i%i%i%i%i" \
                % \
                (self._duration,
                 self.adv_sig_ref(),
                 self.sig_ref_2(),
                 self.sig_ref_1(),
                 self.cal(),
                 self.blanking())

        def nb_state(self):
            """
            Returns signals values minus blanking.
            """
            return sum([self._cal, self._sig_ref_1, self._sig_ref_2, self._adv_sig_ref])

        def set_duration(self, duration):
            self._duration = int(duration)

        def duration(self):
            return self._duration

        def set_blanking(self):
            self._blanking = 1

        def blanking(self):
            return 1 if self._blanking != 0 else 0

        def set_cal(self):
            # See comment in self.__init__ for CAL logic
            self._cal = 2

        def cal(self):
            # See comment in self.__init__ for CAL logic
            return 1 if self._cal != 0 else 0

        def set_sig_ref_1(self):
            self._sig_ref_1 = 4

        def sig_ref_1(self):
            return 1 if self._sig_ref_1 != 0 else 0

        def set_sig_ref_2(self):
            self._sig_ref_2 = 8

        def sig_ref_2(self):
            return 1 if self._sig_ref_2 != 0 else 0

        def set_adv_sig_ref(self):
            self._adv_sig_ref = 16

        def adv_sig_ref(self):
            return 1 if self._adv_sig_ref != 0 else 0

        def phase_word(self):
            return sum((self._duration << 5,
                        self._adv_sig_ref,
                        self._sig_ref_2,
                        self._sig_ref_1,
                        self._cal,
                        self._blanking)) ^ self.polarity_mask

    def __init__(self):

        self._spec_tick = 0.0
        self._hwexposr = 0.0
        self.phases = [SwitchingSignals.SwitchingPhase()]

    def __repr__(self):
        return "%s" % self.phases

    def set_spec_tick(self, spec_tick):
        self._spec_tick = spec_tick

    def set_hwexposr(self, hwexposr):
        self._hwexposr = hwexposr

    def clear_phases(self):
        """
        Clears the switching signal of phases.
        """
        self.phases = []

    def number_phases(self):
        """
        Returns the number of phases.
        """
        return len(self.phases)

    def phase_words(self):
        """
        Returns a vector words containing phase information encoded for
        use by the bof.
        """
        return [p.phase_word() for p in self.phases]

    def total_duration(self):
        """
        Returns the total duration of all phases, in seconds.
        """
        granules = sum([p.duration() for p in self.phases])
        # if no phases are specified or if there is only a single phase,
        # don't return a zero sum. In these cases the duration doesn't mean anything.
        if len(self.phases) > 1:
            return granules * self._spec_tick
        else:
            return 1

    def total_duration_granules(self):
        """
        Returns the total duration of all phases, in granules
        """
        granules = sum([p.duration() for p in self.phases])
        # if no phases are specified or if there is only a single phase,
        # don't return a zero sum. In these cases the duration doesn't mean anything.
        return granules

    def add_phase(self, dur, bl = False, cal = False, sr1 = False, sr2 = False, asr = False):
        """Adds one switching phase to the cycle.

        dur: Duration in spec_ticks. The different modes compute
        blanking times in spec_tick units differently from one-another,
        so this module accepts durations in spec_ticks to avoid having
        to know about these differences.

        What follows are the different switching signals. For each of
        them, if it is true, it is high for this phase. All default to
        False (therefore low) (sig/ref = True for ref).

        bl: Blanking.
        cal: Cal.
        sr1: Sig/Ref 1.
        sr2: Sig/Ref 2.
        asr: Advanced Sig/Ref

        The following example describes a switching signal of 8 phases
        total 400 mS long, involving Cal, Sig/Ref 1 and Blanking:

        ss = SwitchingSignals()
        ss.add_phase(0.01, bl = True, cal = True, sr1 = True)
        ss.add_phase(0.09, cal = True, sr1 = True)
        ss.add_phase(0.01, bl = True, cal = True)
        ss.add_phase(0.09, cal = True)
        ss.add_phase(0.01, bl = True, sr1 = True)
        ss.add_phase(0.09, sr1 = True)
        ss.add_phase(0.01, bl = True)
        ss.add_phase(0.09)

        """
        ph = SwitchingSignals.SwitchingPhase()

        ph.set_duration(dur)

        if bl: ph.set_blanking()
        if cal: ph.set_cal()
        if sr1: ph.set_sig_ref_1()
        if sr2: ph.set_sig_ref_2()
        if asr: ph.set_adv_sig_ref()

        self.phases.append(ph)

    def packed_lut_string(self):
        return struct.pack('>' + str(self.number_phases()) + 'I', *self.phase_words())

    def gbt_phase_starts(self):
        """
        Returns the fractions of the period each GBT phase starts, the
        state of the cal & sig/ref 1 signals on those starts, and the
        length of blanking for those starts. The function returns a
        dictionary containing all those values:

        key:          value:
        period        A double, the ss cycle period, in seconds
        phase-starts  A list of doubles, the phase starts, as fractions, assuming 1 cycle = 1
        cal           A list of ints (1 set, 0 clear), the cal state on each phase
        sig/ref       A list of ints (1 ref, 0 sig), the sig/ref state of each phase
        blanking      A list of doubles, the blank time at the start of each phase, in seconds.
        """
        times = [0.0]
        last = self.phases[0]
        total_time = last.duration() * self._spec_tick
        cal = [last.cal()]
        sr1 = [last.sig_ref_1()]
        blanking = [last.duration() * self._spec_tick if last.blanking() else 0.0]

        for i in self.phases[1:]:

            if i.nb_state() != last.nb_state(): # new GBT phase!
                times.append(total_time)
                cal.append(i.cal())
                sr1.append(i.sig_ref_1())
                blanking.append(i.duration() * self._spec_tick if i.blanking() else 0.0)
                last = i

            total_time += i.duration() * self._spec_tick

        return {"period": self.total_duration(),
                "phase-starts": [t / self.total_duration() for t in times],
                "cal": cal,
                "sig/ref": sr1,
                "blanking": blanking}
