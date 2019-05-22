/** bf_databuf.h
 *
 * Defines shared mem structure for data passing.
 * Includes routines to allocate / attach to shared
 * memory.
 */
#ifndef _BF_DATABUF_H
#define _BF_DATABUF_H

#include <sys/ipc.h>
#include <sys/sem.h>
#include "spead_heap.h"

struct decprecated_bf_databuf {
    char data_type[64]; /**< Type of data in buffer */
    unsigned int buf_type;  /**< GPU_INPUT_BUF or CPU_INPUT_BUF */
    size_t databuf_size; /**< Size for the entire buffer (bytes) */
    size_t struct_size; /**< Size alloced for this struct (bytes) */
    size_t block_size;  /**< Size of each data block (bytes) */
    size_t header_size; /**< Size of each block header (bytes) */
    size_t index_size;  /**< Size of each block's index (bytes) */
    int shmid;          /**< ID of this shared mem segment */
    int semid;          /**< ID of locking semaphore set */
    int n_block;        /**< Number of data blocks in buffer */
};

//#define BF_DATABUF_KEY 0x00C62C70
#define BF_DATABUF_KEY 0x8019bbf9

#define NUM_ANTENNAS 40

// The bin size is the number of elements in the lower trianglular
//   portion of the covariance matrix
//   (41 * 20) gives us the number of complex pair elements
// #define
// #define GPU_BIN_SIZE (((64 * 65) / 2) + 32)
#define GPU_BIN_SIZE (2112)
// #define FITS_BIN_SIZE ((NUM_ANTENNAS * NUM_ANTENNAS + 1) / 2)
#define FITS_BIN_SIZE (820)
// #define NONZERO_BIN_SIZE (FITS_BIN_SIZE + (NUM_ANTENNAS / 2))
#define NONZERO_BIN_SIZE (840)
// #define GPU_BIN_SIZE 4
// This is the number of frequency channels that we will be correlating
//   It will be either 5, 50, or 160, and probably should always be a macro
//   For the purposes of this simulator we don't care about the input to the correlator
//   except that the number of input channels will indicate the number of output channels
//   That is, the total number of complex pairs we will be writing to shared memory
//   is given as: GPU_BIN_SIZE * NUM_CHANNELS
#define NUM_CHANNELS 160
// added by Nick (on 2/19/19) for scalloping fix code
#define NUM_CHANNELS_PAF 25
//#define NUM_CHANNELS_PAF 20
#define NUM_CHANNELS_FRB 5
#define TOTAL_GPU_DATA_SIZE (GPU_BIN_SIZE * NUM_CHANNELS * 2)
#define TOTAL_GPU_DATA_SIZE_PAF (GPU_BIN_SIZE * NUM_CHANNELS_PAF*2)
#define TOTAL_GPU_DATA_SIZE_FRB (GPU_BIN_SIZE * NUM_CHANNELS_FRB * 2)

// For pulsar mode 
#define NUM_PULSAR_CHANNELS 25
// added by Nick (on 2/19/19) for scalloping fix code
//#define NUM_PULSAR_CHANNELS 20
#define NUM_BEAMS 7
//#define TOTAL_GPU_PULSAR_DATA_SIZE (NUM_BEAMS * NUM_PULSAR_CHANNELS * 4* 100)
// added by Nick (on 3/2/19) to disregard cross-pol term
#define TOTAL_GPU_PULSAR_DATA_SIZE (NUM_BEAMS * NUM_PULSAR_CHANNELS * 2 * 100)

#define NUM_BLOCKS 2
//MACROs for mcnts/second
#define ADC_SAMPLE_RATE 155.52
#define COARSE_CHAN_SAMPLE_RATE ADC_SAMPLE_RATE/512
// added by Nick (on 2/19/19) for scalloping fix code
//#define COARSE_CHAN_SAMPLE_RATE ADC_SAMPLE_RATE/256
#define MCNT_RATE (COARSE_CHAN_SAMPLE_RATE/20)*1000000

typedef struct {
    char data_type[64]; /* Type of data in buffer */
    size_t header_size; /* Size of each block header (bytes) */
    size_t block_size;  /* Size of each data block (bytes) */
    int n_block;        /* Number of data blocks in buffer */
    int shmid;          /* ID of this shared mem segment */
    int semid;          /* ID of locking semaphore set */
} bf_databuf_header_t;

#define CACHE_ALIGNMENT (128)
typedef uint8_t bf_databuf_cache_alignment[
    CACHE_ALIGNMENT - (sizeof(bf_databuf_header_t)%CACHE_ALIGNMENT)
];


typedef struct bf_databuf_block_header {
        int64_t good_data;
	uint64_t  mcnt;
        //uint64_t flags[(25+63)/64];
} bf_databuf_block_header_t;

typedef uint8_t bf_databuf_block_header_cache_alignment[
    CACHE_ALIGNMENT - (sizeof(bf_databuf_block_header_t)%CACHE_ALIGNMENT)
];

typedef struct bf_databuf_block {
  bf_databuf_block_header_t header;
  bf_databuf_block_header_cache_alignment padding;
  // we must double the elements since CFITSIO interperates every two elements as a pair
  float data[TOTAL_GPU_DATA_SIZE];
} bf_databuf_block_t;

typedef struct bfp_databuf_block {
  bf_databuf_block_header_t header;
  bf_databuf_block_header_cache_alignment padding;
  // we must double the elements since CFITSIO interperates every two elements as a pair
  float data[TOTAL_GPU_PULSAR_DATA_SIZE];
} bfp_databuf_block_t;

typedef struct bfpaf_databuf_block {
  bf_databuf_block_header_t header;
  bf_databuf_block_header_cache_alignment padding;
float data[TOTAL_GPU_DATA_SIZE_PAF];
} bfpaf_databuf_block_t;


typedef struct bffrb_databuf_block {
  bf_databuf_block_header_t header;

  bf_databuf_block_header_cache_alignment padding;
float data[TOTAL_GPU_DATA_SIZE_FRB];
} bffrb_databuf_block_t;

// Covariance Matrix outputmode
typedef struct bf_databuf {
        bf_databuf_header_t header;
        bf_databuf_cache_alignment padding;
        bf_databuf_block_t block[NUM_BLOCKS];
}bf_databuf_t;

// PAF Cal output mode
typedef struct bfpaf_databuf {
        bf_databuf_header_t header;
        bf_databuf_cache_alignment padding;
        bfpaf_databuf_block_t block[NUM_BLOCKS];
}bfpaf_databuf_t;

// FRB output mode

typedef struct bffrb_databuf {
        bf_databuf_header_t header;
        bf_databuf_cache_alignment padding;
        bffrb_databuf_block_t block[NUM_BLOCKS];
}bffrb_databuf_t;


// Pulsar outputmode
typedef struct bfp_databuf {
        bf_databuf_header_t header;
        bf_databuf_cache_alignment padding;
        bfp_databuf_block_t block[NUM_BLOCKS];
}bfp_databuf_t;

/* union for semaphore ops. */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

#define GPU_INPUT_BUF       1
#define CPU_INPUT_BUF       2
#define DISK_INPUT_BUF      3

#define MAX_BLKS_PER_BUF    1024
#define MAX_HEAPS_PER_BLK   4096

// Single element of the index for the GPU or CPU input buffer
struct cpu_gpu_buf_index
{
    unsigned int heap_cntr;
    unsigned int heap_valid;
    double heap_rcvd_mjd;
};

// Single element of the index for the disk input buffer
struct disk_buf_index
{
    unsigned int struct_offset;
    unsigned int array_offset;
};

// The index that sits at the top of each databuf block
struct databuf_index
{
    union {
        unsigned int num_heaps;     ///< Number of actual heaps in block
        unsigned int num_datasets;  ///< Number of datasets in block
    };

    union {
        unsigned int heap_size;     ///< Size of a single heap
        unsigned int array_size;    ///< Size of a single data array
    };

    // The actual index
    union {
        struct cpu_gpu_buf_index cpu_gpu_buf[MAX_HEAPS_PER_BLK];
        struct disk_buf_index    disk_buf[2*MAX_HEAPS_PER_BLK];
    };
};

#ifdef __cplusplus /* C++ prototypes */
extern "C" {
#endif

/** Create a new shared mem area with given params.  Returns
 * pointer to the new area on success, or NULL on error.  Returns
 * error if an existing shmem area exists with the given shmid (or
 * if other errors occured trying to allocate it).
 */
//struct bf_databuf *bf_databuf_create(int n_block, size_t block_size,
//        int databuf_id, int buf_type);
void bf_conf_databuf_size(struct bf_databuf *d, size_t new_block_size);

/** Return a pointer to a existing shmem segment with given id.
 * Returns error if segment does not exist
 */
//<<<<<<< HEAD:src/vegas_hpc/src/bf_databuf.h
//struct bf_databuf *bf_databuf_attach(int databuf_id);
//struct bfp_databuf *bfp_databuf_attach(int databuf_id);
//int databuf_get_shmid(int databuf_id);
//=======
struct bf_databuf *bf_databuf_attach(int databuf_id, int instance_id);
struct bfp_databuf *bfp_databuf_attach(int databuf_id, int instance_id);
struct bfpaf_databuf *bfpaf_databuf_attach(int databuf_id, int instance_id);
struct bffrb_databuf *bffrb_databuf_attach(int databuf_id, int instance_id);
int databuf_get_shmid(int databuf_id, int instance_id);

/** Detach from shared mem segment */
int databuf_detach(void *d);
int bf_databuf_detach(struct bf_databuf *d);
int bfp_databuf_detach(struct bfp_databuf *d);
int bfpaf_databuf_detach(struct bfpaf_databuf *d);
/** Clear out either the whole databuf (set all sems to 0,
 * clear all header blocks) or a single FITS-style
 * header block.
 */
void bf_databuf_clear(struct bf_databuf *d);
void bf_fitsbuf_clear(char *buf);

/** These return pointers to the header, the index or the data area for
 * the given block_id.
 */
char *bf_databuf_header(struct bf_databuf *d, int block_id);
char *bf_databuf_data(struct bf_databuf *d, int block_id);
char *bf_databuf_index(struct bf_databuf *d, int block_id);
/// Inside a data buffer datablocks there are a number of spead headers
/// followed by the actual data payload records. The calculates the
/// address of the header for the Nth heap of block M (i.e block_id and heap_id respectively)
/// when frequency heaps are used.
struct time_spead_heap *
vegas_datablock_time_heap_header(struct bf_databuf *d, int block_id, int heap_id);

/// This returns a pointer to the data payload of the Nth heap of block M
/// when time heaps are used.
struct freq_spead_heap *
vegas_datablock_freq_heap_header(struct bf_databuf *d, int block_id, int heap_id);

/// This returns a pointer to the data payload of the Nth heap of block M
/// when time heaps are used.
char *vegas_datablock_time_heap_data(struct bf_databuf *d, int block_id, int heap_id);

/// This returns a pointer to the data payload of the Nth heap of block M
/// when freq heaps are used.
char *vegas_datablock_freq_heap_data(struct bf_databuf *d, int block_id, int heap_id);

size_t time_heap_datasize(struct databuf_index* index);
size_t freq_heap_datasize(struct databuf_index* index);

/** Returns lock status for given block_id, or total for
 * whole array.
 */
int bf_databuf_block_status(struct bf_databuf *d, int block_id);
int bf_databuf_total_status(struct bf_databuf *d);

/** Databuf locking functions.  Each block in the buffer
 * can be marked as free or filled.  The "wait" functions
 * block until the specified state happens.  The "set" functions
 * put the buffer in the specified state, returning error if
 * it is already in that state.
 */
int bf_databuf_wait_filled(struct bf_databuf *d, int block_id);
int bfp_databuf_wait_filled(struct bfp_databuf *d, int block_id);
int bfpaf_databuf_wait_filled(struct bfpaf_databuf *d, int block_id);
int bffrb_databuf_wait_filled(struct bffrb_databuf *d, int block_id);
int databuf_wait_filled(int semid, int block_id);
int bf_databuf_set_filled(struct bf_databuf *d, int block_id);
int bf_databuf_wait_free(struct bf_databuf *d, int block_id);
int bf_databuf_set_free(struct bf_databuf *d, int block_id);
int bfp_databuf_set_free(struct bfp_databuf *d, int block_id);
int bfpaf_databuf_set_free(struct bfpaf_databuf *d, int block_id);
int bffrb_databuf_set_free(struct bffrb_databuf *d, int block_id);
int databuf_set_free(int semid, int block_id);

#ifdef __cplusplus /* C++ prototypes */
}
#endif


#endif
