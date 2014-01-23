######################################################################
#
#  dibas_tests.py -- dibas unit tests
#
#  Preparing for test:
#
#    1 - cd to the dibas project home, vegas_devel/scripts/dibas
#
#    2 - copy the appropriate 'dibas.conf', either 'dibas.conf.gb' or
#        'dibas.conf.shao', to ./etc/config/dibas.conf
#
#    3 - source the dibas.bash of the installation to test against. For
#        example, 'source /home/dibas/dibas.bash', or 'source
#        /opt/dibas/dibas.bash', etc. This loads the correct python
#        environment for these tests.
#
#    4 - from the project home run nosetests
#
#  Though these tests use the sourced dibas environment, they are
#  hard-coded to use the 'dibas.conf' file installed in step 2
#  above. This is because code development may require the updated
#  'dibas.conf' that hasn't yet been installed.
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
import os
import math

from VegasBackend import VegasBackend
from VegasLBWBackend import VegasLBWBackend
# from VegasL8LBWBackend import VegasL8LBWBackend
from Backend import SWbits
from GuppiBackend import GuppiBackend
from GuppiCODDBackend import GuppiCODDBackend
from ConfigData import BankData, ModeData
from nose.tools import assert_equal, assert_almost_equal
from nose.tools import assert_equals,assert_almost_equals

def skipped(func):
    from nose.plugins.skip import SkipTest
    def _():
        raise SkipTest("Test %s is skipped" % func.__name__)
    _.__name__ = func.__name__
    return _

# use the dibas.conf in this project directory. The one in the local
# dibas installation may not be compatible with development.
dibas_dir = "."
# whatever is in use by local dibas installation
dibas_data = os.getenv('DIBAS_DATA')

# def setup_dibas_tests():
#     dibas_dir = os.getenv("DIBAS_DIR")

def compute_phases(phases, hwexposr, spec_tick):
    """Takes a bunch of phase lengths, and computes their total duration in
    seconds, after converting each to number of spec_ticks and back, as
    done in the vegas_ssg.py module for non-blanking phases.

    """
    # make list of durations in spec_ticks, in integer multiples of hwexposr
    l1 = map(lambda x: int(math.ceil(x / hwexposr) * hwexposr / spec_tick), phases)
    print l1
    # convert back to time
    l2 = map(lambda x: spec_tick * x, l1)
    print l2
    return l1, l2

def compute_hbw_blanking(phases, hwexposr, spec_tick):
    """Computes the blanking phases for the HBW modes. These are the same
    as the normal switching phases, so just calls 'compute_phases()'

    """
    return compute_phases(phases, hwexposr, spec_tick)

def compute_lbw_blanking(phases, chan_bw, spec_tick):
    """Computes the blanking phases for the LBW modes. These are different
    than the non-blanking phases for modes 4-29, the LBW modes, as per
    memo 'ssgprogram.pdf' by Anish Roshi.

    """
    l1 = map(lambda x: int(math.ceil(x * chan_bw) * 1 / (spec_tick * chan_bw)), phases)
    l2 = map(lambda x: spec_tick * x, l1)
    return l1, l2


def compute_exposure(ssg_phases, blanking_phases):
    return reduce(lambda x, y: x+y, ssg_phases) + reduce(lambda x, y: x+y, blanking_phases)

def compute_expoclks(ssg_spec_ticks, blanking_spec_ticks, fpga_clks_per_spec_tick):
    total_spec_ticks = reduce(lambda x, y: x+y, ssg_spec_ticks) \
                       + reduce(lambda x, y: x+y, blanking_spec_ticks)
    return total_spec_ticks * fpga_clks_per_spec_tick

def dibas_vals(mode, expo):
    """Helper function, computes values for HBW modes to be used for test
    purposes. Assumes a switching signal with 4 phases and 4 blankings,
    and that the blanking is always 2 mS.

    * *mode:* The mode, a string; i.e. 'MODE1'
    * *expo:* The requested exposure

    Returns:

    A tuple:

    (frequency, efsampfr, chan_bw, bw_mode, fpga_clk, exposure, expoclks)

    """
    modenum = int(mode[4])
    config = ConfigParser.ConfigParser()
    config.readfp(open(dibas_dir + "/etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, mode)
    freq = m.frequency * 1e6
    bl_init_val = 0.002
    ssg_init = [expo / 4 - bl_init_val] * 4
    bl_init = [bl_init_val] * 4

    if modenum < 4:
        efsampfr = freq * 2
        bw_mode = "high"
        obs_mode = "HBW"
        spec_tick = m.nchan / freq
    elif modenum > 3 and modenum < 10:
        efsampfr = freq / 4
        bw_mode = "low"
        obs_mode = "LBW"
        spec_tick = 1024.0 / freq
    elif modenum > 9 and modenum < 29:
        efsampfr = freq / 32
        bw_mode = "low"
        obs_mode = "LBW"
        spec_tick = 1024.0 / freq

    chan_bw = efsampfr / (m.nchan * 2)
    fpga_clk = freq / 8
    fpart, ipart = math.modf(m.hwexposr / spec_tick)

    if fpart >= 0.5:
        ipart = int(ipart) + 1

    hwexposr = spec_tick * ipart

    ssg_expoclks, ssg_times = compute_phases(ssg_init, hwexposr, spec_tick)

    if modenum < 4:
        ssg_bl_ticks, ssg_bl_times = compute_hbw_blanking(bl_init, hwexposr, spec_tick)
        exposure = compute_exposure(ssg_times, ssg_bl_times)
        expoclks = compute_expoclks(ssg_expoclks, ssg_bl_ticks, m.nchan / 8)
    else:
        ssg_bl_ticks, ssg_bl_times = compute_lbw_blanking(bl_init, chan_bw, spec_tick)
        exposure = compute_exposure(ssg_times, ssg_bl_times)
        expoclks = compute_expoclks(ssg_expoclks, ssg_bl_ticks, 128)

    return (b, m, freq, exposure, expoclks, ssg_bl_times[0], spec_tick, efsampfr, \
            hwexposr, chan_bw, bw_mode, obs_mode, fpga_clk)


def check_hbw_mode(mode, expo):
    """
    Runs tests for specified mode 'mode', at requested exposure 'expo'.
    """

    params = dibas_vals(mode, expo)
    b, m, frequency, exposure, expoclks, blanktm, spec_tick, efsampfr, \
        hwexposr, chan_bw, bw_mode, obs_mode, fpga_clk = params

    be = VegasBackend(b, m, None, None, None, unit_test = True)

    be.setPolarization('SELF')
    be.setIntegrationTime(expo)

    be.clear_switching_states()
    ssg_duration = expo
    print "ssg_duration =", ssg_duration
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    # call dependency methods and update shared memory
    be.prepare()
    print "Status memory:", be.mock_status_mem
    print "Roach (int) registers:", be.mock_roach_registers
    be.show_switching_setup()
    DEFAULT_VALUE = "unspecified"

    assert_almost_equal(float(be.get_status("_SBLK_01")), blanktm, 5)
    assert_almost_equal(float(be.get_status("_SBLK_02")), blanktm, 5)
    assert_almost_equal(float(be.get_status("_SBLK_03")), blanktm, 5)
    assert_almost_equal(float(be.get_status("_SBLK_04")), blanktm, 5)

    assert_equal(be.get_status("_SCAL_01"), '1')
    assert_equal(be.get_status("_SCAL_02"), '0')
    assert_equal(be.get_status("_SCAL_03"), '1')
    assert_equal(be.get_status("_SCAL_04"), '0')

    assert_almost_equal(float(be.get_status("_SPHS_01")), 0,)
    assert_almost_equal(float(be.get_status("_SPHS_02")), 0.25)
    assert_almost_equal(float(be.get_status("_SPHS_03")), 0.5)
    assert_almost_equal(float(be.get_status("_SPHS_04")), 0.75)

    assert_equal(be.get_status("_SSRF_01"), '0')
    assert_equal(be.get_status("_SSRF_02"), '0')
    assert_equal(be.get_status("_SSRF_03"), '1')
    assert_equal(be.get_status("_SSRF_04"), '1')

    assert_equal(be.get_status('BANKNAM') , 'BANKA')
    assert_equal(be.get_status('MODENUM') , mode)
    assert_equal(be.get_status("BW_MODE") , bw_mode)

    assert_almost_equal(float(be.get_status("CHAN_BW")), chan_bw)
    assert_almost_equal(float(be.get_status("EFSAMPFR")), efsampfr)
    assert_almost_equal(float(be.get_status("EXPOSURE")), exposure)
    assert_almost_equal(float(be.get_status("FPGACLK")), fpga_clk)

    assert_equal(be.get_status("OBSNCHAN"), str(m.nchan))
    assert_equal(be.get_status("OBS_MODE"), obs_mode)
    assert_equal(be.get_status("PKTFMT")  , "SPEAD")
    assert_equal(be.get_status("NCHAN")   , str(m.nchan))
    assert_equal(be.get_status("NPOL")    , '2')
    assert_equal(be.get_status("NSUBBAND"), '1') # mode 1 uses just one.
    assert_equal(be.get_status("SUB0FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB1FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB2FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB3FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB4FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB5FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB6FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB7FREQ"), str(efsampfr / 4))

    assert_equal(be.get_status("BASE_BW"), str(m.filter_bw)) # from config file
    assert_equal(be.get_status("NOISESRC"), 'OFF')
    assert_equal(be.get_status("NUMPHASE"), '4')
    assert_almost_equal(float(be.get_status("SWPERIOD")), exposure)
    assert_equal(be.get_status("SWMASTER"), "VEGAS")
    assert_equal(be.get_status("POLARIZE"), "SELF")
    assert_equal(be.get_status("CRPIX1"), str(m.nchan / 2 + 1))
    assert_almost_equal(float(be.get_status("SWPERINT")), 1.0)
    assert_equal(be.get_status("NMSTOKES"), "2")

    assert_equal(be.get_status("CAL_DCYC"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_FREQ"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_MODE"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_PHS") , DEFAULT_VALUE)

    assert_equal(be.get_status("DATADIR") , dibas_data)
    assert_equal(be.get_status("DATAPORT"), '60000')

    assert_equal(be.get_status("FILENUM") , DEFAULT_VALUE)
    assert_almost_equal(float(be.get_status("HWEXPOSR")), hwexposr)
    assert_equal(be.get_status("M_STTMJD"), '0')
    assert_equal(be.get_status("M_STTOFF"), '0')
    assert_equal(be.get_status("NBITS")   , '8')
    assert_equal(be.get_status("NBITSADC"), '8')

    assert_equal(be.get_status("NPKT")    , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSBW")   , str(frequency))

    assert_equal(be.get_status("OBSFREQ") , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSERVER"), DEFAULT_VALUE)
    assert_equal(be.get_status("OBSID")   , DEFAULT_VALUE)

    assert_equal(be.get_status("SWVER")   , DEFAULT_VALUE)


def check_l1lbw1_mode(mode, expo):
    """A VegasLBWBackend setup test case.

    """

    params = dibas_vals(mode, expo)
    b, m, frequency, exposure, expoclks, blanktm, spec_tick, efsampfr, \
        hwexposr, chan_bw, bw_mode, obs_mode, fpga_clk = params


    be = VegasLBWBackend(b, m, None, None, None, unit_test = True)
    nchan = m.nchan
    frequency_resolution = abs(chan_bw)

    be.setPolarization('SELF')
    be.setProjectId('JUNK')
    be.setScanLength(60.0)
    be.setIntegrationTime(expo)

    be.clear_switching_states()
    ssg_duration = expo
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    # call dependency methods and update shared memory
    be.prepare()
#    print "Status memory:", be.mock_status_mem
#    print "Roach (int) registers:", be.mock_roach_registers
    be.show_switching_setup()
    DEFAULT_VALUE = "unspecified"

    assert_equal(be.get_status('PROJID'), 'JUNK')
    assert_equal(be.get_status('SCANLEN'), '60.0')
    assert_almost_equal(float(be.get_status("_SBLK_01")), blanktm, 6)
    assert_almost_equal(float(be.get_status("_SBLK_02")), blanktm, 6)
    assert_almost_equal(float(be.get_status("_SBLK_03")), blanktm, 6)
    assert_almost_equal(float(be.get_status("_SBLK_04")), blanktm, 6)

    assert_equal(be.get_status("_SCAL_01"), '1')
    assert_equal(be.get_status("_SCAL_02"), '0')
    assert_equal(be.get_status("_SCAL_03"), '1')
    assert_equal(be.get_status("_SCAL_04"), '0')

    assert_almost_equal(float(be.get_status("_SPHS_01")), 0,)
    assert_almost_equal(float(be.get_status("_SPHS_02")), 0.25)
    assert_almost_equal(float(be.get_status("_SPHS_03")), 0.5)
    assert_almost_equal(float(be.get_status("_SPHS_04")), 0.75)

    assert_equal(be.get_status("_SSRF_01"), '0')
    assert_equal(be.get_status("_SSRF_02"), '0')
    assert_equal(be.get_status("_SSRF_03"), '1')
    assert_equal(be.get_status("_SSRF_04"), '1')

    assert_equal(be.get_status('BANKNAM') , 'BANKA')
    assert_equal(be.get_status('MODENUM') , mode)
    assert_equal(be.get_status("BW_MODE") , "low")

    assert_almost_equal(float(be.get_status("CHAN_BW")), chan_bw)
    assert_almost_equal(float(be.get_status("EFSAMPFR")), efsampfr)
    assert_almost_equal(float(be.get_status("EXPOSURE")), exposure, 7)
    assert_equal(int(be.get_status('EXPOCLKS')), expoclks)
    assert_almost_equal(float(be.get_status("FPGACLK")), fpga_clk)

    assert_equal(be.get_status("OBSNCHAN"), str(nchan))
    assert_equal(be.get_status("OBS_MODE"), "LBW")
    assert_equal(be.get_status("PKTFMT")  , "SPEAD")
    assert_equal(be.get_status("NCHAN")   , str(nchan))
    assert_equal(be.get_status("NPOL")    , '2')
    assert_equal(be.get_status("NSUBBAND"), '1')
    assert_equal(be.get_status("SUB0FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB1FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB2FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB3FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB4FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB5FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB6FREQ"), str(efsampfr / 4))
    assert_equal(be.get_status("SUB7FREQ"), str(efsampfr / 4))

    assert_equal(be.get_status("BASE_BW"), str(m.filter_bw)) # from config file
    assert_equal(be.get_status("NOISESRC"), 'OFF')
    assert_equal(be.get_status("NUMPHASE"), '4')
    assert_almost_equal(float(be.get_status("SWPERIOD")), exposure, 5)
    assert_equal(be.get_status("SWMASTER"), "VEGAS")
    assert_equal(be.get_status("POLARIZE"), "SELF")
    assert_equal(be.get_status("CRPIX1"), str(nchan / 2 + 1))
    assert_almost_equal(float(be.get_status("SWPERINT")), 0.1 / ssg_duration)
    assert_equal(be.get_status("NMSTOKES"), "2")

    assert_equal(be.get_status("CAL_DCYC"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_FREQ"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_MODE"), DEFAULT_VALUE)
    assert_equal(be.get_status("CAL_PHS") , DEFAULT_VALUE)

    assert_equal(be.get_status("DATADIR") , dibas_data)
    assert_equal(be.get_status("DATAPORT"), '60000')

    assert_equal(be.get_status("EFSAMPFR"), str(efsampfr))
    assert_equal(be.get_status("FILENUM") , DEFAULT_VALUE)
    assert_almost_equal(float(be.get_status("FPGACLK")), frequency / 8)
    assert_almost_equal(float(be.get_status("HWEXPOSR")), hwexposr, 5)
    assert_equal(be.get_status("M_STTMJD"), '0')
    assert_equal(be.get_status("M_STTOFF"), '0')
    assert_equal(be.get_status("NBITS")   , '8')
    assert_equal(be.get_status("NBITSADC"), '8')

    assert_equal(be.get_status("NPKT")    , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSBW")   , str(m.nchan * chan_bw))

    assert_equal(be.get_status("OBSFREQ") , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSERVER"), DEFAULT_VALUE)
    assert_equal(be.get_status("OBSID")   , DEFAULT_VALUE)

    assert_equal(be.get_status("SWVER")   , DEFAULT_VALUE)

def check_l8lbw1_mode(mode, expo):
    """
    Single L8LBW1 subband test (Mode 10).
    """
    pass
    # params = dibas_vals(mode, expo)
    # b, m, frequency, exposure, expoclks, blanktm, spec_tick, efsampfr, \
    #     hwexposr, chan_bw, bw_mode, obs_mode, fpga_clk = params

    # be = VegasL8LBWBackend(b, m, None, None, None, unit_test = True)

    # be.setPolarization('CROSS')
    # be.setScanLength(60.0)
    # be.setIntegrationTime(expo)
    # be.set_param("subband_freq", [ 300000000.0000 ])

    # be.clear_switching_states()
    # ssg_duration = expo
    # be.set_gbt_ss(ssg_duration,
    #               ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
    #                (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
    #                (0.5, SWbits.REF, SWbits.CALON, 0.002),
    #                (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
    #               )

    # # call dependency methods and update shared memory
    # be.prepare()
    # regs = be.mock_roach_registers
    # shm  = be.mock_status_mem
    # shmkeys = shm.keys()
    # shmkeys.sort()

    # print "Status memory:"
    # for i in shmkeys:
    #     print "%s = %s" % (i, shm[i])

    # assert_equals(int(shm['NCHAN']), m.nchan)
    # assert_equals(int(shm['NSUBBAND']), 1)
    # assert_equals(float(shm['SUB0FREQ']), 300001144.409)
    # #print be.actual_subband_freq

    # for k in regs.keys():
    #     if isinstance(regs[k], str) and len(regs[k]) > 70:
    #         print "reg[%s]=" % (k), regs[k][0]
    #     else:
    #         print "reg[%s]=%i" % (k, regs[k])

    # for i in range(16):
    #     assert("s0_lo_%i_lo_ram" % (i) in regs.keys())
    #     assert("s1_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s2_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s3_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s4_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s5_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s6_lo_%i_lo_ram" % (i) not in regs.keys())
    #     assert("s7_lo_%i_lo_ram" % (i) not in regs.keys())

    # be.set_param("gain1", [1024])
    # be.set_param("gain2", [2048])
    # be.prepare()
    # regs = be.mock_roach_registers
    # assert('mode_sel' in regs.keys())
    # assert('s0_quant_gain1' in regs.keys())
    # assert('s0_quant_gain2' in regs.keys())
    # assert_equals(int(regs['s0_quant_gain1']), 1024)
    # assert_equals(int(regs['s0_quant_gain2']), 2048)
    # assert_equals(int(regs['mode_sel']), 1)

    # be.show_switching_setup()
    # DEFAULT_VALUE = "unspecified"

    # assert_equal(be.get_status('PROJID'), 'JUNK')
    # assert_equal(be.get_status('SCANLEN'), '60.0')
    # assert_almost_equal(float(be.get_status("_SBLK_01")), blanktm, 6)
    # assert_almost_equal(float(be.get_status("_SBLK_02")), blanktm, 6)
    # assert_almost_equal(float(be.get_status("_SBLK_03")), blanktm, 6)
    # assert_almost_equal(float(be.get_status("_SBLK_04")), blanktm, 6)

    # assert_equal(be.get_status("_SCAL_01"), '1')
    # assert_equal(be.get_status("_SCAL_02"), '0')
    # assert_equal(be.get_status("_SCAL_03"), '1')
    # assert_equal(be.get_status("_SCAL_04"), '0')

    # assert_almost_equal(float(be.get_status("_SPHS_01")), 0,)
    # assert_almost_equal(float(be.get_status("_SPHS_02")), 0.25)
    # assert_almost_equal(float(be.get_status("_SPHS_03")), 0.5)
    # assert_almost_equal(float(be.get_status("_SPHS_04")), 0.75)

    # assert_equal(be.get_status("_SSRF_01"), '0')
    # assert_equal(be.get_status("_SSRF_02"), '0')
    # assert_equal(be.get_status("_SSRF_03"), '1')
    # assert_equal(be.get_status("_SSRF_04"), '1')

    # assert_equal(be.get_status('BANKNAM') , 'BANKA')
    # assert_equal(be.get_status('MODENUM') , mode)
    # assert_equal(be.get_status("BW_MODE") , "low")

    # assert_almost_equal(float(be.get_status("CHAN_BW")), chan_bw)
    # assert_almost_equal(float(be.get_status("EFSAMPFR")), efsampfr)
    # assert_almost_equal(float(be.get_status("EXPOSURE")), exposure, 7)
    # assert_equal(int(be.get_status('EXPOCLKS')), expoclks)
    # assert_almost_equal(float(be.get_status("FPGACLK")), fpga_clk)

    # assert_equal(be.get_status("OBSNCHAN"), str(nchan))
    # assert_equal(be.get_status("OBS_MODE"), "LBW")
    # assert_equal(be.get_status("PKTFMT")  , "SPEAD")
    # assert_equal(be.get_status("NCHAN")   , str(nchan))
    # assert_equal(be.get_status("NPOL")    , '2')
    # assert_equal(be.get_status("NSUBBAND"), '1')
    # assert_equal(be.get_status("SUB0FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB1FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB2FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB3FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB4FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB5FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB6FREQ"), str(efsampfr / 4))
    # assert_equal(be.get_status("SUB7FREQ"), str(efsampfr / 4))

    # assert_equal(be.get_status("BASE_BW"), str(m.filter_bw)) # from config file
    # assert_equal(be.get_status("NOISESRC"), 'OFF')
    # assert_equal(be.get_status("NUMPHASE"), '4')
    # assert_almost_equal(float(be.get_status("SWPERIOD")), exposure, 5)
    # assert_equal(be.get_status("SWMASTER"), "VEGAS")
    # assert_equal(be.get_status("POLARIZE"), "SELF")
    # assert_equal(be.get_status("CRPIX1"), str(nchan / 2 + 1))
    # assert_almost_equal(float(be.get_status("SWPERINT")), 0.1 / ssg_duration)
    # assert_equal(be.get_status("NMSTOKES"), "2")

    # assert_equal(be.get_status("CAL_DCYC"), DEFAULT_VALUE)
    # assert_equal(be.get_status("CAL_FREQ"), DEFAULT_VALUE)
    # assert_equal(be.get_status("CAL_MODE"), DEFAULT_VALUE)
    # assert_equal(be.get_status("CAL_PHS") , DEFAULT_VALUE)

    # assert_equal(be.get_status("DATADIR") , dibas_data)
    # assert_equal(be.get_status("DATAPORT"), '60000')

    # assert_equal(be.get_status("EFSAMPFR"), str(efsampfr))
    # assert_equal(be.get_status("FILENUM") , DEFAULT_VALUE)
    # assert_almost_equal(float(be.get_status("FPGACLK")), frequency / 8)
    # assert_almost_equal(float(be.get_status("HWEXPOSR")), hwexposr, 5)
    # assert_equal(be.get_status("M_STTMJD"), '0')
    # assert_equal(be.get_status("M_STTOFF"), '0')
    # assert_equal(be.get_status("NBITS")   , '8')
    # assert_equal(be.get_status("NBITSADC"), '8')

    # assert_equal(be.get_status("NPKT")    , DEFAULT_VALUE)
    # assert_equal(be.get_status("OBSBW")   , str(m.nchan * chan_bw))

    # assert_equal(be.get_status("OBSFREQ") , DEFAULT_VALUE)
    # assert_equal(be.get_status("OBSERVER"), DEFAULT_VALUE)
    # assert_equal(be.get_status("OBSID")   , DEFAULT_VALUE)

    # assert_equal(be.get_status("SWVER")   , DEFAULT_VALUE)


# @with_setup(setup_dibas_tests, None)
def test_MODE1():
    """
    A VegasBackend setup test case.
    """
    check_hbw_mode("MODE1", 0.1)

def test_MODE2():
    """
    A VegasBackend setup test case.
    """
    check_hbw_mode("MODE2", 0.1)

def test_MODE3():
    """
    A VegasBackend setup test case.
    """
    check_hbw_mode("MODE3", 0.1)

def test_MODE4():
    """
    MODE4 L1LBW1 case
    """
    check_l1lbw1_mode("MODE4", 0.1)

def test_MODE5():
    """
    MODE5 L1LBW1 case
    """
    check_l1lbw1_mode("MODE5", 0.1)

def test_MODE6():
    """
    MODE6 L1LBW1 case
    """
    check_l1lbw1_mode("MODE6", 0.1)

def test_MODE7():
    """
    MODE7 L1LBW1 case
    """
    check_l1lbw1_mode("MODE7", 0.1)

def test_MODE8():
    """
    MODE8 L1LBW1 case
    """
    check_l1lbw1_mode("MODE8", 0.1)

def test_MODE9():
    """
    MODE9 L1LBW1 case
    """
    check_l1lbw1_mode("MODE9", 0.1)

def test_MODE10():
    """
    MODE10 L8LBW1 case
    """
    check_l8lbw1_mode("MODE10", 0.1)






# def test_VegasL8LBWBackend_lbw1_mode():
#     """
#     Single L8LBW1 subband test (Mode 10).
#     """
#     assert(True)

#     config = ConfigParser.ConfigParser()
#     config.readfp(open("etc/config/dibas.conf"))
#     b = BankData()
#     b.load_config(config, "BANKA")
#     m = ModeData()
#     m.load_config(config, "MODE10")
#     m.frequency = 1500

#     be = VegasL8LBWBackend(b, m, None, None, None, unit_test = True)

#     be.setPolarization('CROSS')
#     be.setNumberChannels(32768)   # mode 10
#     be.setIntegrationTime(0.1)
#     be.set_param("subband_freq", [ 300000000.0000 ])

#     be.clear_switching_states()
#     ssg_duration = 0.1
#     be.set_gbt_ss(ssg_duration,
#                   ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
#                    (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
#                    (0.5, SWbits.REF, SWbits.CALON, 0.002),
#                    (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
#                   )

#     # call dependency methods and update shared memory
#     be.prepare()
#     regs = be.mock_roach_registers
#     shm  = be.mock_status_mem
#     shmkeys = shm.keys()
#     shmkeys.sort()

#     print "Status memory:"
#     for i in shmkeys:
#         print "%s = %s" % (i, shm[i])

#     assert_equals(int(shm['NCHAN']), 32768)
#     assert_equals(int(shm['NSUBBAND']), 1)
#     assert_equals(float(shm['SUB0FREQ']), 300001144.409)
#     #print be.actual_subband_freq

#     for k in regs.keys():
#         if isinstance(regs[k], str) and len(regs[k]) > 70:
#             print "reg[%s]=" % (k), regs[k][0]
#         else:
#             print "reg[%s]=%i" % (k, regs[k])

#     for i in range(16):
#         assert("s0_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s1_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s2_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s3_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s4_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s5_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s6_lo_%i_lo_ram" % (i) not in regs.keys())
#         assert("s7_lo_%i_lo_ram" % (i) not in regs.keys())

#     be.set_param("gain1", [1024])
#     be.set_param("gain2", [2048])
#     be.prepare()
#     regs = be.mock_roach_registers
#     assert('mode_sel' in regs.keys())
#     assert('s0_quant_gain1' in regs.keys())
#     assert('s0_quant_gain2' in regs.keys())
#     assert_equals(int(regs['s0_quant_gain1']), 1024)
#     assert_equals(int(regs['s0_quant_gain2']), 2048)
#     assert_equals(int(regs['mode_sel']), 1)
#     #assert(False)

# def test_VegasL8LBWBackend_lbw8_mode():
#     """
#     Single L8LBW8 subband test (Mode 20).
#     """
#     assert(True)

#     config = ConfigParser.ConfigParser()
#     config.readfp(open("etc/config/dibas.conf"))
#     b = BankData()
#     b.load_config(config, "BANKA")
#     m = ModeData()
#     m.load_config(config, "MODE20")
#     m.frequency = 1500

#     be = VegasL8LBWBackend(b, m, None, None, None, unit_test = True)

#     be.setPolarization('CROSS')
#     be.setNumberChannels(4096)   # mode 20
#     be.setIntegrationTime(0.1)
#     be.set_param("subband_freq", [ 300e6, 320e6, 340e6, 360e6, 400e6, 420e6, 440e6, 460e6 ])

#     be.clear_switching_states()
#     ssg_duration = 0.1
#     be.set_gbt_ss(ssg_duration,
#                   ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
#                    (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
#                    (0.5, SWbits.REF, SWbits.CALON, 0.002),
#                    (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
#                   )

#     # call dependency methods and update shared memory
#     be.prepare()
#     regs = be.mock_roach_registers
#     shm  = be.mock_status_mem
#     shmkeys = shm.keys()
#     shmkeys.sort()

#     print "Status memory:"
#     for i in shmkeys:
#         print "%s = %s" % (i, shm[i])

#     assert_equals(int(shm['NCHAN']), 4096)
#     assert_equals(int(shm['NSUBBAND']), 8)

#     assert_equals(float(shm['SUB0FREQ']), 300001144.409)
#     assert_equals(float(shm['SUB1FREQ']), 319999694.824)
#     assert_equals(float(shm['SUB2FREQ']), 340001106.262)
#     assert_equals(float(shm['SUB3FREQ']), 359999656.677)
#     assert_equals(float(shm['SUB4FREQ']), 399999618.53)
#     assert_equals(float(shm['SUB5FREQ']), 420001029.968)
#     assert_equals(float(shm['SUB6FREQ']), 439999580.383)
#     assert_equals(float(shm['SUB7FREQ']), 460000991.821)
#     #print be.actual_subband_freq

#     for k in regs.keys():
#         if isinstance(regs[k], str) and len(regs[k]) > 70:
#             print "reg[%s]=" % (k), regs[k][0]
#         else:
#             print "reg[%s]=%i" % (k, regs[k])

#     for i in range(16):
#         assert("s0_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s1_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s2_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s3_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s4_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s5_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s6_lo_%i_lo_ram" % (i) in regs.keys())
#         assert("s7_lo_%i_lo_ram" % (i) in regs.keys())

#     be.set_param("gain1", [1024] * 8)
#     be.set_param("gain2", [2048] * 8)
#     be.prepare()
#     regs = be.mock_roach_registers
#     assert('mode_sel' in regs.keys())
#     assert('s0_quant_gain1' in regs.keys())
#     assert('s0_quant_gain2' in regs.keys())
#     assert_equals(int(regs['s0_quant_gain1']), 1024)
#     assert_equals(int(regs['s0_quant_gain2']), 2048)
#     assert_equals(int(regs['mode_sel']), 0)
#     #assert(False)

@skipped # Not sure how the parfile check should work JJB
def test_GUPPI_INCO_64_backend():
    """
    A GUPPI INCO backend (GuppyBackend) test setup.
    """
    config = ConfigParser.ConfigParser()
    config.readfp(open(dibas_dir + "/etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "INCO_MODE_64")
    m.frequency=800.0
    be = GuppiBackend(b, m, None, None, None, unit_test = True)
    be.set_obs_frequency(1500.0)

    be.prepare()

    print "Status memory:", be.mock_status_mem
    print "Roach (int) registers:", be.mock_roach_registers

    assert_equal(be.get_status('ACC_LEN'), '512')
    assert_equal(be.get_status('BLOCSIZE'), '33554432')
    assert_almost_equal(float(be.get_status('CHAN_DM')), 0.0)
    assert_almost_equal(float(be.get_status('CHAN_BW')), 12.5)
    assert_equal(be.get_status('DATADIR'), dibas_data)
    assert_equal(be.get_status('DS_TIME'), '1')

    assert_equal(be.get_status('FFTLEN'), '16384')

    assert_equal(be.get_status('NPOL'), '4')
    assert_equal(be.get_status('NRCVR'), '2')
    assert_equal(be.get_status('NBIN'), '256')
    assert_equal(be.get_status('NBITS'), '8')

    assert_almost_equal(float(be.get_status('OBSFREQ')), 1500.0)
    assert_almost_equal(float(be.get_status('OBSBW')), 800.0)
    assert_equal(be.get_status('OBSNCHAN'), '64')
    assert_equal(be.get_status('OBS_MODE'), 'SEARCH')
    assert_almost_equal(float(be.get_status('OFFSET0')), 0.0)
    assert_almost_equal(float(be.get_status('OFFSET1')), 0.0)
    assert_almost_equal(float(be.get_status('OFFSET2')), 0.0)
    assert_almost_equal(float(be.get_status('OFFSET3')), 0.0)
    assert_equal(be.get_status('ONLY_I'), '0')
    assert_equal(be.get_status('OVERLAP'), '0')

    assert_equal(be.get_status('POL_TYPE'), 'IQUV')
    assert_equal(be.get_status('PFB_OVER'), '12')
    assert_equal(be.get_status('PARFILE'), dibas_dir + '/etc/config/example.par')
    assert_equal(be.get_status('PKTFMT'), '1SFA')

    assert_almost_equal(float(be.get_status('SCALE0')), 1.0)
    assert_almost_equal(float(be.get_status('SCALE1')), 1.0)
    assert_almost_equal(float(be.get_status('SCALE2')), 1.0)
    assert_almost_equal(float(be.get_status('SCALE3')), 1.0)
    assert_almost_equal(float(be.get_status('TBIN')), 4.096e-05)
    assert_almost_equal(float(be.get_status('TFOLD')), 1.0)

@skipped # difficult if no host information for CODD computers.
def test_GUPPI_CODD_64_backend():
    """
    A GUPPI CODD backend (GuppyCODDBackend) test setup.
    """
    config = ConfigParser.ConfigParser()
    config.readfp(open(dibas_dir + "/etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "CODD_MODE_64")

    be = GuppiCODDBackend(b, m, None, None, unit_test = True)
    be.set_obs_frequency(1500.0)
    be.set_bandwidth(800.0)

    be.prepare()
    print "Status memory:", be.mock_status_mem
    print "Roach (int) registers:", be.mock_roach_registers

    assert_equal(be.get_status('NBITS'), '8')
    assert_equal(be.get_status('OFFSET0'), '0.0')
    assert_equal(be.get_status('OFFSET1'), '0.0')
    assert_equal(be.get_status('OFFSET2'), '0.0')
    assert_equal(be.get_status('OFFSET3'), '0.0')
    assert_equal(be.get_status('TFOLD'), '1.0')
    assert_equal(be.get_status('NRCVR'), '2')
    assert_equal(be.get_status('FFTLEN'), '16384')
    assert_equal(be.get_status('CHAN_BW'), '1.5625')
    assert_equal(be.get_status('NBIN'), '256')
    assert_equal(be.get_status('OBSNCHAN'), '64')
    assert_equal(be.get_status('SCALE0'), '1.0')
    assert_equal(be.get_status('SCALE1'), '1.0')
    assert_equal(be.get_status('SCALE2'), '1.0')
    assert_equal(be.get_status('SCALE3'), '1.0')
    assert_equal(be.get_status('NPOL'), '4')
    assert_equal(be.get_status('POL_TYPE'), 'IQUV')
    assert_equal(be.get_status('BANKNUM'), '0')
    assert_equal(be.get_status('ONLY_I'), '0')
    assert_equal(be.get_status('BLOCSIZE'), '33554432')
    assert_equal(be.get_status('ACC_LEN'), '1')
    assert_equal(be.get_status('OVERLAP'), '0')
    assert_equal(be.get_status('OBS_MODE'), 'SEARCH')
    assert_almost_equal(float(be.get_status('OBSFREQ')), 1149.21875)
    assert_equal(be.get_status('PFB_OVER'), '12')
    assert_equal(be.get_status('PARFILE'), './etc/config/example.par')
    assert_equal(be.get_status('OBSBW'), '100.0')
    assert_equal(be.get_status('DS_TIME'),'64')
    assert_equal(be.get_status('PKTFMT'), '1SFA')
    assert_equal(be.get_status('TBIN'), '6.4e-07')
    assert_equal(be.get_status('CHAN_DM'), '0.0')
