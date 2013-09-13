//# Copyright (C) 2013 Associated Universities, Inc. Washington DC, USA.
//# 
//# This program is free software; you can redistribute it and/or modify
//# it under the terms of the GNU General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or
//# (at your option) any later version.
//# 
//# This program is distributed in the hope that it will be useful, but
//# WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//# General Public License for more details.
//# 
//# You should have received a copy of the GNU General Public License
//# along with this program; if not, write to the Free Software
//# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//# 
//# Correspondence concerning GBT software should be addressed as follows:
//#	GBT Operations
//#	National Radio Astronomy Observatory
//#	P. O. Box 2
//#	Green Bank, WV 24944-0002 USA

//# $Id$


#ifndef vegas_spead_packet_h
#define vegas_spead_packet_h

#include<stdint.h>

/// The first 8 bytes of the SPEAD header as viewed in little-endian format without byte swapping
/// Note: members wider than a single byte must be reorder for little-endian form when reading/writing
struct _SPEAD_HEADER                  
{
    uint8_t magic;
    uint8_t version;
    uint8_t pointer_width;
    uint8_t heap_width;
    uint16_t reserved;
    uint16_t num_items;
};
typedef struct _SPEAD_HEADER SPEAD_HEADER;

/// Item pointer entry in host (little-endian) format after item table byte swap
struct _ItemPointer
{
    unsigned long item_address:40; ///< 40 bits of spead item address
    unsigned item_identifier:23;   ///< 23 bit identifier
    unsigned item_address_mode:1;  ///<  1 bit flag defining immediate (1) or relative (0)
};
typedef struct _ItemPointer ItemPointer;
    

/// A structure overlay for working with the SPEAD header after it has
/// been changed to little endian (host) format.
struct _VegasSpeadPacketHeader
{
    SPEAD_HEADER spead_header; // 8 bytes
    ItemPointer  items[1];     ///< Item pointer table starts here
};

typedef struct _VegasSpeadPacketHeader VegasSpeadPacketHeader;

/// Definitions for the various SPEAD packet entries
/// The magic header (first 32 bits) in character and little-endian format
#define SPEAD_MAGIC_HEAD_CHAR 0x53,0x04,0x03,0x05
#define SPEAD_MAGIC_HEAD 0x53040305

/// SPEAD pointer table id's for typical SPEAD headers
#define HEAP_COUNTER_ID             0x1
#define HEAP_SIZE_ID                0x2
#define HEAP_OFFSET_ID              0x3
#define PAYLOAD_OFFSET_ID           0x4 
#define TIME_STAMP_ID               0x20
#define SPECTRUM_COUNTER_ID         0x21
#define SPECTRUM_PER_INTEGRATION_ID 0x22
#define MODE_NUMBER_ID              0x23
#define SWITCHING_STATE_ID          0x24
#define PAYLOAD_DATA_OFFSET_ID      0x25

/// Additional data structures for handling non-SPEAD packets (LBW)
/// Not sure where this is otherwise documented. The LBW header
/// appears to follow the convention:
/// +-----------------------------------------+
/// | unused:12 | status:4 | fpga_counter:48  |
/// +-----------------------------------------+
/// | unused:12 | status:4 | fpga_counter:48  |
/// +-----------------------------------------+
/// |  8192 bytes of data                     |
/// +-----------------------------------------+
/// The second 8 bytes is a duplicate of the first 8 bytes, and is
/// ignored by the HPC software.

/// The structure of a raw LBW packet header after byte swapping
struct _LBW_Packet
{
    uint64_t time_counter:48;
    uint8_t  status:8;        // only lower 4 bits are used
    uint8_t  notused:8;
};
typedef struct _LBW_Packet LBW_Packet;

/// A union to easly perform the header 8 byte-swap
union _LBW_endian
{
    LBW_Packet le;
    int64_t header;
};
typedef union _LBW_endian LBW_endian;

#endif
