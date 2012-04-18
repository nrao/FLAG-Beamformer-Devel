/* guppi_params.h 
 *
 * Defines structure used internally to represent observation 
 * parameters.  Includes routines to read/write this info to
 * a "FITS-style" shared memory buffer.
 */
#ifndef _GUPPI_PARAMS_H
#define _GUPPI_PARAMS_H

#include "guppi_udp.h"
#include "guppi_defines.h"

#ifndef NEW_GBT

struct guppi_params {
    /* Packet information for the current block */
    long long packetindex;      // Index of first packet in raw data block
    double drop_frac_avg;       // Running average of the fract of dropped packets
    double drop_frac_tot;       // Total fraction of dropped packets
    double drop_frac;           // Fraction of dropped packets in this block
    int packetsize;             // Size in bytes of data portion of each packet
    int n_packets;              // Total number of packets in current block
    int n_dropped;              // Number of packets dropped in current block
    int packets_per_block;      // Total number of packets per block
    int stt_valid;              // Has an accurate start time been measured
    /* Backend hardware info */
    int decimation_factor;      // Number of raw spectra integrated
    int n_bits_adc;             // Number of bits sampled by ADCs
    int pfb_overlap;            // PFB overlap factor
    float scale[16*1024];       // Per-channel scale factor
    float offset[16*1024];      // Per-channel offset
};

#else

struct guppi_params
{
    /* Packet information for the current block */
    int num_pkts_rcvd;          // Number of packets received in current block
    int num_pkts_dropped;       // Number of packets dropped in current block
    double drop_frac;           // Fraction of dropped packets in this block
    double drop_frac_avg;       // Running average of the fract of dropped packets
    double drop_frac_tot;       // Total fraction of dropped packets

    int stt_valid;              // Has an accurate start time been measured
};

#endif

#if FITS_TYPE == PSRFITS

#include "psrfits.h"
void guppi_read_obs_mode(const char *buf, char *mode);
void guppi_read_net_params(char *buf, struct guppi_udp_params *u);
void guppi_read_subint_params(char *buf, 
                              struct guppi_params *g, 
                              struct psrfits *p);
void guppi_read_obs_params(char *buf, 
                           struct guppi_params *g, 
                           struct psrfits *p);
void guppi_free_psrfits(struct psrfits *p);

#else

#include "sdfits.h"
void guppi_read_obs_mode(const char *buf, char *mode);
void guppi_read_net_params(char *buf, struct guppi_udp_params *u);
void guppi_read_subint_params(char *buf, 
                              struct guppi_params *g, 
                              struct sdfits *p);
void guppi_read_obs_params(char *buf, 
                           struct guppi_params *g, 
                           struct sdfits *p);
void guppi_free_sdfits(struct sdfits *p);

#endif

#endif
