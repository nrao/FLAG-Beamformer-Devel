
#include "l8lbw1_fixups.h"

void fixup_l8lbw1_block(struct vegas_databuf *db, int curblock_in)
{
    // This code swaps each 32bit quantity to correct the L8LBW1 mode data
    struct time_spead_heap *in_hdr;
    struct time_spead_heap *out_hdr;
    struct time_spead_heap_packet_l1 *in;
    struct time_spead_heap_packet_l1 *out;    
    int s, heap;
    struct time_sample a, b;
    struct databuf_index *index_in;
    
    index_in = (struct databuf_index*)vegas_databuf_index(db, curblock_in);    
    in_hdr  = (struct time_spead_heap *)vegas_databuf_data(db, curblock_in);
    out_hdr = (struct time_spead_heap *)vegas_databuf_data(db, curblock_in);
    
    in  = (struct time_spead_heap_packet_l1 *)&in_hdr[MAX_HEAPS_PER_BLK];
    out = (struct time_spead_heap_packet_l1 *)&out_hdr[MAX_HEAPS_PER_BLK];
    
    // for each heap     
    for (heap=0; heap<index_in->num_heaps; ++heap)
    {
        // Perform a 32bit swap on the data
        for (s=0;s<2048;s+=2)
        {
            a = in[heap].data[s + 0];
            b = in[heap].data[s + 1];
            out[heap].data[s + 0] = b;
            out[heap].data[s + 1] = a;
        }      
    }            
}
