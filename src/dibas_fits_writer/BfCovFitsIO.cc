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
//# GBT Operations
//# National Radio Astronomy Observatory
//# P. O. Box 2
//# Green Bank, WV 24944-0002 USA

#include <stdio.h>
#include "assert.h"

#include "BfCovFitsIO.h"
BfCovFitsIO::BfCovFitsIO(const char *path_prefix, int simulator, int instance_id) : BfFitsIO(path_prefix, simulator, instance_id)
{
    // What distinquishes modes is their data format
    data_size = FITS_BIN_SIZE * NUM_CHANNELS;
    sprintf(data_form, "%dC", data_size);
}

// example implementation of abstract method
int BfCovFitsIO::myAbstract() {
    return 1;
}    



// covariance data coming out of GPU has tons of zeros and some
// redundant values that need to be purged first
int BfCovFitsIO::write_HI(int mcnt, float *data) {
        data_size = FITS_BIN_SIZE * NUM_CHANNELS;
        float fits_matrix[NUM_CHANNELS * FITS_BIN_SIZE * 2];
        printf("about to parse data\n");
        parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS);
        printf("about to write parsed data\n");
        writeRow(mcnt, fits_matrix);
        printf("done writing data\n");
        return 1;
}    

int BfCovFitsIO::write_PAF(int mcnt, float *data) {
        float fits_matrix[NUM_CHANNELS_PAF * FITS_BIN_SIZE * 2];
        data_size = FITS_BIN_SIZE * NUM_CHANNELS_PAF;
        printf("about to parse data\n");
        parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS_PAF);
        printf("about to write parsed data\n");
        writeRow(mcnt, fits_matrix);
        printf("done writing data\n");
        return 1;
}

int BfCovFitsIO::write_FRB(int mcnt, float *data) {
        float fits_matrix[NUM_CHANNELS_FRB * FITS_BIN_SIZE * 2];
        data_size = FITS_BIN_SIZE * NUM_CHANNELS_FRB;
        printf("about to parse data\n");
        parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS_FRB);
        printf("about to write parsed data\n");
        writeRow(mcnt, fits_matrix);
        printf("done writing data\n");
        return 1;
} 


int BfCovFitsIO::write(int mcnt, float *data) {
// For a matrix of 40x40 there will be 20 redundant values
   float fits_matrix[NUM_CHANNELS * FITS_BIN_SIZE * 2];
   printf("about to parse data\n");
   return 1;
}


void
BfCovFitsIO::parseAndReorderGpuCovMatrix(float const *const gpu_matrix, int gpu_corr_num, float *const fits_matrix, int fits_corr_num, int num_channels)
{
        int gpuToNativeMap[820];
        FILE *fptr= NULL;
        fptr = fopen("/users/npingel/FLAG/bf/repos/FLAG-Beamformer-Devel/docs/gpuToNativeMap.dat","rb");
        for (int i=0; i < 820; i++)
        {
        fscanf(fptr,"%d",&gpuToNativeMap[i]);
         }
        fclose(fptr);
        for (int z = 0; z < num_channels; z++)
        {
            int fits_pos = z*fits_corr_num*2;
            for (int j = 0; j < fits_corr_num; j++)
                {
                        int gpu_pos = z*gpu_corr_num*2;
                        int gpu_idx_real = gpuToNativeMap[j]*2;
                        int corr_idx_real = gpu_pos+gpu_idx_real;
                        int corr_idx_imag = corr_idx_real+1;
                        int fits_idx_real = fits_pos+(j*2);
                        int fits_idx_imag = fits_idx_real+1;
                        float corr_val_real = gpu_matrix[corr_idx_real];
                        float corr_val_imag = gpu_matrix[corr_idx_imag];
                        fits_matrix[fits_idx_real] = corr_val_real;
                        fits_matrix[fits_idx_imag] = corr_val_imag;
                 }
        }
}


// This function takes the GPU's covariance matrix output (64x64)
//   and parses it into a consolidated format suitable for writing to FITS.
// There are two "steps" here.
// 1. We know that only the first NONZERO_BIN_SIZE elements are non-zero
//    That is, xGPU will only be writing data to this number of elements.
//    So, for each frequency bin, we can simply stop processing after
//    we have processed NONZERO_BIN_SIZE elements in each frequency bin.
// 2. We know that there will be NUM_ANTENNAS/2 redundant elements in
//    each frequency bin. That is, of the NONZERO_BIN_SIZE elements,
//    some will be duplicates. These are slightly more difficult to remove.
//    This is done with the next_red_element index tracker, etc.
void
BfCovFitsIO::parseGpuCovMatrix(float const *const gpu_matrix, float *const fits_matrix)
{
    //parseGpuCovMatrix(gpu_matrix, NONZERO_BIN_SIZE, fits_matrix, FITS_BIN_SIZE, NUM_CHANNELS); 
    // this is not right, but avoids current bug from crashing things!
    parseGpuCovMatrix(gpu_matrix, FITS_BIN_SIZE, fits_matrix, FITS_BIN_SIZE, NUM_CHANNELS); 
}

void
BfCovFitsIO::parseGpuCovMatrix(float const *const gpu_matrix, int gpu_size, float *const fits_matrix, int fits_size, int num_channels)
{
    // Counts number of redundant elements encountered
    int red_els = 0;
    // Counts total number of elements encountered
    int els = 0;

    // Holds index of next redundant element
    int next_red_element = 1;
    // The next_red_element will be inc indices away from
    //   the previously found redundant element
    // We start at 8 because we know that the 
    int inc = 8;

    int fits_real_index = 0;
    int fits_imag_index = 1;
    
    int fits_data_sz = num_channels * fits_size * 2;
    int i, j;
    //for (i = 0; i < NUM_CHANNELS; i++)
    for (i = 0; i < num_channels; i++)
    {
        //printf("i: %d\n", i);
        // Remember we need to double the bin size because each complex pair
        //   is actually represented as two floats
        // This also means that we are iterating by two (since we are treating every
        //   two elements as an atomic unit)
        for (j = 0; j < gpu_size * 2; j += 2)
        //for (j = 0; j < NONZERO_FITS_BIN_SIZE * 2; j += 2)
        {
            // index counters for convenience
            int gpu_real_index = (i * gpu_size * 2) + j;
            int gpu_imag_index = gpu_real_index + 1;

            if (gpu_real_index / 2 == next_red_element)
            {
                next_red_element += inc;
                // Due to the nature of the "matrix of 2x2 submatrices" structure,
                //   the next redundant element will always be 4 elements after the
                //   current one 
                inc += 4;
                red_els++;
            }
            else
            {
                // TBF: bug!
                //assert(fits_real_index < sz); 
                if (fits_real_index > fits_data_sz) 
                    printf("Bad index: %d > %d; j: %d\n", fits_real_index, fits_data_sz, j);

                //value = 1.5; //gpu_matrix[gpu_real_index];
                //fits_matrix[fits_real_index] = value; 
                fits_matrix[fits_real_index] = gpu_matrix[gpu_real_index];
                fits_matrix[fits_imag_index] = gpu_matrix[gpu_imag_index];

                els++;
                // These variables keep track of the indices of the data table that
                //   will be written to FITS
                // Again, these must increment by 2 due to the atomic nature
                //   of a pair of floats in this context
                fits_real_index+=2;
                fits_imag_index+=2;
            }
        }
    }
}



