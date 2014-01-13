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

/******************************************************************************

The high bandwidth (HBW) mode of operation produces an input to the 
FITS writer (i.e this function) in the following form (per phase):
struct HBW
{
    float hbw[CHAN][1][STOKES];
};

Which is equivalent to:
struct HBW
{
    float hbw[CHAN][STOKES];
};

The FITS writer definition requires data in the form:
struct FITS_Input
{
    float input[SUBBAND][STOKES][CHAN];
};

For the HBW case there is only one-subband, so the reformmatting is 
achieved through a tiled 4x4 transpose.



For the 8 subband LBW case, the GPU output takes the following form:
struct L8LBW8_GPU_output
{
    float output[CHAN][STOKES][SUBBAND];
};

So here we need a 'hopping' transpose which selects the four stokes
values for a given subband and then proceeds with the transpose as in
the single band case. The annotations indicate where the 4 xmm registers
grab data for the transpose.

Input. Lines have consectutive addresses.
CH0S0P0,CH0S0P1,CH0S0P2,CH0S0P3,  <-- xmm0  Input stride=NSTOKES*NSUBBAND
CH0S1P0,CH0S1P1,CH0S1P2,CH0S1P3,
CH0S2P0,CH0S2P1,CH0S2P2,CH0S2P3,
CH0S3P0,CH0S3P1,CH0S3P2,CH0S3P3,
CH0S4P0,CH0S4P1,CH0S4P2,CH0S4P3,
CH0S5P0,CH0S5P1,CH0S5P2,CH0S5P3,
CH0S6P0,CH0S6P1,CH0S6P2,CH0S6P3,
CH0S7P0,CH0S7P1,CH0S7P2,CH0S7P3,

CH1S0P0,CH1S0P1,CH1S0P2,CH1S0P3,  <-- xmm1
CH1S1P0,CH1S1P1,CH1S1P2,CH1S1P3,
CH1S2P0,CH1S2P1,CH1S2P2,CH1S2P3,
CH1S3P0,CH1S3P1,CH1S3P2,CH1S3P3,
CH1S4P0,CH1S4P1,CH1S4P2,CH1S4P3,
CH1S5P0,CH1S5P1,CH1S5P2,CH1S5P3,
CH1S6P0,CH1S6P1,CH1S6P2,CH1S6P3,
CH1S7P0,CH1S7P1,CH1S7P2,CH1S7P3,

CH2S0P0,CH2S0P1,CH2S0P2,CH2S0P3,  <-- xmm2
CH2S1P0,CH2S1P1,CH2S1P2,CH2S1P3,
CH2S2P0,CH2S2P1,CH2S2P2,CH2S2P3,
CH2S3P0,CH2S3P1,CH2S3P2,CH2S3P3,
CH2S4P0,CH2S4P1,CH2S4P2,CH2S4P3,
CH2S5P0,CH2S5P1,CH2S5P2,CH2S5P3,
CH2S6P0,CH2S6P1,CH2S6P2,CH2S6P3,
CH2S7P0,CH2S7P1,CH2S7P2,CH2S7P3,

CH3S0P0,CH3S0P1,CH3S0P2,CH3S0P3,  <-- xmm3
CH3S1P0,CH3S1P1,CH3S1P2,CH3S1P3,
CH3S2P0,CH3S2P1,CH3S2P2,CH3S2P3,
CH3S3P0,CH3S3P1,CH3S3P2,CH3S3P3,
CH3S4P0,CH3S4P1,CH3S4P2,CH3S4P3,
CH3S5P0,CH3S5P1,CH3S5P2,CH3S5P3,
CH3S6P0,CH3S6P1,CH3S6P2,CH3S6P3,
CH3S7P0,CH3S7P1,CH3S7P2,CH3S7P3,

8 Band Transpose output:
CH0S0P0,CH1S0P0,CH2S0P0,CH3S0P0, ...
CH0S0P1,CH1S0P1,CH2S0P1,CH3S0P1, ...
CH0S0P2,CH1S0P2,CH2S0P2,CH3S0P2, ...
CH0S0P3,CH1S0P3,CH2S0P3,CH3S0P3, ...

CH0S1P0,CH1S1P0,CH2S1P0,CH3S1P0, ...
CH0S1P0,CH1S1P0,CH2S1P0,CH3S1P0, ...
CH0S1P1,CH1S1P1,CH2S1P1,CH3S1P1, ...
CH0S1P2,CH1S1P2,CH2S1P2,CH3S1P2, ...
CH0S1P3,CH1S1P3,CH2S1P3,CH3S1P3, ...

... x nsubband

CH0S4P0,CH1S4P1,CH2S4P2,CH3S4P3, ... x nchan
CH0S5P0,CH1S5P1,CH2S5P2,CH3S5P3, ...
CH0S6P0,CH1S6P1,CH2S6P2,CH3S6P3, ...
CH0S7P0,CH1S7P1,CH2S7P2,CH3S7P3, ...



So it seems ...

for (subband=0; subband<nsubbands; ++subband)
{
    for (ch=0; ch<nchan; ch+=4)
    {
        // xmm0 case
        xmm0 <- in[(ch+0) * nsubband * nstokes + subband * nstokes];
        // xmm1 case
        xmm1 <- in[(ch+1) * nsubband * nstokes + subband * nstokes];
        // xmm2 case
        xmm2 <- in[(ch+2) * nsubband * nstokes + subband * nstokes];
        // xmm3 case
        xmm3 <- in[(ch+3) * nsubband * nstokes + subband * nstokes];
        
        out[subband *  nstokes * nchan + 0*nchan + ch] <- xmm0
        out[subband *  nstokes * nchan + 1*nchan + ch] <- xmm1
        out[subband *  nstokes * nchan + 1*nchan + ch] <- xmm2
        out[subband *  nstokes * nchan + 1*nchan + ch] <- xmm3
    }
}

// The use of nstokes is really a misnomer. It just happens to be 4,
// the same dimension as the number of floats in an xmm register.

******************************************************************************/


#define restrict __restrict__

#ifdef __SSE__ 
#define REGS __m128 row0,row1,row2,row3 
#else
#define REGS 
#endif

#define UNALIGNED_LOAD \
                row0 = _mm_loadu_ps(&in[inidx+i_stride*0]);\
                row1 = _mm_loadu_ps(&in[inidx+i_stride*1]);\
                row2 = _mm_loadu_ps(&in[inidx+i_stride*2]);\
                row3 = _mm_loadu_ps(&in[inidx+i_stride*3])
    
#define ALIGNED_LOAD \
                row0 = _mm_load_ps(&in[inidx+i_stride*0]);\
                row1 = _mm_load_ps(&in[inidx+i_stride*1]);\
                row2 = _mm_load_ps(&in[inidx+i_stride*2]);\
                row3 = _mm_load_ps(&in[inidx+i_stride*3])

#define UNALIGNED_STORE \
                _mm_storeu_ps(&out[outidx+o_stride*0], row0);\
                _mm_storeu_ps(&out[outidx+o_stride*1], row1);\
                _mm_storeu_ps(&out[outidx+o_stride*2], row2);\
                _mm_storeu_ps(&out[outidx+o_stride*3], row3)

#define ALIGNED_STORE \
                _mm_store_ps(&out[outidx+o_stride*0], row0);\
                _mm_store_ps(&out[outidx+o_stride*1], row1);\
                _mm_store_ps(&out[outidx+o_stride*2], row2);\
                _mm_store_ps(&out[outidx+o_stride*3], row3)

#define BEGIN_LOOP \
    for(int subband = 0; subband < nsubbands; ++subband)\
    {\
        /* These are the distances between addresses for each xmm reg */ \
        const int i_stride = nsubbands*nstokes; \
        const int o_stride = nchannels; \
        for (int channel=0; channel < nchannels; channel += 4) \
        {\
            REGS; \
            int inidx    = channel*nsubbands*nstokes+subband*nstokes; \
            int outidx   = subband*nchannels*nstokes+channel

            

#define END_LOOP \
        }\
    }

/**
 * Transpose a 4x4 block of floats from aligned (i.e. address is a multiple of 16 bytes) or 
 * unaligned. There are four cases and we don't want conditionals in our loops so
 * the macros above unroll the code in each case. BUBF (butt-ugly-but-fast).
 *
 * Note: The routine does not protect against nchannels values which would
 * produce un-aligned accesses. Also note that -O3 optimization option is required
 * to allow full xmm register use. Omission of this option produces code which spills
 * almost every access back to memory (broken IMHO).
 */

void transpose(float * restrict in, float * restrict out, int nsubbands, int nchannels)
{
    const int nstokes = 4;

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
            out[outidx+o_stride*0+0] = in[inidx+i_stride*0+0];
            out[outidx+o_stride*0+1] = in[inidx+i_stride*1+0];
            out[outidx+o_stride*0+2] = in[inidx+i_stride*2+0];
            out[outidx+o_stride*0+3] = in[inidx+i_stride*3+0];

            // XX -- XX XX //      // XX XX XX XX //
            // XX -- XX XX // ---\ // -- -- -- -- //
            // XX -- XX XX // ---/ // XX XX XX XX //
            // XX -- XX XX //      // XX XX XX XX //
            out[outidx+o_stride*1+0] = in[inidx+i_stride*0+1];
            out[outidx+o_stride*1+1] = in[inidx+i_stride*1+1];
            out[outidx+o_stride*1+2] = in[inidx+i_stride*2+1];
            out[outidx+o_stride*1+3] = in[inidx+i_stride*3+1];

            // XX XX -- XX //      // XX XX XX XX //
            // XX XX -- XX // ---\ // XX XX XX XX //
            // XX XX -- XX // ---/ // -- -- -- -- //
            // XX XX -- XX //      // XX XX XX XX //
            out[outidx+o_stride*2+0] = in[inidx+i_stride*0+2];
            out[outidx+o_stride*2+1] = in[inidx+i_stride*1+2];
            out[outidx+o_stride*2+2] = in[inidx+i_stride*2+2];
            out[outidx+o_stride*2+3] = in[inidx+i_stride*3+2];

            // XX XX XX -- //      // XX XX XX XX //
            // XX XX XX -- // ---\ // XX XX XX XX //
            // XX XX XX -- // ---/ // XX XX XX XX //
            // XX XX XX -- //      // -- -- -- -- //
            out[outidx+o_stride*3+0] = in[inidx+i_stride*0+3];
            out[outidx+o_stride*3+1] = in[inidx+i_stride*1+3];
            out[outidx+o_stride*3+2] = in[inidx+i_stride*2+3];
            out[outidx+o_stride*3+3] = in[inidx+i_stride*3+3];
    END_LOOP;
#endif//__SSE__
}
