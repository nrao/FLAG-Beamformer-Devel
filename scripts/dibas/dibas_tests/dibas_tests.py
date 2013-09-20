import ConfigParser

from VegasBackend import VegasBackend
from Backend import SWbits
from GuppiBackend import GuppiBackend
from GuppiCODDBackend import GuppiCODDBackend
from ConfigData import BankData, ModeData

def AlmostEqual(a, b, diff = 1e-6):
    if abs(float(a) - float(b)) < diff:
        return True
    else:
        print "a=", a, "- b=", b, "is >", diff

def Equal(a, b):
    if str(a) == str(b):
        return True
    else:
        print "a=", a, "is not equal to b=", b

def test_VegasBackend():
    """
    A VegasBackend setup test case.
    """

    config = ConfigParser.ConfigParser()
    config.readfp(open("dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "MODE1")

    be = VegasBackend(b, m, None, None, unit_test = True)
    frequency = 1.44e9
    sampler_frequency = frequency * 2
    nchan = 1024
    chan_bw = sampler_frequency / (nchan * 2)
    frequency_resolution = abs(chan_bw)

    be.setValonFrequency(frequency)    # config file
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

    assert AlmostEqual(be.get_status("_SBLK_01"), 0.002, 1e-6)
    assert AlmostEqual(be.get_status("_SBLK_02"), 0.002, 1e-6)
    assert AlmostEqual(be.get_status("_SBLK_03"), 0.002, 1e-6)
    assert AlmostEqual(be.get_status("_SBLK_04"), 0.002, 1e-6)

    assert be.get_status("_SCAL_01") == '1'
    assert be.get_status("_SCAL_02") == '0'
    assert be.get_status("_SCAL_03") == '1'
    assert be.get_status("_SCAL_04") == '0'

    assert AlmostEqual(be.get_status("_SPHS_01"), 0, 1e-6)
    assert AlmostEqual(be.get_status("_SPHS_02"), 0.25, 1e-6)
    assert AlmostEqual(be.get_status("_SPHS_03"), 0.5, 1e-6)
    assert AlmostEqual(be.get_status("_SPHS_04"), 0.75, 1e-6)

    assert be.get_status("_SSRF_01") == '1'
    assert be.get_status("_SSRF_02") == '1'
    assert be.get_status("_SSRF_03") == '0'
    assert be.get_status("_SSRF_04") == '0'

    assert be.get_status('BANKNAM')  == 'BANKA'
    assert be.get_status('MODENUM')  == 'MODE1'
    assert be.get_status("BW_MODE")  == "HBW"

    assert AlmostEqual(be.get_status("CHAN_BW"), chan_bw, 1e-6)
    assert AlmostEqual(be.get_status("EFSAMPFR"), sampler_frequency, 1e-6)
    assert AlmostEqual(be.get_status("EXPOSURE"), 0.1, 1e-6)
    assert AlmostEqual(be.get_status("FPGACLK"), frequency / 8, 1e-6)
    assert be.get_status("OBSNCHAN") == str(nchan)
    assert be.get_status("OBS_MODE") == "HBW"
    assert be.get_status("PKTFMT")   == "SPEAD"
    assert be.get_status("NCHAN")    == str(nchan)
    assert be.get_status("NPOL")     == '2'
    assert be.get_status("NSUBBAND") == '1' # mode 1 uses just one.
    assert be.get_status("SUB0FREQ") == str(frequency / 2)
    assert be.get_status("SUB1FREQ") == str(frequency / 2)
    assert be.get_status("SUB2FREQ") == str(frequency / 2)
    assert be.get_status("SUB3FREQ") == str(frequency / 2)
    assert be.get_status("SUB4FREQ") == str(frequency / 2)
    assert be.get_status("SUB5FREQ") == str(frequency / 2)
    assert be.get_status("SUB6FREQ") == str(frequency / 2)
    assert be.get_status("SUB7FREQ") == str(frequency / 2)

    assert Equal(be.get_status("BASE_BW"), '1400') # from config file
    assert Equal(be.get_status("NOISESRC"), 'OFF')
    assert Equal(be.get_status("NUMPHASE"), '4')
    assert AlmostEqual(be.get_status("SWPERIOD"), 0.1, 1e-5)
    assert Equal(be.get_status("SWMASTER"), "VEGAS")
    assert Equal(be.get_status("POLARIZE"), "SELF")
    assert Equal(be.get_status("CRPIX1"), nchan / 2 + 1)
    assert AlmostEqual(be.get_status("SWPERINT"), 0.1 / ssg_duration)
    assert Equal(be.get_status("NMSTOKES"), 2)

    assert be.get_status("CAL_DCYC") == DEFAULT_VALUE
    assert be.get_status("CAL_FREQ") == DEFAULT_VALUE
    assert be.get_status("CAL_MODE") == DEFAULT_VALUE
    assert be.get_status("CAL_PHS")  == DEFAULT_VALUE

    assert be.get_status("DATADIR")  == DEFAULT_VALUE
    assert be.get_status("DATAHOST") == DEFAULT_VALUE
    assert be.get_status("DATAPORT") == DEFAULT_VALUE

    assert Equal(be.get_status("EFSAMPFR"), sampler_frequency)
    assert Equal(be.get_status("EXPOSURE"), 0.1)
    assert be.get_status("FILENUM")  == DEFAULT_VALUE
    assert AlmostEqual(be.get_status("FPGACLK"), frequency / 8)
    assert be.get_status("HWEXPOSR") == DEFAULT_VALUE
    assert be.get_status("M_STTMJD") == DEFAULT_VALUE
    assert be.get_status("M_STTOFF") == DEFAULT_VALUE
    assert be.get_status("NBITS")    == DEFAULT_VALUE
    assert be.get_status("NBITSADC") == DEFAULT_VALUE

    assert be.get_status("NPKT")     == DEFAULT_VALUE
    assert be.get_status("OBSBW")    == DEFAULT_VALUE

    assert be.get_status("OBSFREQ")  == DEFAULT_VALUE
    assert be.get_status("OBSSEVER") == DEFAULT_VALUE
    assert be.get_status("OBSID")    == DEFAULT_VALUE

    assert be.get_status("SWVER")    == DEFAULT_VALUE


def test_GUPPI_INCO_64_backend():
    """
    A GUPPI INCO backend (GuppyBackend) test setup.
    """
    config = ConfigParser.ConfigParser()
    config.readfp(open("dibas.conf"))
    b = BankData()
    b.load_config(config, "BANKA")
    m = ModeData()
    m.load_config(config, "INCO_MODE_64")

    be = GuppiBackend(b, m, None, None, unit_test = True)
    be.set_obs_frequency(1500.0)
    be.set_bandwidth(800.0)

    be.prepare()

    print "Status memory:", be.mock_status_mem
    print "Roach (int) registers:", be.mock_roach_registers

    assert Equal(be.get_status('ACC_LEN'), 512)
    assert Equal(be.get_status('BLOCSIZE'), 33554432)
    assert Equal(be.get_status('CHAN_DM'), 0.0)
    assert Equal(be.get_status('CHAN_BW'), 12.5)
    #assert Equal(be.get_status('DATADIR'), )
    assert Equal(be.get_status('DS_TIME'), 1)

    assert Equal(be.get_status('FFTLEN'), 16384)

    assert Equal(be.get_status('NPOL'), 4)
    assert Equal(be.get_status('NRCVR'), 2)
    assert Equal(be.get_status('NBIN'), 256)
    assert Equal(be.get_status('NBITS'), 8)

    assert Equal(be.get_status('OBSFREQ'), 1500.0)
    assert Equal(be.get_status('OBSBW'), 800.0)
    assert Equal(be.get_status('OBSNCHAN'), 64)
    assert Equal(be.get_status('OBS_MODE'), 'SEARCH')
    assert Equal(be.get_status('OFFSET0'), 0.0)
    assert Equal(be.get_status('OFFSET1'), 0.0)
    assert Equal(be.get_status('OFFSET2'), 0.0)
    assert Equal(be.get_status('OFFSET3'), 0.0)
    assert Equal(be.get_status('ONLY_I'), 0)
    assert Equal(be.get_status('OVERLAP'), 0)

    assert Equal(be.get_status('POL_TYPE'), 'IQUV')
    assert Equal(be.get_status('PFB_OVER'), 12)
    assert Equal(be.get_status('PARFILE'), './etc/config/example.par')
    assert Equal(be.get_status('PKTFMT'), '1SFA')

    assert Equal(be.get_status('SCALE0'), 1.0)
    assert Equal(be.get_status('SCALE1'), 1.0)
    assert Equal(be.get_status('SCALE2'), 1.0)
    assert Equal(be.get_status('SCALE3'), 1.0)
    assert Equal(be.get_status('TBIN'), 4.096e-05)
    assert Equal(be.get_status('TFOLD'), 1.0)

def test_GUPPI_CODD_64_backend():
    """
    A GUPPI CODD backend (GuppyCODDBackend) test setup.
    """
    config = ConfigParser.ConfigParser()
    config.readfp(open("dibas.conf"))
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

    assert Equal(be.get_status('NBITS'), '8')
    assert Equal(be.get_status('OFFSET0'), '0.0')
    assert Equal(be.get_status('OFFSET1'), '0.0')
    assert Equal(be.get_status('OFFSET2'), '0.0')
    assert Equal(be.get_status('OFFSET3'), '0.0')
    assert Equal(be.get_status('TFOLD'), '1.0')
    assert Equal(be.get_status('NRCVR'), '2')
    assert Equal(be.get_status('FFTLEN'), 65536)
    assert Equal(be.get_status('CHAN_BW'), '12.5')
    assert Equal(be.get_status('NBIN'), '256')
    assert Equal(be.get_status('OBSNCHAN'), '8')
    assert Equal(be.get_status('SCALE0'), '1.0')
    assert Equal(be.get_status('SCALE1'), '1.0')
    assert Equal(be.get_status('SCALE2'), '1.0')
    assert Equal(be.get_status('SCALE3'), '1.0')
    assert Equal(be.get_status('NPOL'), '4')
    assert Equal(be.get_status('POL_TYPE'), 'AABBCRCI')
    assert Equal(be.get_status('BANKNUM'), '0')
    assert Equal(be.get_status('ONLY_I'), '0')
    assert Equal(be.get_status('BLOCSIZE'), 134094848)
    assert Equal(be.get_status('ACC_LEN'), '1')
    assert Equal(be.get_status('OVERLAP'), 2048)
    assert Equal(be.get_status('OBS_MODE'), 'COHERENT_SEARCH')
    assert Equal(be.get_status('OBSFREQ'), '1043.75')
    assert Equal(be.get_status('PFB_OVER'), '12')
    assert Equal(be.get_status('PARFILE'), './etc/config/example.par')
    assert Equal(be.get_status('OBSBW'), '100.0')
    assert Equal(be.get_status('DS_TIME'), '512')
    assert Equal(be.get_status('PKTFMT'), '1SFA')
    assert Equal(be.get_status('TBIN'), '1e-08')
    assert Equal(be.get_status('CHAN_DM'), '0.0')
