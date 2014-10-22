#ifndef l8lbw1_fixups_h
#define l8lbw1_fixups_h

#include "vegas_databuf.h"
#include "spead_heap.h"

// Define this due to the l8lbw1 mode not working properly.
// Note that this code will not support the 512k channel modes!
struct cmplx_sample
{
    int8_t re;
    int8_t im;
};
struct time_sample
{
    struct cmplx_sample pol[2];
};

// represent a set of 8 subbands with 2 polarizations with complex values
struct l8_time_sample
{
    struct time_sample subband[8];
};

// represent the contents of a l8/lbw8 packet
// e.g p.data[time_sample].data[n].subband[0];
struct time_spead_heap_packet_l8
{
    struct l8_time_sample data[256];
};

// representation of a l8/lbw1 packet
struct time_spead_heap_packet_l1
{
    struct time_sample data[2048];
};

void fixup_l8lbw1_block(struct vegas_databuf *, int curblk);



#endif
