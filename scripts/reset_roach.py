import corr,time

roach = 'roach03'
fpga=corr.katcp_wrapper.FpgaClient(roach,7147)

tick = time.time()

while time.time() - tick < 5:
    if fpga.is_connected():
	break 

fpga.write_int('rst',1)
fpga.write_int('rst',0)

def reset():
    fpga.write_int('sg_sync',0x12)

    #rst counter
    fpga.write_int('rst',1)
    fpga.write_int('rst',0)

    fpga.write_int('arm',0)
    fpga.write_int('arm',1)
    fpga.write_int('arm',0)
    fpga.write_int('sg_sync',0x12)
    fpga.write_int('sg_sync',0x11)

reset()

