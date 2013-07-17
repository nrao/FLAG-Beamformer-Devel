#!/usr/bin/python2.6


import numpy, corr, time, struct, sys, logging, pylab, matplotlib, scipy

#connect roach
fpga = corr.katcp_wrapper.FpgaClient('192.168.40.70')
time.sleep(1)

#pay attention, the snap module is controled by the inner software register
#as snap64_ctrl which can be found at /proc/pid/hw/ioreg/
#the number write to the snap ctrl register is from 0 to 7, do not use the outer trig
#to control the snap module

fpga.write_int('s64_adcp1_ctrl', 0)
time.sleep(1)
fpga.write_int('s64_adcp1_ctrl', 7)
time.sleep(1)


d_0 = struct.unpack('>16384b', fpga.read('s64_adcp1_bram_msb', 8192*2))

fd_0 = []

#put the parrellel data into a serial array
for i in range(2048):
        
    fd_0.append(d_0[i*4+0]/128.0)
    fd_0.append(d_0[i*4+1]/128.0)
    fd_0.append(d_0[i*4+2]/128.0)
    fd_0.append(d_0[i*4+3]/128.0)


#plot it
pylab.plot(fd_0, label='data')
pylab.show()

