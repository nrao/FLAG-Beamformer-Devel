import struct, socket, time


#### Define global constants ####


# IP address and port for GUPPI receiver
UDP_IP			        = "127.0.0.1"
UDP_PORT		        = 50000

WAVEFORM_SZ	            = 2000

# Item Identifiers for the SPEAD packet
# Note: these are constants and should not be modified later
IMMEDIATE		        = 0x800000
POINTER			        = 0x000000

heap_cntr_id 		    = 0x000001 + IMMEDIATE
heap_size_id 		    = 0x000002 + IMMEDIATE
heap_offset_id 		    = 0x000003 + IMMEDIATE
packet_payload_len_id	= 0x000004 + IMMEDIATE

time_cntr_id 		    = 0x000020 + IMMEDIATE
spectrum_cntr_id	    = 0x000021 + IMMEDIATE
integ_size_id		    = 0x000022 + IMMEDIATE
mode_id			        = 0x000023 + IMMEDIATE
status_bits_id		    = 0x000024 + IMMEDIATE
payload_data_off_id	    = 0x000025 + POINTER


#### Function definitions ####


# Convert SPEAD packet dictionary to a byte-stream
def packet_to_bytes(packet):
	return struct.pack('> 2I xHxL xHxL xHxL xHxL xHxL xHxL xHxL xHxL xHxL xHxL 2000I',
	packet['header_upr'],
	packet['header_lwr'],
	heap_cntr_id, packet[heap_cntr_id],
	heap_size_id, packet[heap_size_id], 
	heap_offset_id, packet[heap_offset_id], 
	packet_payload_len_id, packet[packet_payload_len_id], 
	time_cntr_id , packet[time_cntr_id],
	spectrum_cntr_id, packet[spectrum_cntr_id], 
	integ_size_id, packet[integ_size_id], 
	mode_id, packet[mode_id], 
	status_bits_id, packet[status_bits_id],
	payload_data_off_id, packet[payload_data_off_id],
	*packet['payload']
)


#### Main function ###


# Generate the sampled waveform to transmit
waveform = range(WAVEFORM_SZ)

# Create the SPEAD packet
# The header is 8 bytes long
# The Item Identifiers (keys) are 3 bytes long
# The values and addresses are 5 bytes long
spead_packet = {
	'header_upr'	        : 0x53040305,
    'header_lwr'            : 0x0000000A,
	heap_cntr_id		    : 0,
	heap_size_id		    : 6*8 + WAVEFORM_SZ*4,	# size of total heap
	heap_offset_id		    : 0,
	packet_payload_len_id	: WAVEFORM_SZ*4,	    # length of this pkt's payload
	time_cntr_id		    : 0,
	spectrum_cntr_id	    : 0,
	integ_size_id		    : 256,
	mode_id			        : 1,
	status_bits_id		    : 0,
	payload_data_off_id	    : 0,
	'payload'		        : waveform
}

# Send 10 packets
for heap_cntr in range(1500, 12000):

    if heap_cntr < 2000:	
        spead_packet[heap_cntr_id] = heap_cntr
    else:	
        spead_packet[heap_cntr_id] = heap_cntr - 2000
 	
    # Send the packet
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet_to_bytes(spead_packet), (UDP_IP, UDP_PORT))

    time.sleep(0.001)
