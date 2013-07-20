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

static char rcs_id[] =  "$Id$";

// Parent
#include "transpose.h"
// Other
#ifdef __SSE__
#include "xmmintrin.h"
#endif//__SSE__

#define restrict __restrict__

#ifdef __SSE__ 
#define REGS __m128 row0,row1,row2,row3 
#else
#define REGS 
#endif

#define UNALIGNED_LOAD \
                row0 = _mm_loadu_ps(&in[inidx+nstokes*0]);\
                row1 = _mm_loadu_ps(&in[inidx+nstokes*1]);\
                row2 = _mm_loadu_ps(&in[inidx+nstokes*2]);\
                row3 = _mm_loadu_ps(&in[inidx+nstokes*3])
    
#define ALIGNED_LOAD \
                row0 = _mm_load_ps(&in[inidx+nstokes*0]);\
                row1 = _mm_load_ps(&in[inidx+nstokes*1]);\
                row2 = _mm_load_ps(&in[inidx+nstokes*2]);\
                row3 = _mm_load_ps(&in[inidx+nstokes*3])

#define UNALIGNED_STORE \
                _mm_storeu_ps(&out[outidx+nchannels*0], row0);\
                _mm_storeu_ps(&out[outidx+nchannels*1], row1);\
                _mm_storeu_ps(&out[outidx+nchannels*2], row2);\
                _mm_storeu_ps(&out[outidx+nchannels*3], row3)

#define ALIGNED_STORE \
                _mm_store_ps(&out[outidx+nchannels*0], row0);\
                _mm_store_ps(&out[outidx+nchannels*1], row1);\
                _mm_store_ps(&out[outidx+nchannels*2], row2);\
                _mm_store_ps(&out[outidx+nchannels*3], row3)

#define BEGIN_LOOP \
    for(int subband = 0; subband < nsubbands; ++subband)\
    {\
        int nblocks = nchannels / 4;\
        int page = subband * nchannels * nstokes;\
        for(int block = 0; block < nblocks; ++block)\
        {\
            REGS; \
            int inidx = page + block*nstokes*4;\
            int outidx = page + block*4;\

#define END_LOOP \
        }\
    }

/**
 * Transpose a 4x4 block of floats from aligned (i.e. address is a multiple of 16 bytes) or 
 * unaligned. There are four cases and we don't want conditionals in our loops so
 * the macros above unroll the code in each case. BUBF (butt-ugly-but-fast).
 *
 * Note: The routine does not protect against nchannels and nstokes values which would
 * also produce un-aligned accesses.
 */

void transpose(float * restrict in, float * restrict out, int nsubbands, int nchannels, int nstokes)
{
    int alignment_case=0;
    enum Alignment { BOTH_ALIGNED, INPUT_UNALIGNED, OUTPUT_UNALIGNED, BOTH_UNALIGNED };

    if ((long)in & 0xFL)
        alignment_case|=0x1;
    if ((long)out & 0xFL)
        alignment_case|=0x2;

#ifdef __SSE__
    switch (alignment_case)
    { 
    case BOTH_ALIGNED:
        BEGIN_LOOP;
        ALIGNED_LOAD;
        _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        ALIGNED_STORE;
        END_LOOP;
    break;
    case OUTPUT_UNALIGNED:
        BEGIN_LOOP;
        ALIGNED_LOAD;
        _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        UNALIGNED_STORE;
        END_LOOP;
    break;
    case INPUT_UNALIGNED:
        BEGIN_LOOP;
        UNALIGNED_LOAD;
        _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        ALIGNED_STORE;
        END_LOOP;
    break;
    case BOTH_UNALIGNED:
        BEGIN_LOOP;
        UNALIGNED_LOAD;
        _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        UNALIGNED_STORE;
        END_LOOP;
    break;
    }

#else
    BEGIN_LOOP;
            // If the SSE intrinsics aren't available,
            // do it the old fashioned way

            // -- XX XX XX //      // -- -- -- -- //
            // -- XX XX XX // ---\ // XX XX XX XX //
            // -- XX XX XX // ---/ // XX XX XX XX //
            // -- XX XX XX //      // XX XX XX XX //
            out[outidx+nchannels*0+0] = in[inidx+nstokes*0+0];
            out[outidx+nchannels*0+1] = in[inidx+nstokes*1+0];
            out[outidx+nchannels*0+2] = in[inidx+nstokes*2+0];
            out[outidx+nchannels*0+3] = in[inidx+nstokes*3+0];

            // XX -- XX XX //      // XX XX XX XX //
            // XX -- XX XX // ---\ // -- -- -- -- //
            // XX -- XX XX // ---/ // XX XX XX XX //
            // XX -- XX XX //      // XX XX XX XX //
            out[outidx+nchannels*1+0] = in[inidx+nstokes*0+1];
            out[outidx+nchannels*1+1] = in[inidx+nstokes*1+1];
            out[outidx+nchannels*1+2] = in[inidx+nstokes*2+1];
            out[outidx+nchannels*1+3] = in[inidx+nstokes*3+1];

            // XX XX -- XX //      // XX XX XX XX //
            // XX XX -- XX // ---\ // XX XX XX XX //
            // XX XX -- XX // ---/ // -- -- -- -- //
            // XX XX -- XX //      // XX XX XX XX //
            out[outidx+nchannels*2+0] = in[inidx+nstokes*0+2];
            out[outidx+nchannels*2+1] = in[inidx+nstokes*1+2];
            out[outidx+nchannels*2+2] = in[inidx+nstokes*2+2];
            out[outidx+nchannels*2+3] = in[inidx+nstokes*3+2];

            // XX XX XX -- //      // XX XX XX XX //
            // XX XX XX -- // ---\ // XX XX XX XX //
            // XX XX XX -- // ---/ // XX XX XX XX //
            // XX XX XX -- //      // -- -- -- -- //
            out[outidx+nchannels*3+0] = in[inidx+nstokes*0+3];
            out[outidx+nchannels*3+1] = in[inidx+nstokes*1+3];
            out[outidx+nchannels*3+2] = in[inidx+nstokes*2+3];
            out[outidx+nchannels*3+3] = in[inidx+nstokes*3+3];
    END_LOOP;
#endif//__SSE__
}
