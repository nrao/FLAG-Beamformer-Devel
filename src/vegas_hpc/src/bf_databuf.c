/* bf_databuf.c
 *
 * Routines for creating and accessing main data transfer
 * buffer in shared memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>

#include "fitshead.h"
#include "vegas_status.h"
#include "bf_databuf.h"
#include "vegas_error.h"
#include "hashpipe_ipckey.h"



struct bf_databuf *bf_databuf_create(int n_block, size_t block_size,
        int databuf_id, int buf_type) {
    struct bf_databuf *d;    
    return(d);
}

/** 
 * Resizes the blocks within the specified databuf. The number of blocks
 * are automatically changed, so that the total buffer size remains constant.
 */
void bf_conf_databuf_size(struct bf_databuf *d, size_t new_block_size)
{

    /* Calculate number of data blocks that can fit into the existing buffer */
    //int new_n_block = (d->databuf_size - d->struct_size) / (new_block_size + d->header_size + d->index_size);
    
    /* Make sure that there won't be more data blocks than semaphores */
    /*
    if(new_n_block > MAX_BLKS_PER_BUF)
    {
        printf("Warning: the disk buffer contains more than %d blocks. Only %d blocks will be used\n",
                MAX_BLKS_PER_BUF, MAX_BLKS_PER_BUF);
        new_n_block = MAX_BLKS_PER_BUF;
    }
    */

    /* Fill params into databuf */
    //d->n_block = new_n_block;
    //d->block_size = new_block_size;

    return;
}

/** Detach from the shared memory databuffer */
int bf_databuf_detach(struct bf_databuf *d) {
    return databuf_detach(d);
}

int bfp_databuf_detach(struct bfp_databuf *d) {
    return databuf_detach(d);
}

int databuf_detach(void *d) {
    int rv = shmdt(d);
    if (rv!=0) {
        vegas_error("vegas_status_detach", "shmdt error");
        return(VEGAS_ERR_SYS);
    }
    return(VEGAS_OK);
}

void bf_databuf_clear(struct bf_databuf *d) {

/*
    // Zero out semaphores 
    union semun arg;
    if(d->buf_type == DISK_INPUT_BUF)
    {
      arg.array = (unsigned short *)malloc(sizeof(unsigned short)*MAX_BLKS_PER_BUF);
      memset(arg.array, 0, sizeof(unsigned short)*MAX_BLKS_PER_BUF);
    }
    else
    {
      arg.array = (unsigned short *)malloc(sizeof(unsigned short)*d->n_block);
      memset(arg.array, 0, sizeof(unsigned short)*d->n_block);
    }

    semctl(d->header.semid, 0, SETALL, arg);
    free(arg.array);

    // Clear all headers 
    int i;
    for (i=0; i<d->n_block; i++) {
        vegas_fitsbuf_clear(bf_databuf_header(d, i));
    }
*/

}

void bf_fitsbuf_clear(char *buf) {
    char *end, *ptr;
    end = ksearch(buf, "END");
    if (end!=NULL) {
        for (ptr=buf; ptr<=end; ptr+=80) memset(ptr, ' ', 80);
    }
    memset(buf, ' ' , 80);
    strncpy(buf, "END", 3);
}

/// Returns a pointer to the 1st FITS header for the given block
char *bf_databuf_header(struct bf_databuf *d, int block_id) {
    //return((char *)d + d->struct_size + block_id*d->header_size);
    return((char *)d);
}

/// Returns a pointer to the 1st index for the given block
char *bf_databuf_index(struct bf_databuf *d, int block_id) {
    //return((char *)d + d->struct_size + d->n_block*d->header_size
    //        + block_id*d->index_size);
    return((char *)d);
}
/// Returns a pointer to the base of the data for the given block
char *bf_databuf_data(struct bf_databuf *d, int block_id) {
    //return((char *)d + d->struct_size + d->n_block*d->header_size
    //        + d->n_block*d->index_size + block_id*d->block_size);
    return((char *)d);
}

size_t time_heap_datasize(struct databuf_index* index)
{
    return(index->heap_size - sizeof(struct time_spead_heap));
}

size_t freq_heap_datasize(struct databuf_index* index)
{
    return(index->heap_size - sizeof(struct freq_spead_heap));
}

/**
    ASCII diagram of vegas databuf layout:

    +---------------------------+
    | bf_databuf structure   |
    +---------------------------+
    | FITS header for block 0   |   <---- bf_databuf_header(db,0)
    +---------------------------+
    | FITS header for block 1   |
    +---------------------------+
    |           ...             |
    +---------------------------+    
    | FITS header for block N-1 |
    +---------------------------+
    | Index for block 0         |   <---- bf_databuf_index(db, 0)
    +---------------------------+
    | Index for block 1         |
    +---------------------------+    
    |           ...             |    
    +---------------------------+
    | Index for block N-1       |            
    +---------------------------+
    | Data for block 0          |    <---- bf_databuf_data(db, 0)        
    +---------------------------+
    | Data for block 1          |            
    +---------------------------+
    |           ...             |    
    +---------------------------+
    | Data for block N-1        |            
    +---------------------------+        
    
The sizes of the data blocks, FITS headers, and Indexes are given in the bf_databuf structure. 

Inside each data block, are multiple 'heaps' with the spead headers packed at the top of the buffer,
followed by the data for each heap. 

The Index entry for the block contains the member 'num_heaps'
(aka 'num_datasets') which specifies the number of valid heap entries. 

    +---------------------------+
    | spead header for heap 0   |
    +---------------------------+
    | spead header for heap 1   |
    +---------------------------+
    |           ...             |
    +---------------------------+    
    | spead header for heap M   |
    +---------------------------+    
    | data for heap 0           |
    +---------------------------+
    | data for heap 1           |
    +---------------------------+    
    |           ...             |
    +---------------------------+    
    | data for heap M           |
    +---------------------------+    
    
*/

// Inside a data buffer datablocks there are a number of spead headers
// followed by the actual data payload records. The calculates the
// address of the header for the Nth heap of block M (i.e block_id and heap_id respectively)
//  when frequency heaps are used.
struct time_spead_heap *
vegas_datablock_time_heap_header(struct bf_databuf *d, int block_id, int heap_id)
{
    char *p = bf_databuf_data(d, block_id);
    p += sizeof(struct time_spead_heap) * heap_id;
    return (struct time_spead_heap *)p;
}

// Inside a data buffer datablocks there are a number of spead headers
// followed by the actual data payload records. The calculates the
// address of the header for the Nth heap of block M (i.e block_id and heap_id respectively)
//  when frequency heaps are used.
struct freq_spead_heap *
vegas_datablock_freq_heap_header(struct bf_databuf *d, int block_id, int heap_id)
{
    char *p = bf_databuf_data(d, block_id);
    p += sizeof(struct freq_spead_heap) * heap_id;
    return (struct freq_spead_heap *)p;
}

// This returns a pointer to the data payload of the Nth heap of block M
// when time heaps are used.
char *
vegas_datablock_time_heap_data(struct bf_databuf *d, int block_id, int heap_id)
{
    char *p;
    struct databuf_index *index;
    index = (struct databuf_index *)bf_databuf_index(d, block_id);
    
    p = bf_databuf_data(d, block_id);
    p += sizeof(struct time_spead_heap) * MAX_HEAPS_PER_BLK;
    p += (index->heap_size - sizeof(struct time_spead_heap)) * heap_id;
    return p;
}

// This returns a pointer to the data payload of the Nth heap of block M
// when freq heaps are used.
char *
vegas_datablock_freq_heap_data(struct bf_databuf *d, int block_id, int heap_id)
{
    struct databuf_index *index;
    index = (struct databuf_index *)bf_databuf_index(d, block_id);
    
    char *p = bf_databuf_data(d, block_id);
    p += sizeof(struct freq_spead_heap) * MAX_HEAPS_PER_BLK;
    p += (index->heap_size - sizeof(struct freq_spead_heap)) * heap_id;
    return p;
}

/** Attach to the specified data buffer.
 *  Returns the address of the databuffer if successful , or zero on error.
 */
//<<<<<<< HEAD:src/vegas_hpc/src/bf_databuf.c
struct bf_databuf *bf_databuf_attach(int databuf_id, int instance_id) {

    int shmid = databuf_get_shmid(databuf_id, instance_id);
    if (shmid == -1)
        return NULL;

    /* Attach */
    struct bf_databuf *d;
    d = shmat(shmid, NULL, 0);
    if (d==(void *)-1) {
        vegas_error("bf_databuf_attach", "shmat error");
        return(NULL);
    }

    return(d);

}

struct bfp_databuf *bfp_databuf_attach(int databuf_id, int instance_id) {

    int shmid = databuf_get_shmid(databuf_id, instance_id);
    if (shmid == -1)
        return NULL;

    // Attach
    struct bfp_databuf *d;
    d = shmat(shmid, NULL, 0);
    if (d==(void *)-1) {
        vegas_error("bf_databuf_attach", "shmat error");
        return(NULL);
    }

    return(d);
}

/** Retreive the shared memory ID for the given data buffer id 
 */
int databuf_get_shmid(int databuf_id, int instance_id) {

//    // TBF: needs to be instance aware!
//    int instance_id = 0;
//=======
//struct vegas_databuf *vegas_databuf_attach(int databuf_id, int instance_id) {
//>>>>>>> instances:src/vegas_hpc/src/vegas_databuf.c

    // Get shared memory block
    key_t key = hashpipe_databuf_key(instance_id);
    //if(key == HASHPIPE_KEY_ERROR) {
    if(key == -1) {
        //hashpipe_error(__FUNCTION__, "hashpipe_databuf_key error");
        vegas_error("bf_databuf_attach", "hashpipe_databuf_key error");
        //return NULL;
        return -1;
    }
    printf("bf_databuf shmget key: %x\n" , key);
    
    // Get shmid
    int shmid;
    shmid = shmget(key + databuf_id - 1, 0, 0666);
    if (shmid==-1) {
        // Doesn't exist, exit quietly otherwise complain
        if (errno!=ENOENT)
            vegas_error("bf_databuf_attach", "shmget error");
        //return(NULL);
        return(-1);
    }

    return(shmid);
}


int bf_databuf_block_status(struct bf_databuf *d, int block_id) {
    return(semctl(d->header.semid, block_id, GETVAL));
}

int bf_databuf_total_status(struct bf_databuf *d) {

    /* Get all values at once */
    union semun arg;
    arg.array = (unsigned short *)malloc(sizeof(unsigned short)*MAX_BLKS_PER_BUF);
    
    memset(arg.array, 0, sizeof(unsigned short)*MAX_BLKS_PER_BUF);
    semctl(d->header.semid, 0, GETALL, arg);
    int i,tot=0;
    for (i=0; i<d->header.n_block; i++) tot+=arg.array[i];
    free(arg.array);
    return(tot);

}

/** Wait until the specified data block becomes free.
 *  This will return 0 on success, or VEGAS_TIMEOUT if the buffer
 *  was not made free within 250ms.
 */
int bf_databuf_wait_free(struct bf_databuf *d, int block_id) {
    int rv;
    struct sembuf op;
    op.sem_num = block_id;
    op.sem_op = 0;
    op.sem_flg = 0;
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 250000000;
    rv = semtimedop(d->header.semid, &op, 1, &timeout);
    if (rv==-1) { 
        if (errno==EAGAIN) return(VEGAS_TIMEOUT);
        if (errno==EINTR) return(VEGAS_ERR_SYS);
        vegas_error("bf_databuf_wait_free", "semop error");
        perror("semop");
        return(VEGAS_ERR_SYS);
    }
    return(0);
}

    /** This needs to wait for the semval of the given block
     * to become > 0, but NOT immediately decrement it to 0.
     * Probably do this by giving an array of semops, since
     * (afaik) the whole array happens atomically:
     * step 1: wait for val=1 then decrement (semop=-1)
     * step 2: increment by 1 (semop=1)
     */
int bfp_databuf_wait_filled(struct bfp_databuf *d, int block_id) {
    return databuf_wait_filled(d->header.semid, block_id);
}
int bf_databuf_wait_filled(struct bf_databuf *d, int block_id) {
    return databuf_wait_filled(d->header.semid, block_id);
}

int databuf_wait_filled(int semid, int block_id) {
    int rv;
    struct sembuf op[2];
    op[0].sem_num = op[1].sem_num = block_id;
    op[0].sem_flg = op[1].sem_flg = 0;
    op[0].sem_op = -1;
    op[1].sem_op = 1;
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 250000000;
    //rv = semtimedop(d->header.semid, op, 2, &timeout);
    rv = semtimedop(semid, op, 2, &timeout);
    if (rv==-1) { 
        if (errno==EAGAIN) return(VEGAS_TIMEOUT);
        // Don't complain on a signal interruption
        if (errno==EINTR) return(VEGAS_ERR_SYS);
        vegas_error("bf_databuf_wait_filled", "semop error");
        perror("semop");
        return(VEGAS_ERR_SYS);
    }
    return(0);
}

    /** This function should always succeed regardless of the current
     * state of the specified databuf.  So we use semctl (not semop) to set
     * the value to zero.
     */
int bf_databuf_set_free(struct bf_databuf *d, int block_id) {
    return databuf_set_free(d->header.semid, block_id);
}

int bfp_databuf_set_free(struct bfp_databuf *d, int block_id) {
    return databuf_set_free(d->header.semid, block_id);
}

int databuf_set_free(int semid, int block_id) {
    int rv;
    union semun arg;
    arg.val = 0;
    //rv = semctl(d->header.semid, block_id, SETVAL, arg);
    rv = semctl(semid, block_id, SETVAL, arg);
    if (rv==-1) { 
        vegas_error("bf_databuf_set_free", "semctl error");
        return(VEGAS_ERR_SYS);
    }
    return(0);
}


    /** This function should always succeed regardless of the current
     * state of the specified databuf.  So we use semctl (not semop) to set
     * the value to one.
     */
int bf_databuf_set_filled(struct bf_databuf *d, int block_id) {
    int rv;
    union semun arg;
    arg.val = 1;
    rv = semctl(d->header.semid, block_id, SETVAL, arg);
    if (rv==-1) { 
        vegas_error("bf_databuf_set_filled", "semctl error");
        return(VEGAS_ERR_SYS);
    }
    return(0);
}
