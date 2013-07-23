#! /usr/bin/env python2.6

import corr,time,numpy,struct,sys, socket
from matplotlib.pyplot import * 
execfile('mixercic_funcs.py')
#execfile('l8_debug_funcs.py')

n_inputs = 4 # number of simultaneous inputs - should be 4 for final design
bramlength = 10 # size of brams used in mixers (2^?)
#lo_f = [104, 103, 75, 91, 92, 122, 135, 144]
lo_f = [75, 91, 92, 100, 104, 122, 135, 144]
lo_f_actual = lo_f
bw = 1200 # bandwidth, in MHz
dec_rate = 128.0

#boffile='v13_16r128dr_ver113b_2013_Mar_21_0034.bof'
#boffile='v13_16r64dr_ver114_2013_Apr_06_2235.bof'
#boffile='v13_16r128dr_ver117_2013_Apr_12_0131.bof'
#boffile='l8_ver115_2013_May_17_1027.bof'
#boffile='l8_ver117_2013_May_25_1242.bof'
#boffile='l8_ver118_01_2013_Jun_05_0524.bof'
#boffile='l8_ver118_02_2013_Jun_10_0215.bof'
#boffile='l8_ver118_03_2013_Jun_12_0031.bof'
#boffile='l8_ver118_04_2013_Jun_12_1609.bof'
#boffile='l8_ver118_05_2013_Jun_14_1628.bof'
#boffile='l8_ver118_06_2013_Jun_15_1631.bof'
#boffile='l8_ver118_07_2013_Jun_20_1345.bof'
#boffile='l8_ver121_2013_Jun_17_1308.bof'
#boffile='l8_ver118_10_2013_Jul_20_1659.bof'
boffile='l8_ver118_10_2013_Jul_21_0137.bof'
roach = '192.168.40.99'

dest_ip  = 10*(2**24) +  145 #10.0.0.145
src_ip   = 10*(2**24) + 4  #10.0.0.4

dest_port     = 60000
fabric_port     = 60000

mac_base = (2<<40) + (2<<32)

def boardreset():
  print '------------------------'
  print 'Configuring receiver core...',
  # have to do this manually for now
  fpga.tap_start('tap0','gbe0',mac_base+src_ip,src_ip,fabric_port)
  print 'done'
  print '------------------------'
  print 'Setting-up packet core...',
  sys.stdout.flush()
  fpga.write_int('dest_ip',dest_ip)
  fpga.write_int('dest_port',dest_port)
  fpga.write_int('sg_sync', 0b10100)
  time.sleep(1)
  fpga.write_int('arm', 0)
  fpga.write_int('arm', 1)
  fpga.write_int('arm', 0)
  time.sleep(1)
  fpga.write_int('sg_sync', 0b10101)
  fpga.write_int('sg_sync', 0b10100)
  print 'done'

def setgain(subband, gain1, gain2):
  fpga.write_int('s'+str(subband)+'_quant_gain1',gain1)
  fpga.write_int('s'+str(subband)+'_quant_gain2',gain2)

def gbeconf():
  fpga.tap_start('tap0','gbe0',mac_base+src_ip,src_ip,fabric_port)

def getadc0():
  adc0=np.fromstring(fpga.snapshot_get('adcsnap0',man_trig=True,man_valid=True)['data'],dtype='>i1')
  return adc0

def getadc1():
  adc1=np.fromstring(fpga.snapshot_get('adcsnap1',man_trig=True,man_valid=True)['data'],dtype='>i1')
  return adc1

def makecomplex(data0, data1):
    data_len = len(data0)
    complexdata = np.zeros(data_len, dtype=np.complex64)
    complexdata.real = data0
    complexdata.imag = data1
    return complexdata

def get_mixer_ri(pol, reim):
    mixer = np.fromstring(fpga.snapshot_get('s1_mixer_p'+str(pol)+reim,man_trig=True, man_valid=True)['data'],dtype='>i1')
    return mixer
    
def get_mixer(pol):
    mixer_re = get_mixer_ri(pol, 're')
    mixer_im = get_mixer_ri(pol, 'im')
    return mixer_re, mixer_im

def get_mixer_lmt():
    lmt = fpga.read_int('s1_mixer_cnt')
    return lmt

def get_dr16(pol):
    '''
    subband 1, first CIC filter (dec_rate=16)
    '''
    dr16=np.fromstring(fpga.snapshot_get('s1_p'+str(pol)+'_dr16', man_trig=True, man_valid=True)['data'], dtype='>i8')
    dr16_re = dr16[0::2]
    dr16_im = dr16[1::2]
    dr16_out = makecomplex(dr16_re, dr16_im)
    return dr16_out

def get_1stcic(pol):
    first_cic = np.fromstring(fpga.snapshot_get('s1_p'+str(pol)+'_firstcic', man_trig=True, man_valid=True)['data'],dtype='>i4')
    first_cic_full = np.fromstring(fpga.snapshot_get('s1_p'+str(pol)+'_firstcic_full', man_trig=True, man_valid=True)['data'],dtype='>i8')
    fst_c_re = first_cic[0::4]
    fst_c_im = first_cic[1::4]
    fst_c_full_re = first_cic_full[0::4]
    fst_c_full_im = first_cic_full[1::4]
    first_cic_out = makecomplex(fst_c_re, fst_c_im)
    first_cic_full_out = makecomplex(fst_c_full_re, fst_c_full_im)
    return first_cic_out, first_cic_full_out

def get_halfband(pol):
    halfband = np.fromstring(fpga.snapshot_get('s1_p'+str(pol)+'_halfband', man_trig=True, man_valid=True)['data'], dtype='>i4')
    hb_re = halfband[0::4]
    hb_im = halfband[1::4]
    hb_out = makecomplex(hb_re, hb_im)
    return hb_out

def get_chc(pol):
    chc_raw = np.fromstring(fpga.snapshot_get('cic'+str(pol)+'_snap', man_trig=True, man_valid=True)['data'], dtype='>i4')
    chc_re = chc_raw[0::2*(dec_rate/(2**n_inputs))]
    chc_im = chc_raw[1::2*(dec_rate/(2**n_inputs))]
    chc = makecomplex(chc_re, chc_im)
    return chc

def plotfft2(data, lo_f, bw, dec_rate):
    f_index = np.linspace(lo_f - bw/(1.*dec_rate), lo_f + bw/(1.*dec_rate), len(
data))
    plot(f_index, 10*np.log10(np.abs(np.fft.fftshift(np.fft.fft(data)))))


def plotfft(ax, data, lo_f, bw, dec_rate):
    f_index = np.linspace(lo_f - bw/(1.*dec_rate), lo_f + bw/(1.*dec_rate), len(data))
    ax.plot(f_index, 10*np.log10(np.abs(np.fft.fftshift(np.fft.fft(data)))))

def plot_adc(bw):
    data0 = getadc0()
    data1 = getadc1()
    f, (ax1, ax2) = subplots(2)
    ax1.plot(data0[100:200],'-o')
    ax2.plot(data1[100:200],'-o')
    show()
    f, (ax1, ax2) = subplots(2)
    adcdata0 = makecomplex(data0, data0)
    adcdata1 = makecomplex(data1, data1)
    plotfft(ax1, adcdata0, 0, bw, 1)
    plotfft(ax2, adcdata1, 0, bw, 1)
    show()

def plot_mixer(lo_f):
    m_p1re, m_p1im = get_mixer(1)
    m_p2re, m_p2im = get_mixer(2)
    f, (ax1, ax2, ax3, ax4) = subplots(4)
    ax1.plot(m_p1re,'-o')
    ax2.plot(m_p1im,'-o')
    ax3.plot(m_p2re,'-o')
    ax4.plot(m_p2im,'-o')
    show()
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, makecomplex(m_p1re, 0), lo_f, bw, 1)
    plotfft(ax2, makecomplex(m_p2re, 0), lo_f, bw, 1)    
    show()

def plot_dr16(lo_f):
    dr16_p1 = get_dr16(1)
    dr16_p2 = get_dr16(2)
    f, (ax1, ax2, ax3, ax4) = subplots(4)
    ax1.plot(dr16_p1.real, '-o')
    ax2.plot(dr16_p1.imag, '-o')
    ax3.plot(dr16_p2.real, '-o')
    ax4.plot(dr16_p2.imag, '-o')
    show()
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, dr16_p1, lo_f, bw, 16.)
    plotfft(ax2, dr16_p2, lo_f, bw, 16.)
    show()

def plot_1stcic(lo_f):
    c_p1, c_p1_full = get_1stcic_ri(1)
    c_p2, c_p2_full = get_1stcic_ri(2)
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, c_p1, lo_f, bw, 32.)
    plotfft(ax2, c_p2, lo_f, bw, 32.)
    show()
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, c_p1_full, lo_f, bw, 32.)
    plotfft(ax2, c_p2_full, lo_f, bw, 32.)
    show()

def plot_hb(lo_f):
    hb_p1 = get_halfband(1)
    hb_p2 = get_halfband(2)
    f, (ax1, ax2, ax3, ax4) = subplots(4)
    ax1.plot(hb_p1.real, '-o')
    ax2.plot(hb_p1.imag, '-o')
    ax3.plot(hb_p2.real, '-o')
    ax4.plot(hb_p2.imag, '-o')
    show()
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, hb_p1, lo_f, bw, 32.)
    plotfft(ax2, hb_p2, lo_f, bw, 32.)
    show()

def plot_chc(lo_f):
    chc_p1 = get_chc(1)
    chc_p2 = get_chc(2)
    f, (ax1, ax2, ax3, ax4) = subplots(4)
    ax1.plot(chc_p1.real, '-o')
    ax2.plot(chc_p1.imag, '-o')
    ax3.plot(chc_p2.real, '-o')
    ax4.plot(chc_p2.imag, '-o')
    show()
    f, (ax1, ax2) = subplots(2)
    plotfft(ax1, chc_p1, lo_f, bw, dec_rate)
    plotfft(ax2, chc_p2, lo_f, bw, dec_rate)
    show()

def getsubband1():
  ''' This is a plot after the first subband (pol1, real)'''
  s1p1re=np.fromstring(fpga.snapshot_get('s1p1re_snap',man_trig=True,man_valid=True)['data'],dtype='<i1')[::4]
  return s1p1re 

def getpacket():
  size=8208
  udp_ip='10.0.0.145'
  udp_port=60000
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
  sock.bind((udp_ip, udp_port))
  data, addr = sock.recvfrom(size)
  sock.close()
  return data

def plotpacketfft(i):
  data=getpacket()
  a=np.array(struct.unpack('>8208b', data), dtype=np.int8)[16:] #question here, >, or <?
  realX = a[0+i*4::32]
  imagX = a[1+i*4::32]
  realY = a[2+i*4::32]
  imagY = a[3+i*4::32]
  X = np.zeros(256, dtype=np.complex64)
  X.real = realX.astype(np.float)
  X.imag = imagX.astype(np.float)
  l = len(X)
  Y = np.zeros(256, dtype=np.complex64)
  Y.real = realY.astype(np.float)
  Y.imag = imagY.astype(np.float)
  l_y = len(Y)
  f_index_x = np.linspace(lo_f_actual[i] - bw/(1.*128), lo_f_actual[i] + bw/(1.*128), l)
  subplot(211)
  plot(f_index_x, 10*np.log10(np.abs(np.fft.fftshift(np.fft.fft(X)))))
  f_index_y = np.linspace(lo_f_actual[i] - bw/(1.*128), lo_f_actual[i] + bw/(1.*128), l_y)
  subplot(212)
  plot(f_index_y, 10*np.log10(np.abs(np.fft.fftshift(np.fft.fft(Y)))))

def plotsubband1fft():
  #to fix, need complex data
  s1p1re = getsubband1()
  f_index = np.linspace(lo_f_actual[1] - bw/(2.*128), lo_f_actual[1] + bw/(2.*128), len(s1p1re))
  plot(f_index, 10*np.log10(np.abs(np.fft.fftshift(np.fft.fft(s1p1re)))))

def lo_adjust(i, new_lo):
  lo_actual, lo_wave = lo_setup(fpga, new_lo, bw, n_inputs, 's'+str(i), bramlength, 1)
  lo_f_actual[i] = new_lo
  return lo_actual, lo_wave


print('Connecting to server %s on port... '%(roach)),
fpga = corr.katcp_wrapper.FpgaClient(roach)
time.sleep(2)

if fpga.is_connected():
	print 'ok\n'	
else:
    print 'ERROR\n'

print '------------------------'
print 'Programming FPGA with %s...' % boffile,
fpga.progdev(boffile)
time.sleep(5)
print 'ok\n'

print '------------------------'
print 'Setting the port 0 linkup :',

gbe0_link=bool(fpga.read_int('gbe0'))
print gbe0_link

if not gbe0_link:
   print 'There is no cable plugged into port0'

print '------------------------'
print 'Configuring receiver core...',   
# have to do this manually for now
fpga.tap_start('tap0','gbe0',mac_base+src_ip,src_ip,fabric_port)
time.sleep(2)
print 'done'

print '------------------------'
print 'Setting-up packet core...',
sys.stdout.flush()
fpga.write_int('dest_ip',dest_ip)
time.sleep(2)
fpga.write_int('dest_port',dest_port)
time.sleep(2)

#fpga.write_int('mode_sel', 0)
time.sleep(1)
fpga.write_int('sg_sync', 0b10100)
time.sleep(1)
fpga.write_int('arm', 0)
time.sleep(1)
fpga.write_int('arm', 1)
time.sleep(1)
fpga.write_int('arm', 0)
time.sleep(1)
fpga.write_int('sg_sync', 0b10101)
time.sleep(1)
fpga.write_int('sg_sync', 0b10100)
print 'done'

#########################################


#lo_setup(fpga, lo_f, bandwidth=400, n_inputs, cnt_r_name='mixer_cnt', mixer_name='s1', bramlength=8)


#lo_f_actual[0] = lo_setup(fpga, lo_f[0], bw, n_inputs, 's0', bramlength)
lo_f_actual[1], wave = lo_setup(fpga, lo_f[1], bw, n_inputs, 's1', bramlength, 1)
#lo_f_actual[2] = lo_setup(fpga, lo_f[2], bw, n_inputs, 's2', bramlength)
#lo_f_actual[3] = lo_setup(fpga, lo_f[3], bw, n_inputs, 's3', bramlength)
#lo_f_actual[4] = lo_setup(fpga, lo_f[4], bw, n_inputs, 's4', bramlength)
#lo_f_actual[5] = lo_setup(fpga, lo_f[5], bw, n_inputs, 's5', bramlength)
#lo_f_actual[6] = lo_setup(fpga, lo_f[6], bw, n_inputs, 's6', bramlength)
#lo_f_actual[7] = lo_setup(fpga, lo_f[7], bw, n_inputs, 's7', bramlength)

time.sleep(1)

setgain(1, 2**12, 2**14)
#fpga.write_int('s0_quant_gain',2**20)
#fpga.write_int('s1_quant_gain',2**20)
#fpga.write_int('s2_quant_gain',2**20)
#fpga.write_int('s3_quant_gain',2**20)
#fpga.write_int('s4_quant_gain',2**20)
#fpga.write_int('s5_quant_gain',2**20)
#fpga.write_int('s6_quant_gain',2**20)
#fpga.write_int('s7_quant_gain',2**20)

print "Board Clock: ",fpga.est_brd_clk()
