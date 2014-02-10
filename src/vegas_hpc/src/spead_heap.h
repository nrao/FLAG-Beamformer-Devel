#ifndef _SPEAD_HEAP_H_
#define _SPEAD_HEAP_H_

/// Bit definitions for the switching signal status field in host order
#define SIG_REF_BIT      (0x1)
#define CAL_BIT          (0x2)
#define ADV_SIG_REF_BIT  (0x4)
#define BLANKING_BIT     (0x8)
#define BLANKING_MASK    (0x8)
#define CAL_SR_MASK      (SIG_REF_BIT|CAL_BIT|ADV_SIG_REF_BIT)
#define SCAN_NOT_STARTED (0x10)


#pragma pack(push)
#pragma pack(1)

struct freq_spead_heap {
    unsigned char time_cntr_addr_mode;
    unsigned short time_cntr_id;
    unsigned char time_cntr_top8;
    unsigned int time_cntr;
    unsigned char spectrum_cntr_addr_mode;
    unsigned short spectrum_cntr_id;
    unsigned char pad1;
    unsigned int spectrum_cntr;
    unsigned char integ_size_addr_mode;
    unsigned short integ_size_id;
    unsigned char pad2;
    unsigned int integ_size;
    unsigned char mode_addr_mode;
    unsigned short mode_id;
    unsigned char pad3;
    unsigned int mode;
    unsigned char status_bits_addr_mode;
    unsigned short status_bits_id;
    unsigned char pad4;
    unsigned int status_bits;
    unsigned char payload_data_off_addr_mode;
    unsigned short payload_data_off_id;
    unsigned char pad5;
    unsigned int payload_data_off;
};

struct time_spead_heap {
    unsigned char time_cntr_addr_mode;
    unsigned short time_cntr_id;
    unsigned char time_cntr_top8;
    unsigned int time_cntr;
    unsigned char mode_addr_mode;
    unsigned short mode_id;
    unsigned char pad1;
    unsigned int mode;
    unsigned char status_bits_addr_mode;
    unsigned short status_bits_id;
    unsigned char pad2;
    unsigned int status_bits;
    unsigned char payload_data_off_addr_mode;
    unsigned short payload_data_off_id;
    unsigned char pad3;
    unsigned int payload_data_off;
};

/// A data structure to overlay one item in the time/freq_spead_heap structures
/// Used in vegas_spead_packet_copy()
struct _spead_heap_entry {
    unsigned char  addr_mode;
    unsigned short item_id;
    unsigned char  item_top8;
    unsigned int   item_lower32;
};
typedef struct _spead_heap_entry spead_heap_entry;

#pragma pack(pop)


#endif
