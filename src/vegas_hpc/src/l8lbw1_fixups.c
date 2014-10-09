
#include <stdio.h>
#include "l8lbw1_fixups.h"

#if defined(SIMPLE_LONGWORD_SWAP)
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

#else

// New algorithm:
// Loop building 32M compressed (l8lbw1) blocks from l8lbw8 input
// When the l8lbw1 block is complete, send it to the gpu.
// The borders of complete l8lbw8 input and l8lbw1 output should
// always be in lock-step. (i.e an integral number of l8lbw8
// blocks should equal 1 l8lbw1 output block.
// 8 * l8lbw8 --> l8lbw1
// Note: Data is being compressed into the 1st data block to be
// sent to the GPU. The GPU will therefore get every 8th buffer.

// This uses L8LBW8 packets to form a L8LBW1 input block for the GPU
// Since the high channel modes need more data, eight input blocks
// are used to form a full gpu input block.
void fixup_l8lbw1_block_merge(struct vegas_databuf *db, int input_blks[8])
{
    struct time_spead_heap *l8_hdr;
    struct time_spead_heap *l1_hdr;
    struct time_spead_heap_packet_l8 *l8;
    struct time_spead_heap_packet_l1 *l1;    
    int s, out_heap, out_sample, heap;
    struct databuf_index *index_in, *index_out;
    int in_blk_idx;
    const int num_needed = 8;
        
    // We are collapsing 8 blocks into 1. The first block gets the results
    l1_hdr = (struct time_spead_heap *)vegas_databuf_data(db, input_blks[0]);
    index_out = (struct databuf_index*)vegas_databuf_index(db, input_blks[0]);
        
    l1 = (struct time_spead_heap_packet_l1 *)&l1_hdr[MAX_HEAPS_PER_BLK];
    
    out_heap = 0;
    out_sample = 0;
    
    // for each data block
    for (in_blk_idx=0; in_blk_idx<num_needed; ++in_blk_idx)
    {
        l8_hdr = (struct time_spead_heap *)vegas_databuf_data(db, input_blks[in_blk_idx]); 
        index_in = (struct databuf_index*)vegas_databuf_index(db, input_blks[in_blk_idx]);           
        l8 = (struct time_spead_heap_packet_l8 *)&l8_hdr[MAX_HEAPS_PER_BLK]; 

        // for each heap in the block    
        for (heap=0; heap<index_in->num_heaps; ++heap)
        {   
            // for each subband zero entry in the l8 packet           
            for (s=0; s<256; ++s)
            {
                l1[out_heap].data[out_sample++] = l8[heap].data[s].subband[0];
            }

            // As we process the heaps, every 8th heap we copy the header
            // into the output heap spead headers
            if (out_sample % 2048 == 0)
            {
                if (out_heap != 0)
                {
                    l1_hdr[out_heap] = l8_hdr[heap];
                }
                out_heap++;
                out_sample = 0;
            }
        }
    }
    // Record the new size of the (1st) input buffer
    index_out->num_heaps = out_heap;   
}

#endif
