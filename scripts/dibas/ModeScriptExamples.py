# Example of configuration script using DIBAS Dealer/Players
# To run:
# ipython -i ConfigureExamples.py

from dealer import Dealer

# Create an instance of the Dealer
d = Dealer()

#project_id = 'TGBT13A_511_03'
# A 'throw-away' project for testing:
project_id = 'JUNK'
# A observer for testing:
observer = 'test_observer'

def CODD(channels):
    """
    Configure the DIBAS for Coherent dedispersion with 2048 channels.
    """
    # Load the firmware and configure the roach IP addresses:
    d.set_mode('CODD_MODE_%s' % (str(channels)), force=True)
    # Set some parameters specific to this mode:
    d.set_param(observer=observer)
    d.set_param(project_id=project_id)
    d.set_param(bandwidth=800)
    d.set_param(scale_p1=30.0)
    d.set_param(scale_p0=30.0)
    d.set_param(scan_length=30.0)
    d.set_param(obs_frequency=1445)
    # This is for testing only. It overrides the node number, which is normally
    # taken from the configuration file. (BANKA is node 1, BANKB is node 2, ...)
    # d.set_param(_node_number=1)
    # Update the values in status memory and firmware registers
    return d.prepare()

def INCO(channels):
    """
    Configure the DIBAS for incoherent dedispersion with 2048 channels
    """
    d.set_mode('INCO_MODE_%s' % (str(channels)), True)
    d.set_param(observer=observer)
    d.set_param(project_id=project_id)
    d.set_param(bandwidth=800)
    d.set_param(scan_length=30.0)
    d.set_param(scale_i=900)
    d.set_param(scale_q=900)
    d.set_param(scale_u=900)
    d.set_param(scale_v=900)
    return d.prepare()

def MODE1():
    """
    Configure the DIBAS for 'mode1' (1024 channels) spectral line mode.
    The switching setup below provides 4 states, with a short blanking period
    inbetween transitions.
    """
    d.set_mode('MODE1')
    d.set_param(observer=observer)
    d.set_param(project_id=project_id)
    d.set_param(exposure=1.0)
    d.set_param(scan_length=30.0)
    d.clear_switching_states()
    d.add_switching_state(0.05, blank=True, cal=False, sig=False)
    d.add_switching_state(0.2, blank=False, cal=False, sig=False)
    d.add_switching_state(0.25, blank=False, cal=True, sig=False)
    d.add_switching_state(0.05, blank=True, cal=False, sig=True)
    d.add_switching_state(0.20, blank=False, cal=False, sig=True)
    d.add_switching_state(0.25, blank=False, cal=True, sig=True)
    # This normally would be set automatically, and should not be needed.
    # However, it is also a good example of how to set an individual
    # register on one of the Banks.
    d.players['BANKH'].katcp.write_int('fftshift', 0x5555)
    d.prepare()

# This is GBT/Ygor specific:
#from gbt.ygor import GrailClient
#
#cl = GrailClient("wind", 18000)
#SC = cl.create_manager('ScanCoordinator')
#auto_set = False
#
#def state_callback(a, b, c):
#    """
#    This is called whenever scans are run on the GBT telescope
#    """
#    stateval = c['state']['state']['value']
#    if stateval == 'Running':
#        print d.start()
#    elif stateval == 'Stopping' or stateval == 'Aborting':
#        print d.stop()

#def source_callback(a, b, c):
#    """
#    This is called whenever the observer sets the 'source' parameter
#    """
#    if b == 'source':
#        d.set_status(SRC_NAME=c['source']['source']['value'])
#
#def set_auto():
#    """
#    Enable GBT telescope events to be monitored for state and source parameters.
#    """
#    global auto_set
#    if not auto_set:
#        SC.reg_param('state', state_callback)
#        SC.reg_param('source', source_callback)
#        auto_set = True
#
#def clear_auto():
#    """
#    Disable GBT telescope events to be monitored for state and source parameters.
#    """
#    global auto_set
#    if auto_set:
#        SC.unreg_param('state', state_callback)
#        SC.unreg_param('source', source_callback)
#        auto_set = False
