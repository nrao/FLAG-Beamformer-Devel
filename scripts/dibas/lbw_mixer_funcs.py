######################################################################
#
#  L8 Mixer calculation utilities for DIBAS.
#
#  Copyright (C) 2014 Associated Universities, Inc. Washington DC, USA.
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

# L8_LBW1 / L8_LBW8 parameter calculations
# algorithms taken from mixercic_funcs.py

import math
import struct

class LBWMixerCalcs(object):
    """
    L8_LBW1 / L8_LBW8 parameter calculations helper
    This class creates and returns a dictionary with register name
    value pairs. No direct FPGA access is performed.
    """

    class SinCosUnion(object):
        def __init__(self, sin = 0.0, cos = 0.0):
            self.cos = cos
            self.sin = sin

        def __repr__(self):
            return "sin: %s, cos: %s" % (str(self.sin), str(self.cos))

        def as_int(self):
            return (int(self.cos) << 16) + int(self.sin)

    def __init__(self, valon_frequency):
        self.valon_frequency = 0
        self.set_valon_frequency(valon_frequency)
        self.clear_results()

        self.log_bramlength = 10 # log2 bram register length
        self.interleave   =   16 # interleave factor for lo table
        self.sample_f     =   self.valon_frequency * 2 # from Anish's document
        self.MAX_INT      =   32767

    def set_valon_frequency(self, valon_frequency):
        """
        Update the mixer calculation of a change in valon frequency.
        This updates the effective sampling frequency.
        """
        # Convert a value in MHz to Hz
        if valon_frequency < 5000:
            valon_frequency = valon_frequency * 1e6
        else:
            valon_frequency = valon_frequency

        if  self.valon_frequency != valon_frequency:
            self.valon_frequency = valon_frequency
            self.clear_results() # previous results now invalid
        self.sample_f = valon_frequency * 2

    def wave_generator(self, normalized_frequency, interleave=16, log_bram_len=10):
        """
        Parameters:
            normalized_frequency:
                represents the factor to apply when generating the sin/cos tables
	    interleave:
                The number of registers to interleave the data (always 16)
        log_bram_size:
                The log2 of the length of the bram tables (always 10)
        Return values:
            resutls: an int array of 32-bit integers.
            siniw: an int array of 16-bit integers
            cosiw: an int array of 16-bit integers
        Notes:
	    Assuming the data type is FIX_16_15 in BRAM
        """
        bram_len = 1 << log_bram_len
        nsamples = interleave * bram_len
        scu = LBWMixerCalcs.SinCosUnion()
        non_interleaved = list()
        # we're going to return this
        interleaved = [scu] * nsamples

        if abs(normalized_frequency) < 1e-3:
            for i in range(0, nsamples):
                interleaved.append(SinCosUnion(self.MAX_INT, self.MAX_INT))

            return interleaved

        npts = float(nsamples)
        t = 0.0

        for i in range(0, nsamples):
            sinf = math.sin(2 * math.pi * normalized_frequency * t / npts) * self.MAX_INT
            cosf = math.cos(2 * math.pi * normalized_frequency * t / npts) * self.MAX_INT
            non_interleaved.append(LBWMixerCalcs.SinCosUnion(sinf, cosf))
            t += 1.0

        for r in range(bram_len):
            for i in range(interleave):
                scu = LBWMixerCalcs.SinCosUnion(non_interleaved[interleave * r + i].sin,
                                                non_interleaved[interleave * r + i].cos)
                interleaved[r + i * bram_len] = scu

        return interleaved


    def get_lo_results(self):
        """
        Returns a dictionary with keys which match register names and values which
        can be written directly to the respective register.
        """
        return self.lo_regs


    def clear_results(self):
        self.lo_regs = {}


    def fill_mixer_bram(self, subband_num, bram_block):
        """
        subband_num: The subband number in the range 0..7
        bram_block: A list of 16 binary strings created by wave_generator()
        """

        self.lo_regs['s' + str(subband_num) + '_mixer_cnt'] = 1022
        # setup the sixteen registers for this subband
        for i in range(self.interleave):
           self.lo_regs['s' + str(subband_num) +'_lo_'+str(i)+'_lo_ram'] = bram_block[i]


    def calc_lo_frequency(self, lo_freq, sampling_f, interleave=16, log_bram_len = 10):
        """
        Updated version from Glenn Jones
        Note: this version and the iterative version do not return the same results.
        Glen verified that the calculation does not require the increased resolution
        of the iterative version of the calculation.
        Returns the normalized frequency and actual achieved frequency
        """
        wave_len = interleave * (1 << log_bram_len)
        frac_lo = round((lo_freq / sampling_f) * wave_len)
        actual_lo = sampling_f * frac_lo / float(wave_len)
        return frac_lo, actual_lo


    def wave_to_string(self, wave):
        """Converts the calculated interleaved values into a list of 16 packed
        strings for the roach.

        """
        packed_vals = []
        for i in range(0, 16):
            start_idx = i * 1024;
            end_idx = start_idx + 1024
            my_vals = [i.as_int() for i in wave[start_idx:end_idx]]
            packed_vals.append(struct.pack('>%ii' % len(my_vals), *my_vals))

        return packed_vals


    def lo_setup(self, lo_f, subband_num):
        """
        Calculate the lo frequency tables for the bram's given the sub-band number (0..7) and the
        base-band requested center frequency.
        The binary data strings to send to the LO registers is returned in the self.regs dictionary.
        See get_lo_results()
        """

        frac_lo, actual_f = self.calc_lo_frequency(lo_f, self.sample_f)
        interleaved_results = self.wave_generator(frac_lo)
        bram_block_list = self.wave_to_string(interleaved_results)
        self.fill_mixer_bram(subband_num, bram_block_list)
        return frac_lo, actual_f
