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
BfCovFitsIO::BfCovFitsIO(const char *path_prefix, int simulator, int instance_id, int cov_mode) : BfFitsIO(path_prefix, simulator, instance_id, cov_mode)
{
    // What distinquishes modes is their data format
   if (cov_mode == 0){  
       data_size = FITS_BIN_SIZE * NUM_CHANNELS;
       sprintf(data_form, "%dC", data_size);
   } 
   else if (cov_mode == 1){
       data_size = 2112 * NUM_CHANNELS_PAF;
       sprintf(data_form, "%dC", data_size);
   }
   else if (cov_mode == 2){
       data_size = FITS_BIN_SIZE * NUM_CHANNELS_FRB;
       sprintf(data_form, "%dC", data_size);
   }
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
        //printf("about to parse data\n");
        parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS);
        //printf("about to write parsed data\n");
        writeRow(mcnt, fits_matrix);
        //printf("done writing data\n");
        return 1;
}    

int BfCovFitsIO::write_PAF(int mcnt, float *data) {
        float fits_matrix[NUM_CHANNELS_PAF * FITS_BIN_SIZE * 2];
        data_size = 2112 * NUM_CHANNELS_PAF;
        //printf("about to parse data\n");
        //parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS_PAF);
        //printf("about to write parsed data\n");
        writeRow(mcnt, data);
        //printf("done writing data\n");
        return 1;
}

int BfCovFitsIO::write_FRB(int mcnt, float *data) {
        float fits_matrix[NUM_CHANNELS_FRB * FITS_BIN_SIZE * 2];
        data_size = FITS_BIN_SIZE * NUM_CHANNELS_FRB;
        //printf("about to parse data\n");
        parseAndReorderGpuCovMatrix(data,2112,fits_matrix,FITS_BIN_SIZE,NUM_CHANNELS_FRB);
        //printf("about to write parsed data\n");
        writeRow(mcnt, fits_matrix);
        //printf("done writing data\n");
        return 1;
} 


int BfCovFitsIO::write(int mcnt, float *data) {
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


    
    
    
    

    
    
    
    
    

    
    
    
    
    
    
    
       

       
       
   
   
   
   
   
   
   
   
   
   
   
    

    
    
    
    

    
    
    
    
    
    
    
    
    
   



