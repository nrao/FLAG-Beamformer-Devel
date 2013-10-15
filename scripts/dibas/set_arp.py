import struct

def set_arp(roach, regs, macs):
    """
    Sets the arp table on the roach.

    * *roach*: The katcp_wrapper.FpgaClient for the roach to be programmed.

    * *regs*: The registers to be programmed (list)

    * *macs*: A dictionary of the mac addresses to be programmed. The
       key is the last quad of the IP address.
    """
    for reg_name in regs:
        arp = roach.read(reg_name, 256*8, 0x3000 )
        arp_tab = list(struct.unpack('>256Q', arp))

        for ip in macs:
            arp_tab[ip] = macs[ip]

        arp = struct.pack('>256Q', *arp_tab)
        roach.write(reg_name, arp, 0x3000)
