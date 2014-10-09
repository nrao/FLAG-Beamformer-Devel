
#ifndef DataBlockInfoCache_h
#define DataBlockInfoCache_h
class DataBlockInfoCache
{
public:
    cpu_gpu_buf_index   _heap_idx[2 * MAX_HEAPS_PER_BLK];
    time_spead_heap     _heap_hdr[2 * MAX_HEAPS_PER_BLK];
    
    DataBlockInfoCache()
    {
        memset(_heap_idx, 0, sizeof(_heap_idx));
        memset(_heap_hdr, 0, sizeof(_heap_hdr));
    }
    /*
     Take an input block and cache its index and spead header info.
     This does a upper to lower shift prior to loading the upper half
     */
    void input(time_spead_heap *hdr_base, databuf_index *idx)
    {
        // shift upper to lower buffer
        memcpy(&_heap_hdr[0], &_heap_hdr[MAX_HEAPS_PER_BLK], MAX_HEAPS_PER_BLK * sizeof(time_spead_heap));
        memcpy(&_heap_idx[0], &_heap_idx[MAX_HEAPS_PER_BLK], MAX_HEAPS_PER_BLK * sizeof(cpu_gpu_buf_index)); 
        // store new input data in upper half
        memcpy(&_heap_hdr[MAX_HEAPS_PER_BLK], hdr_base, MAX_HEAPS_PER_BLK * sizeof(time_spead_heap));
        memcpy(&_heap_idx[MAX_HEAPS_PER_BLK], idx,      MAX_HEAPS_PER_BLK * sizeof(cpu_gpu_buf_index)); 
    }

    /* verify the next num_heaps of data is valid according to the index */
    int is_valid(int heap_start, int num_heaps)
    {
        for (int i = heap_start; i < (heap_start + num_heaps); ++i)
        {
            if (!_heap_idx[i].heap_valid)
            {
                return FALSE;
            }
        }
        return TRUE;
    }

/*
    A note about blanking:
    The blanking status is copied from the time series input into the array
    status_bits, with time acending with index, like so:
    status_bits[32] = t0
    status_bits[33] = t0 + dt
    status_bits[34] = t0 + dt + dt
    ...
    status_bits[N+32] = t0 + dt * N;
    
    So when we think about labeling the frequency heap outputs, the convention
    is to use the '1st' non-blanked time-series (e.g. index 32 above) to fill 
    in the timestamp, counter, mjd etc.

    However, when we think about how to process blanking, we need to use the
    most recent(e.g index N+32 above), status to drive the blanking state machine.
    Below, the check which sets 0x2 is taken from the most current time-series status.
*/

/*
 * Check the input time series for blanking and encode
 * the result.
 * Return value: 
 *  - bit 0x4 -- indicates cal or sig/ref state changed during input
 *  - bit 0x2 -- indicates if the most recent time sample had blanking asserted
 *  - bit 0x1 -- indicates if any of the time samples had blanking asserted
 */

    /* check the switching and blanking status */
    int is_blanked(int heap_start, int num_heaps)
    {
        int state_changed = 0;
        int banked_at_start = (_heap_hdr[heap_start + num_heaps- 1].status_bits & 0x8)  ? 0x2 : 0x0;
        int is_blanked = (banked_at_start || (_heap_hdr[heap_start].status_bits & 0x8)) ? 0x1 : 0x0;

        for (int i = heap_start + 1; i < (heap_start + num_heaps); ++i)
        {
            if ((_heap_hdr[i].status_bits & 0x3) != (_heap_hdr[i-1].status_bits & 0x3))
            {
                state_changed = 0x4;
            }
            if (_heap_hdr[i].status_bits & (BLANKING_BIT | SCAN_NOT_STARTED))
            {
                is_blanked = 0x1;
            }
        }
        return (banked_at_start | state_changed | is_blanked);  
    }
    int status(int heapidx)
    {
        return _heap_hdr[heapidx].status_bits;
    }
    double mjd(int heapidx)
    {
        return _heap_idx[heapidx].heap_rcvd_mjd;
    }
};


#endif
