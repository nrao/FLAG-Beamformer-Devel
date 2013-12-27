/* vegas_udp.c
 *
 * UDP implementations.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <execinfo.h>

#include "vegas_udp.h"
#include "vegas_databuf.h"
#include "vegas_error.h"
#include "vegas_defines.h"
#include "spead_packet.h"
#include "spead_heap.h"

enum IDIndex { HEAP_COUNTER_IDX, HEAP_SIZE_IDX, HEAP_OFFSET_IDX,
               PAYLOAD_OFFSET_IDX, TIME_STAMP_IDX, SPECTRUM_COUNTER_IDX,
               SPECTRUM_PER_INTEGRATION_IDX, MODE_NUMBER_IDX, NSPEAD_IDXES }; 

/// A (fake) SPEAD header template for low-bw mode non-SPEAD packets
const unsigned char sphead[] = { 
    SPEAD_MAGIC_HEAD_CHAR,                         0x00, 0x00, 0x00, 0x08, // 0
    0x80, 0x00, HEAP_COUNTER_ID,             0x00, 0x00, 0x00, 0x00, 0x00, // 1 (pkt num)
    0x80, 0x00, HEAP_SIZE_ID,                0x00, 0x00, 0x00, 0x20, 0x20, // 2
    0x80, 0x00, HEAP_OFFSET_ID,              0x00, 0x00, 0x00, 0x00, 0x00, // 3
    0x80, 0x00, PAYLOAD_OFFSET_ID,           0x00, 0x00, 0x00, 0x20, 0x00, // 4
    0x80, 0x00, TIME_STAMP_ID,               0x00, 0x00, 0x00, 0x00, 0x00, // 5 (abs time?)
    0x80, 0x00, SPECTRUM_COUNTER_ID,         0x00, 0x00, 0x00, 0x00, 0x0D, // 6
    0x80, 0x00, SPECTRUM_PER_INTEGRATION_ID, 0x00, 0x00, 0x00, 0x00, 0x00, // 7
    0x00, 0x00, MODE_NUMBER_ID,              0x00, 0x00, 0x00, 0x00, 0x00  // 8 
};


#ifdef NEW_GBT
// #define BYTE_ARR_TO_UINT(array, idx) (ntohl(((unsigned int*)(array))[idx]))
#endif

// This is actually faster than the macro expansion of above, plus it conforms with
// C99 standards.
union __XXY
{
    const int*  iptr;
    const char* cptr;
};
static inline unsigned int BYTE_ARR_TO_UINT(const char *p1, int idx)
{
    union __XXY x;
    x.cptr = p1;
    return ntohl(x.iptr[idx]);
}

/// Extract the number of items in the SPEAD header.
static uint32_t ok_packets =0;
static uint32_t error_packets=0;

int32_t num_spead_items(const VegasSpeadPacketHeader *sptr)
{
    //Check that the header is valid
    if( BYTE_ARR_TO_UINT((const char *)sptr, 0) != SPEAD_MAGIC_HEAD )
    {
        vegas_error("num_spead_items()", "Spead header missing\n");
        return (VEGAS_ERR_PACKET);
    }

    int32_t num_items = (int32_t)be16toh(sptr->spead_header.num_items);
    if (num_items > 10)
    {
        void *bt[32];
        int nentries;
        
        int i;
        char *ptr;
        
        vegas_error("num_spead_items", "num_items > 10");
        
        nentries = backtrace(bt,sizeof(bt) / sizeof(bt[0]));
        backtrace_symbols_fd(bt,nentries,fileno(stdout));

        error_packets++;
        ptr = (char *)&sptr->spead_header;
        for (i=0; i<8; ++i)
        {
            printf("%x ", 0xff & *ptr);
            ptr++;
        }
        printf("good=%d, bad=%d\n ", ok_packets, error_packets);
        // pthread_exit(0);
        return (VEGAS_ERR_PACKET); // error code
    }
    ok_packets++;

    return num_items;
}

/// The SPEAD header item table is big endian. 
/// This byte swaps the item table prior to further processing.
int byte_swap_spead_header(struct vegas_udp_packet *p)
{
    int32_t num_items;
    int32_t i;

    VegasSpeadPacketHeader *sheader = (VegasSpeadPacketHeader *)p->data;

    //Get number of items (from last 2 bytes of header)
    num_items = num_spead_items(sheader);
    if (num_items < 0)
        return VEGAS_ERR_PACKET;
        
    // byte swap the ItemPointer table
    uint64_t *pd;
    pd = (uint64_t *)&sheader->items[0];
    for (i=0; i<num_items; ++i)
    {
        *pd = be64toh(*pd);
        ++pd;
    }
    return(VEGAS_OK);
}

/// Take a low bandwidth packet and insert a SPEAD header onto it.
/// The resulting packet is in SPEAD format, with the item table in host byte order.
void lbw_packet_to_host_spead(struct vegas_udp_packet *b)
{
    
    // Index into the raw data buffer to find the LBW header. Remembering in
    // the LBW mode we record the packet at an offset from the start of the
    // packet data buffer.
    LBW_endian *wire = (LBW_endian *)&b->data[sizeof(sphead)-2*sizeof(uint64_t)];
    // Convert to host order. LBW_endian union allows for relocation of status bits
    wire->header = be64toh(wire->header);
    
    uint64_t tmcounter = wire->le.time_counter;
    uint8_t  status_bits = wire->le.status & 0xF; // Status is lower 4 bits.
    
    // It apears that the first field is the FPGA counter, which increments
    // by 0x800 in each packet. Hence the packet sequence number is tmcounter
    // (60 bits) shifted down by 11 leaving a 49 bit value. Only 40 bits
    // fits into a SPEAD item_address field, so the top 9 bits are shaved off.
    uint64_t pktnum = tmcounter >> 11;
    tmcounter = tmcounter & 0xFFFFFFFFFFLL;
    
    // Now insert the fake spead header from the template
	memcpy(b->data,sphead,sizeof(sphead));

    byte_swap_spead_header(b);    
    // Index to the start of the pointer table
    ItemPointer *hdr_ptr = (ItemPointer *)&b->data[sizeof(SPEAD_HEADER)];
    hdr_ptr[0].item_address = pktnum;                ///< 40 bit HEAP_COUNTER_ID field
    hdr_ptr[3].item_address = 8192;                  ///< PAYLOAD_OFFSET_ID
    hdr_ptr[4].item_address = tmcounter;             ///< Lower 40 bits of FPGA counter TIME_STAMP_ID     
    hdr_ptr[6].item_address = status_bits;           ///< SPECTRUM_PER_INTEGRATION_ID ?
    
    b->packet_size = 8192 + 72; // 8202 - sizeof(int64_t) + 
        
}

/// Initialize the UDP socket connection
int vegas_udp_init(struct vegas_udp_params *p) {

    /* Resolve sender hostname */
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    int rv = getaddrinfo(p->sender, NULL, &hints, &result);
    if (rv!=0) { 
        vegas_error("vegas_udp_init", "getaddrinfo failed");
        return(VEGAS_ERR_SYS);
    }

    /* Set up socket */
    p->sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (p->sock==-1) { 
        vegas_error("vegas_udp_init", "socket error");
        freeaddrinfo(result);
        return(VEGAS_ERR_SYS);
    }

    /* bind to local address */
    struct sockaddr_in local_ip;
    local_ip.sin_family =  AF_INET;
    local_ip.sin_port = htons(p->port);
    local_ip.sin_addr.s_addr = INADDR_ANY;
    rv = bind(p->sock, (struct sockaddr *)&local_ip, sizeof(local_ip));
    if (rv==-1) {
        vegas_error("vegas_udp_init", "bind");
        return(VEGAS_ERR_SYS);
    }

    /* Set up socket to recv only from sender */
    for (rp=result; rp!=NULL; rp=rp->ai_next) {
        if (connect(p->sock, rp->ai_addr, rp->ai_addrlen)==0) { break; }
    }
    if (rp==NULL) { 
        vegas_error("vegas_udp_init", "connect error");
        close(p->sock); 
        freeaddrinfo(result);
        return(VEGAS_ERR_SYS);
    }
    memcpy(&p->sender_addr, rp, sizeof(struct addrinfo));
    freeaddrinfo(result);

    /* Non-blocking recv */
    fcntl(p->sock, F_SETFL, O_NONBLOCK);

    /* Increase recv buffer for this sock */
    int bufsize = 128*1024*1024;
    socklen_t ss = sizeof(int);
    rv = setsockopt(p->sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(int));
    if (rv<0) { 
        vegas_error("vegas_udp_init", "Error setting rcvbuf size.");
        perror("setsockopt");
    } 
    rv = getsockopt(p->sock, SOL_SOCKET, SO_RCVBUF, &bufsize, &ss); 
    if (0 && rv==0) { 
        printf("vegas_udp_init: SO_RCVBUF=%d\n", bufsize);
    }

    /* Poll command */
    p->pfd.fd = p->sock;
    p->pfd.events = POLLIN;
    
    return(VEGAS_OK);
}

/// Wait for a UDP network packet. Times out once a second.
/// @return { VEGAS_OK, VEGAS_TIMEOUT, VEGAS_ERR_SYS }
int vegas_udp_wait(struct vegas_udp_params *p) {
    int rv = poll(&p->pfd, 1, 1000); /* Timeout 1sec */
    if (rv==1) { 
        return(VEGAS_OK); /* Data ready */
    } 
    else if (rv==0) { 
        return(VEGAS_TIMEOUT); /* Timed out */
    } 
    else { 
        /* EINTR is not actually an error */
        if (errno == EINTR) {
            printf("Got interrupted system call (EINTR)... continuing\n");
            return(VEGAS_TIMEOUT);
        }
        return(VEGAS_ERR_SYS); /* Other error */
    }  
}

/// Receives the network packet and processes the packet filling the udp_packet structure.
/// The resulting packet has a SPEAD header, and the item table is in host byte order
int vegas_udp_recv(struct vegas_udp_params *p, struct vegas_udp_packet *b, char bw_mode[]) 
{
    int rv = 0;
    int hbw = (strncmp(bw_mode, "high", 4) == 0);
    if (hbw) /* high bandwidth mode */
    {
        rv = recv(p->sock, b->data, VEGAS_MAX_PACKET_SIZE, 0);
    }
    else    /* lbw */
    {
        // Copy the packet into the databuffer offset so that we don't need to recopy
        // the data later. A 72 byte SPEAD header will be prepended, so we leave
        // space for that, but we need to offset backwards to allow for the 16 byte LBW header
        // off the wire. Bottom line is that real data should land at correct offset
        rv = recv(p->sock, &b->data[sizeof(sphead) - sizeof(uint64_t) * 2], VEGAS_MAX_PACKET_SIZE, 0);
        if (8208 != rv) /* sanity check */
        {
            return VEGAS_ERR_PACKET;
        }
    }
    // record the actual packet length received
    b->packet_size = rv;
    
    if (rv==-1) 
    { 
        // error receiving packet
        return(VEGAS_ERR_SYS); 
    }
    else if (p->packet_size) 
    {
        // expected packet size is non-zero -- good. Are we expecting SPEAD packets?
        // If not the else cases below return an error, as the next stages
        // expect SPEAD packets.
    	if (strncmp(p->packet_format, "SPEAD", 5) == 0)
	    {
            int32_t is_ok;
    	    if (!hbw)    /* only for lbw */
	        {
                // Insert fake spead header and byte swap the item table to host order
                // since we synthesize the header, this should never fail
                is_ok = VEGAS_OK;
                lbw_packet_to_host_spead(b);                
    	    }
            else
            {
                // In HBW mode the spead header is already there, so just byte swap the item table
                is_ok = byte_swap_spead_header(b);
            }
            if (is_ok < 0)
                return(VEGAS_ERR_PACKET);
            else 
                return vegas_chk_spead_pkt_size(b);
        }
		else if (rv!=p->packet_size)
			return(VEGAS_ERR_PACKET);
		else
        {
            // invalid mode
            return(VEGAS_ERR_PACKET);
            // return(VEGAS_OK);
        }
    } else { 
        // expecting zero length packets ??
        return(VEGAS_ERR_PACKET);
    }
}

/// Byte swap a 64 bit value
unsigned long long change_endian64(const unsigned long long *d) 
{
#if 0
    // painful and unecessary
    unsigned long long tmp;
    char *in=(char *)d, *out=(char *)&tmp;
    int i;
    for (i=0; i<8; i++) {
        out[i] = in[7-i];
    }
    return(tmp);
#else
    return be64toh(*d);
#endif 
}

/// @defgroup GUPPI ''GUPPI style (non-spead format) processing routines.''
/// These appear to all be guppi 1SFA style packet related functions. Since
/// the LBW packets fake a SPEAD header, different methods should
/// apply here. Just in case we ifdef them out to prevent mistakes.
// @{
#ifndef SPEAD

unsigned long long vegas_udp_packet_seq_num(const struct vegas_udp_packet *p) {
    // XXX Temp for new baseband mode, blank out top 8 bits which 
    // contain channel info.

    unsigned long long tmp = change_endian64((unsigned long long *)p->data);
    tmp &= 0x00FFFFFFFFFFFFFF;
    return(tmp);
    //return(change_endian64((unsigned long long *)(p->data)));
}


#define PACKET_SIZE_ORIG ((size_t)8208)
#define PACKET_SIZE_SHORT ((size_t)544)
#define PACKET_SIZE_1SFA ((size_t)8224)
#define PACKET_SIZE_1SFA_OLD ((size_t)8160)
#define PACKET_SIZE_FAST4K ((size_t)4128)
#define PACKET_SIZE_PASP ((size_t)528)
#define PACKET_SIZE_SPEAD ((size_t)8248)

size_t vegas_udp_packet_datasize(size_t packet_size) {
    /* Special case for the new "1SFA" packets, which have an extra
     * 16 bytes at the end reserved for future use.  All other vegas
     * packets have 8 bytes index at the front, and 8 bytes error
     * flags at the end.
     * NOTE: This represents the "full" packet output size...
     */
    if (packet_size==PACKET_SIZE_1SFA) // 1SFA packet size
        return((size_t)8192);
    else if (packet_size == 8208)
        return ((size_t)8192); // simple 8K
    else if (packet_size==PACKET_SIZE_SHORT) 
        //return((size_t)256);
        return((size_t)512);
	else if (packet_size==PACKET_SIZE_SPEAD)
		return(packet_size - 6*8); // 8248-6*8
    else              
        return(packet_size - 2*sizeof(unsigned long long));
}


char *vegas_udp_packet_data(const struct vegas_udp_packet *p) {
    /* This is valid for all vegas packet formats
     * PASP has 16 bytes of header rather than 8.
     */
    if (p->packet_size==PACKET_SIZE_PASP)
        return((char *)(p->data) + (size_t)16);
    return((char *)(p->data) + sizeof(unsigned long long));
}

unsigned long long vegas_udp_packet_flags(const struct vegas_udp_packet *p) {
    return(*(unsigned long long *)((char *)(p->data) 
                + p->packet_size - sizeof(unsigned long long)));
}

/** Copy the data portion of a vegas udp packet to the given output
 * address.  This function takes care of expanding out the 
 * "missing" channels in 1SFA packets.
 */
void vegas_udp_packet_data_copy(char *out, const struct vegas_udp_packet *p) {
    if (p->packet_size==PACKET_SIZE_1SFA_OLD) {
        /* Expand out, leaving space for missing data.  So far only 
         * need to deal with 4k-channel case of 2 spectra per packet.
         * May need to be updated in the future if 1SFA works with 
         * different numbers of channels.
         *
         * TODO: Update 5/12/2009, newer 1SFA modes always will have full 
         * data contents, and the old 4k ones never really worked, so
         * this code can probably be deleted.
         */
        const size_t pad = 16;
        const size_t spec_data_size = 4096 - 2*pad;
        memset(out, 0, pad);
        memcpy(out + pad, vegas_udp_packet_data(p), spec_data_size);
        memset(out + pad + spec_data_size, 0, 2*pad);
        memcpy(out + pad + spec_data_size + pad + pad, 
                vegas_udp_packet_data(p) + spec_data_size, 
                spec_data_size);
        memset(out + pad + spec_data_size + pad
                + pad + spec_data_size, 0, pad);
    } else {
        /* Packet has full data, just do a memcpy */
        memcpy(out, vegas_udp_packet_data(p), 
                vegas_udp_packet_datasize(p->packet_size));
    }
}

/** Copy function for baseband data that does a partial
 * corner turn (or transpose) based on nchan.  In this case
 * out should point to the beginning of the data buffer.
 * block_pkt_idx is the seq number of this packet relative
 * to the beginning of the block.  packets_per_block
 * is the total number of packets per data block (all channels).
 */
void vegas_udp_packet_data_copy_transpose(char *databuf, int nchan,
        unsigned block_pkt_idx, unsigned packets_per_block,
        const struct vegas_udp_packet *p) {
    const unsigned chan_per_packet = nchan;
    const size_t bytes_per_sample = 4;
    const unsigned samp_per_packet = vegas_udp_packet_datasize(p->packet_size) 
        / bytes_per_sample / chan_per_packet;
    const unsigned samp_per_block = packets_per_block * samp_per_packet;

    char *iptr, *optr;
    unsigned isamp,ichan;
    iptr = vegas_udp_packet_data(p);

    for (isamp=0; isamp<samp_per_packet; isamp++) {
        optr = databuf + bytes_per_sample * (block_pkt_idx*samp_per_packet 
                + isamp);
        for (ichan=0; ichan<chan_per_packet; ichan++) {
            memcpy(optr, iptr, bytes_per_sample);
            iptr += bytes_per_sample;
            optr += bytes_per_sample*samp_per_block;
        }
    }

#if 0 
    // Old version...
    const unsigned pkt_idx = block_pkt_idx / nchan;
    const unsigned ichan = block_pkt_idx % nchan;
    const unsigned offset = ichan * packets_per_block / nchan + pkt_idx;
    memcpy(databuf + offset*vegas_udp_packet_datasize(p->packet_size), 
            vegas_udp_packet_data(p),
            vegas_udp_packet_datasize(p->packet_size));
#endif
}

size_t parkes_udp_packet_datasize(size_t packet_size) {
    return(packet_size - sizeof(unsigned long long));
}

void parkes_to_vegas(struct vegas_udp_packet *b, const int acc_len, 
        const int npol, const int nchan) {

    /* Convert IBOB clock count to packet count.
     * This assumes 2 samples per IBOB clock, and that
     * acc_len is the actual accumulation length (=reg_acclen+1).
     */
    const unsigned int counts_per_packet = (nchan/2) * acc_len;
    unsigned long long *packet_idx = (unsigned long long *)b->data;
    (*packet_idx) = change_endian64(packet_idx);
    (*packet_idx) /= counts_per_packet;
    (*packet_idx) = change_endian64(packet_idx);

    /* Reorder from the 2-pol Parkes ordering */
    int i;
    char tmp[VEGAS_MAX_PACKET_SIZE];
    char *pol0, *pol1, *pol2, *pol3, *in;
    in = b->data + sizeof(long long);
    if (npol==2) {
        pol0 = &tmp[0];
        pol1 = &tmp[nchan];
        for (i=0; i<nchan/2; i++) {
            /* Each loop handles 2 values from each pol */
            memcpy(pol0, in, 2*sizeof(char));
            memcpy(pol1, &in[2], 2*sizeof(char));
            pol0 += 2;
            pol1 += 2;
            in += 4;
        }
    } else if (npol==4) {
        pol0 = &tmp[0];
        pol1 = &tmp[nchan];
        pol2 = &tmp[2*nchan];
        pol3 = &tmp[3*nchan];
        for (i=0; i<nchan; i++) {
            /* Each loop handles one sample */
            *pol0 = *in; in++; pol0++;
            *pol1 = *in; in++; pol1++;
            *pol2 = *in; in++; pol2++;
            *pol3 = *in; in++; pol3++;
        }
    }
    memcpy(b->data + sizeof(long long), tmp, sizeof(char) * npol * nchan);
}
#endif
// @}

#ifdef SPEAD

/* Check that the size of the received SPEAD packet is correct.
 * This is acheived by reading the size fields in the SPEAD packet header,
 * and comparing them to the actual size of the received packet. */
int vegas_chk_spead_pkt_size(const struct vegas_udp_packet *p)
{
	unsigned int spead_hdr_upr = 0x53040305;
	int num_items, payload_size;
    int i;

    //Confirm we have enough bytes for header + 3 fields
    if(p->packet_size < 8*4)
    {
        printf("packet size less than 32 bytes\n");
        return (VEGAS_ERR_PACKET);
    }
    
    //Check that the header is valid
    if( BYTE_ARR_TO_UINT(p->data, 0) != spead_hdr_upr )
    {
        printf("Spead header missing\n");
        return (VEGAS_ERR_PACKET);
    }

    VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data;
    //Get number of items from the header
    num_items = (int)num_spead_items(sptr);

    payload_size = -1;

    //Get packet payload length, by searching through the fields
    for(i = 0; i<num_items; ++i)
    {
        //If we found the packet payload length item    
        if (sptr->items[i].item_identifier == PAYLOAD_OFFSET_ID)
        {
            payload_size = sptr->items[i].item_address;
            break;
        }
    }
    
    if(payload_size == -1)
    {
        printf("payload offset not found\n");
        return (VEGAS_ERR_PACKET);
    }

    //Confirm that packet size is correct
    if(p->packet_size != sizeof(SPEAD_HEADER) + num_items*sizeof(ItemPointer) + payload_size)
    {
        printf("packet_size does not match sum of header and payload\n");
        printf("packet_size=%ld, expected %ld, payloadsize=%d, nitems=%d\n",
               p->packet_size, sizeof(SPEAD_HEADER) + num_items*sizeof(ItemPointer) + payload_size,
               payload_size, num_items);
        return (VEGAS_ERR_PACKET);
    }
        
    // Confirm the data_size is correct
    if ((p->packet_size - (sizeof(SPEAD_HEADER) + num_items*sizeof(ItemPointer))) != vegas_spead_packet_datasize(p))
    {
        printf("VEGAS_ERR_PACKET %s, %d\n", __FILE__, __LINE__);
        return (VEGAS_ERR_PACKET);
    }
    return (VEGAS_OK);
}

unsigned int vegas_spead_packet_heap_cntr(const struct vegas_udp_packet *p)
{
    const VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data;
    //Get number of items from the header (num_items always in BE order)
    uint32_t i, num_items = num_spead_items(sptr);
    
    //Get heap counter, by searching through the fields
    for(i = 0; i<num_items; ++i)
    {
        //If we found the packet payload length item    
        if (sptr->items[i].item_identifier == HEAP_COUNTER_ID)
        {
            return ((uint32_t)sptr->items[i].item_address);
        }
    }
    
    return (VEGAS_ERR_PACKET);
}


unsigned int vegas_spead_packet_heap_offset(const struct vegas_udp_packet *p)
{
    const VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data;
    //Get number of items from the header (num_items always in BE order)
    uint32_t i, num_items = num_spead_items(sptr);

    //Get heap offset, by searching through the fields
    for(i = 0; i<num_items; ++i)
    {
        //If we found the packet payload length item    
        if (sptr->items[i].item_identifier == HEAP_OFFSET_ID)
        {
            return ((uint32_t)sptr->items[i].item_address);
        }
    }
    
    return (VEGAS_ERR_PACKET);
}


unsigned int vegas_spead_packet_seq_num(int heap_cntr, int heap_offset, int packets_per_heap)
{
    return (heap_cntr * packets_per_heap) + (heap_offset / PAYLOAD_SIZE);
}


/// Return a pointer to the begining of the payload, accounting for
/// variable length SPEAD headers
char* vegas_spead_packet_data(const struct vegas_udp_packet *p)
{
    VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data;
    size_t data_offset = sizeof(SPEAD_HEADER) + num_spead_items(sptr)*sizeof(ItemPointer);
    return (char*)(p->data + data_offset);
}


/// Find the size of the data in this packet, omitting header bytes
unsigned int vegas_spead_packet_datasize(const struct vegas_udp_packet *p)
{
    VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data;   
    return p->packet_size - sizeof(SPEAD_HEADER) - num_spead_items(sptr)*sizeof(ItemPointer);
}


int vegas_spead_packet_copy(struct vegas_udp_packet *p, char *header_addr,
                            char* payload_addr, char bw_mode[])
{
    char* pkt_payload;
    int payload_size, offset;
    int hbw = (strncmp(bw_mode, "high", 4) == 0);
    VegasSpeadPacketHeader *sptr = (VegasSpeadPacketHeader *)p->data; 
    // ItemPointer *hdr_items = (ItemPointer *)header_addr;
    spead_heap_entry *sheap =  (spead_heap_entry *)header_addr; 
    uint32_t i, num_items = num_spead_items(sptr);
  
    /* Copy header. No reversing of byte order is necessary 
     * as the packet header is now in host order. Some convertion is
     * necessary as the spead_heap fields dont exactly match the field
     * widths in the spead packet header.
     */
              
    for(i = 0; i < num_items-4; i++)
    {   
        // If this is the spec per integration item, add 1 in hbw mode        
        if (hbw && sptr->items[i+4].item_identifier == SPECTRUM_PER_INTEGRATION_ID)
            ++sptr->items[i+4].item_address;

        // Convert the address mode bit into a value seen as a byte:       
        sheap[i].addr_mode = (uint8_t) sptr->items[i+4].item_address_mode ? 0x80 : 0x0;
        // Get the lower 16 bits of the id. 
        // Note this truncates the 23 bit identifier field to 16 bits
        sheap[i].item_id   = (uint16_t)sptr->items[i+4].item_identifier;
        // split the 40 bit item into lower and upper portions
        sheap[i].item_lower32 = (uint32_t)sptr->items[i+4].item_address;
        sheap[i].item_top8  = (uint8_t)(sptr->items[i+4].item_address >> 32);

    }
    

    /* Copy payload */
    pkt_payload  = vegas_spead_packet_data(p);
    payload_size = vegas_spead_packet_datasize(p);

    /* If high-bandwidth mode, byte swap the int32_t data into little endian form. */
    if(hbw)
    {
        for(offset = 0; offset < payload_size; offset += 4)
        {
            *(unsigned int *)(payload_addr + offset) =
                ntohl(*(unsigned int *)(pkt_payload + offset));
        }
    }
    
    /* Else if low-bandwidth mode */
    else if(strncmp(bw_mode, "low", 3) == 0)
        memcpy(payload_addr, pkt_payload, payload_size);

    return 0;
}

#endif // endif SPEAD

/// Close the UDP socket
int vegas_udp_close(struct vegas_udp_params *p) {
    close(p->sock);
    return(VEGAS_OK);
}
