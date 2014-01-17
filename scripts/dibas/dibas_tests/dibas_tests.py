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

from VegasBackend import VegasBackend
from VegasL8LBWBackend import VegasL8LBWBackend
from VegasL1LBWBackend import VegasL1LBWBackend
from Backend import SWbits
from GuppiBackend import GuppiBackend
from GuppiCODDBackend import GuppiCODDBackend
from ConfigData import BankData, ModeData
from nose.tools import assert_equal, assert_almost_equal

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


# @with_setup(setup_dibas_tests, None)
def test_VegasBackend():
    """
    A VegasBackend setup test case.
    """

    config = ConfigParser.ConfigParser()
    config.readfp(open(dibas_dir + "/etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE1")

    be = VegasBackend(b, m, None, None, None, unit_test = True)
    frequency = 1.5e9
    sampler_frequency = frequency * 2
    nchan = 1024
    chan_bw = sampler_frequency / (nchan * 2)
    frequency_resolution = abs(chan_bw)

    be.setPolarization('SELF')
    be.setNumberChannels(1024)   # mode 1 (config file)
    be.setIntegrationTime(0.1)

    be.clear_switching_states()
    ssg_duration = 0.1
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

    assert_almost_equal(float(be.get_status("_SBLK_01")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_02")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_03")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_04")), 0.002, 6)

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
    assert_equal(be.get_status('MODENUM') , 'MODE1')
    assert_equal(be.get_status("BW_MODE") , "high")

    assert_almost_equal(float(be.get_status("CHAN_BW")), chan_bw)
    assert_almost_equal(float(be.get_status("EFSAMPFR")), sampler_frequency)
    assert_almost_equal(float(be.get_status("EXPOSURE")), 0.1)
    assert_almost_equal(float(be.get_status("FPGACLK")), frequency / 8)

    assert_equal(be.get_status("OBSNCHAN"), str(nchan))
    assert_equal(be.get_status("OBS_MODE"), "HBW")
    assert_equal(be.get_status("PKTFMT")  , "SPEAD")
    assert_equal(be.get_status("NCHAN")   , str(nchan))
    assert_equal(be.get_status("NPOL")    , '2')
    assert_equal(be.get_status("NSUBBAND"), '1') # mode 1 uses just one.
    assert_equal(be.get_status("SUB0FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB1FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB2FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB3FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB4FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB5FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB6FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB7FREQ"), str(sampler_frequency / 4))

    assert_equal(be.get_status("BASE_BW"), '1450') # from config file
    assert_equal(be.get_status("NOISESRC"), 'OFF')
    assert_equal(be.get_status("NUMPHASE"), '4')
    assert_almost_equal(float(be.get_status("SWPERIOD")), 0.1, 5)
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

    assert_equal(be.get_status("EFSAMPFR"), str(sampler_frequency))
    assert_almost_equal(float(be.get_status("EXPOSURE")), 0.1)
    assert_equal(be.get_status("FILENUM") , DEFAULT_VALUE)
    assert_almost_equal(float(be.get_status("FPGACLK")), frequency / 8)
    assert_equal(float(be.get_status("HWEXPOSR")), 0.000524288)
    assert_equal(be.get_status("M_STTMJD"), '0')
    assert_equal(be.get_status("M_STTOFF"), '0')
    assert_equal(be.get_status("NBITS")   , '8')
    assert_equal(be.get_status("NBITSADC"), '8')

    assert_equal(be.get_status("NPKT")    , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSBW")   , '1500000000.0')

    assert_equal(be.get_status("OBSFREQ") , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSERVER"), DEFAULT_VALUE)
    assert_equal(be.get_status("OBSID")   , DEFAULT_VALUE)

    assert_equal(be.get_status("SWVER")   , DEFAULT_VALUE)


def test_VegasL1LBWBackend():
    """A VegasL1LBWBackend setup test case.

    """

    config = ConfigParser.ConfigParser()
    config.readfp(open(dibas_dir + "/etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE4")

    be = VegasL1LBWBackend(b, m, None, None, None, unit_test = True)
    frequency = m.frequency * 1e6
    fpga_clock = frequency / 8
    sampler_frequency = frequency  / 4
    nchan = m.nchan
    chan_bw = sampler_frequency / (nchan * 2)
    print "chan_bw in test is", chan_bw
    print "nchan in test is ", nchan
    frequency_resolution = abs(chan_bw)

    be.setPolarization('SELF')
    be.setProjectId('JUNK')
    be.setScanLength(60.0)
    be.setIntegrationTime(0.1)

    be.clear_switching_states()
    ssg_duration = 0.1
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    exposure = be.ss.total_duration()

    # call dependency methods and update shared memory
    be.prepare()
    print "Status memory:", be.mock_status_mem
    print "Roach (int) registers:", be.mock_roach_registers
    be.show_switching_setup()
    DEFAULT_VALUE = "unspecified"

    assert_equal(be.get_status('PROJID'), 'JUNK')
    assert_equal(be.get_status('SCANLEN'), '60.0')
    assert_almost_equal(float(be.get_status("_SBLK_01")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_02")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_03")), 0.002, 6)
    assert_almost_equal(float(be.get_status("_SBLK_04")), 0.002, 6)

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
    assert_equal(be.get_status('MODENUM') , 'MODE4')
    assert_equal(be.get_status("BW_MODE") , "low")

    assert_almost_equal(float(be.get_status("CHAN_BW")), chan_bw)
    assert_almost_equal(float(be.get_status("EFSAMPFR")), sampler_frequency)
    assert_almost_equal(float(be.get_status("EXPOSURE")), exposure, 7)
    assert_equal(int(be.get_status('EXPOCLKS')), int(fpga_clock * exposure + 0.5))
    assert_almost_equal(float(be.get_status("FPGACLK")), fpga_clock)

    assert_equal(be.get_status("OBSNCHAN"), str(nchan))
    assert_equal(be.get_status("OBS_MODE"), "LBW")
    assert_equal(be.get_status("PKTFMT")  , "SPEAD")
    assert_equal(be.get_status("NCHAN")   , str(nchan))
    assert_equal(be.get_status("NPOL")    , '2')
    assert_equal(be.get_status("NSUBBAND"), '1')
    assert_equal(be.get_status("SUB0FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB1FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB2FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB3FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB4FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB5FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB6FREQ"), str(sampler_frequency / 4))
    assert_equal(be.get_status("SUB7FREQ"), str(sampler_frequency / 4))

    assert_equal(be.get_status("BASE_BW"), '1450') # from config file
    assert_equal(be.get_status("NOISESRC"), 'OFF')
    assert_equal(be.get_status("NUMPHASE"), '4')
    assert_almost_equal(float(be.get_status("SWPERIOD")), 0.1, 5)
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

    assert_equal(be.get_status("EFSAMPFR"), str(sampler_frequency))
    assert_equal(be.get_status("FILENUM") , DEFAULT_VALUE)
    assert_almost_equal(float(be.get_status("FPGACLK")), frequency / 8)
    assert_almost_equal(float(be.get_status("HWEXPOSR")), 0.01, 5)
    assert_equal(be.get_status("M_STTMJD"), '0')
    assert_equal(be.get_status("M_STTOFF"), '0')
    assert_equal(be.get_status("NBITS")   , '8')
    assert_equal(be.get_status("NBITSADC"), '8')

    assert_equal(be.get_status("NPKT")    , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSBW")   , '187500000.0')

    assert_equal(be.get_status("OBSFREQ") , DEFAULT_VALUE)
    assert_equal(be.get_status("OBSERVER"), DEFAULT_VALUE)
    assert_equal(be.get_status("OBSID")   , DEFAULT_VALUE)

    assert_equal(be.get_status("SWVER")   , DEFAULT_VALUE)

def test_VegasL8LBWBackend_lbw1_mode():
    """
    Single L8LBW1 subband test (Mode 10).
    """
    assert(True)

    config = ConfigParser.ConfigParser()
    config.readfp(open("etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE10")
    m.frequency = 1500

    be = VegasL8LBWBackend(b, m, None, None, None, unit_test = True)

    be.setPolarization('CROSS')
    be.setNumberChannels(32768)   # mode 10
    be.setIntegrationTime(0.1)
    be.set_param("subband_freq", [ 300000000.0000 ])

    be.clear_switching_states()
    ssg_duration = 0.1
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    # call dependency methods and update shared memory
    be.prepare()
    regs = be.mock_roach_registers
    shm  = be.mock_status_mem
    shmkeys = shm.keys()
    shmkeys.sort()

    print "Status memory:"
    for i in shmkeys:
        print "%s = %s" % (i, shm[i])

    assert_equals(int(shm['NCHAN']), 32768)
    assert_equals(int(shm['NSUBBAND']), 1)
    assert_equals(float(shm['SUB0FREQ']), 300001144.409)
    #print be.actual_subband_freq

    for k in regs.keys():
        if isinstance(regs[k], str) and len(regs[k]) > 70:
            print "reg[%s]=" % (k), regs[k][0]
        else:
            print "reg[%s]=%i" % (k, regs[k])

    for i in range(16):
        assert("s0_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s1_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s2_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s3_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s4_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s5_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s6_lo_%i_lo_ram" % (i) not in regs.keys())
        assert("s7_lo_%i_lo_ram" % (i) not in regs.keys())

    be.set_param("gain1", [1024])
    be.set_param("gain2", [2048])
    be.prepare()
    regs = be.mock_roach_registers
    assert('mode_sel' in regs.keys())
    assert('s0_quant_gain1' in regs.keys())
    assert('s0_quant_gain2' in regs.keys())
    assert_equals(int(regs['s0_quant_gain1']), 1024)
    assert_equals(int(regs['s0_quant_gain2']), 2048)
    assert_equals(int(regs['mode_sel']), 1)
    #assert(False)

def test_VegasL8LBWBackend_lbw8_mode():
    """
    Single L8LBW8 subband test (Mode 20).
    """
    assert(True)

    config = ConfigParser.ConfigParser()
    config.readfp(open("etc/config/dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE20")
    m.frequency = 1500

    be = VegasL8LBWBackend(b, m, None, None, None, unit_test = True)

    be.setPolarization('CROSS')
    be.setNumberChannels(4096)   # mode 20
    be.setIntegrationTime(0.1)
    be.set_param("subband_freq", [ 300e6, 320e6, 340e6, 360e6, 400e6, 420e6, 440e6, 460e6 ])

    be.clear_switching_states()
    ssg_duration = 0.1
    be.set_gbt_ss(ssg_duration,
                  ((0.0, SWbits.SIG, SWbits.CALON, 0.002),
                   (0.25, SWbits.SIG, SWbits.CALOFF, 0.002),
                   (0.5, SWbits.REF, SWbits.CALON, 0.002),
                   (0.75, SWbits.REF, SWbits.CALOFF, 0.002))
                  )

    # call dependency methods and update shared memory
    be.prepare()
    regs = be.mock_roach_registers
    shm  = be.mock_status_mem
    shmkeys = shm.keys()
    shmkeys.sort()

    print "Status memory:"
    for i in shmkeys:
        print "%s = %s" % (i, shm[i])

    assert_equals(int(shm['NCHAN']), 4096)
    assert_equals(int(shm['NSUBBAND']), 8)

    assert_equals(float(shm['SUB0FREQ']), 300001144.409)
    assert_equals(float(shm['SUB1FREQ']), 319999694.824)
    assert_equals(float(shm['SUB2FREQ']), 340001106.262)
    assert_equals(float(shm['SUB3FREQ']), 359999656.677)
    assert_equals(float(shm['SUB4FREQ']), 399999618.53)
    assert_equals(float(shm['SUB5FREQ']), 420001029.968)
    assert_equals(float(shm['SUB6FREQ']), 439999580.383)
    assert_equals(float(shm['SUB7FREQ']), 460000991.821)
    #print be.actual_subband_freq

    for k in regs.keys():
        if isinstance(regs[k], str) and len(regs[k]) > 70:
            print "reg[%s]=" % (k), regs[k][0]
        else:
            print "reg[%s]=%i" % (k, regs[k])

    for i in range(16):
        assert("s0_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s1_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s2_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s3_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s4_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s5_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s6_lo_%i_lo_ram" % (i) in regs.keys())
        assert("s7_lo_%i_lo_ram" % (i) in regs.keys())

    be.set_param("gain1", [1024] * 8)
    be.set_param("gain2", [2048] * 8)
    be.prepare()
    regs = be.mock_roach_registers
    assert('mode_sel' in regs.keys())
    assert('s0_quant_gain1' in regs.keys())
    assert('s0_quant_gain2' in regs.keys())
    assert_equals(int(regs['s0_quant_gain1']), 1024)
    assert_equals(int(regs['s0_quant_gain2']), 2048)
    assert_equals(int(regs['mode_sel']), 0)
    #assert(False)

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
