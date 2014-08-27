/* check_vegas_databuf.c
 *
 * Basic prog to test dstabuf shared mem routines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "fitshead.h"
#include "vegas_error.h"
#include "vegas_status.h"
#include "vegas_databuf.h"
#include "vegas_defines.h"

void usage() { 
    fprintf(stderr, 
            "Usage: check_vegas_databuf [options]\n"
            "Options:\n"
            "  -h, --help\n"
            "  -q, --quiet\n"
            "  -c, --create\n"
            "  -d, --delete\n"            
            "  -i n, --id=n  (1)\n"
            "  -s n, --size=n (32768)\n"
            "  -n n, --nblock=n (24)\n"
            );
}

int main(int argc, char *argv[]) {

    /* Loop over cmd line to fill in params */
    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"quiet",  0, NULL, 'q'},
        {"create", 0, NULL, 'c'},
        {"delete", 0, NULL, 'd'},        
        {"no_sts", 0, NULL, 'z'},        
        {"id",     1, NULL, 'i'},
        {"size",   1, NULL, 's'},
        {"nblock", 1, NULL, 'n'},
        {"type",   1, NULL, 't'},
        {0,0,0,0}
    };
    int opt,opti;
    int quiet=0;
    int create=0;
    int db_id=1;
    int blocksize = 32768;
    int nblock = 24;
    int type = 1;
    int deletebuf=0;
    int print_status_mem = 1;

    while ((opt=getopt_long(argc,argv,"hzqcdi:s:n:t:",long_opts,&opti))!=-1) {
        switch (opt) {
            case 'c':
                create=1;
                break;
            case 'd':
                deletebuf=1;
                create=0;
                break;                
            case 'q':
                quiet=1;
                break;
            case 'z':
                print_status_mem=0;
                break;
            case 'i':
                db_id = atoi(optarg);
                break;
            case 's':
                blocksize = atoi(optarg);
                break;
            case 'n':
                nblock = atoi(optarg);
                break;
            case 't':
                type = atoi(optarg);
                break;
            case 'h':
            default:
                usage();
                exit(0);
                break;
        }
    }

    /* Create mem if asked, otherwise attach */
    struct vegas_databuf *db=NULL;
    if (create) { 
        db = vegas_databuf_create(nblock, blocksize*1024, db_id, type);

        if (db==NULL) {
            fprintf(stderr, "Error creating databuf %d (may already exist).\n",
                    db_id);
            exit(1);
        }
    } else {
        db = vegas_databuf_attach(db_id);
        if (db==NULL) { 
            fprintf(stderr, 
                    "Error attaching to databuf %d (may not exist).\n",
                    db_id);
            exit(1);
        }
        if (deletebuf)
        {
            /* attach worked so it exists. Now clear it and detach in
               preparation to delete.
            */
            int shmid, semid, rtnval;
            vegas_databuf_clear(db);            
            shmid = db->shmid;
            semid = db->semid;
            rtnval = 0;
            if (shmctl(shmid,IPC_RMID, 0) != 0)
            {
                perror("removal of buffer failed:");
                rtnval = -1;
            }
            printf("buffer deleted successfully\n");
            if (shmdt(db) != 0)
            {
                perror("shm detach failed:");
                // silently fail
            }
            if (semctl(semid,IPC_RMID, 0) != 0)
            {
                perror("removal of semaphores failed:");
                rtnval = -1;
            }
            printf("sems deleted successfully\n");            
            exit (rtnval);
        }
        
    }

    if (quiet)
    {
        /* skip the verbose stats */
        exit(0);
    }

    /* Print basic info */
    printf("databuf %d stats:\n", db_id);
    printf("  shmid=%d\n", db->shmid);
    printf("  semid=%d\n", db->semid);
    printf("  databuffer type=%d\n", db->buf_type);
    printf("  n_block=%d\n", db->n_block);
    printf("  struct_size=%zd\n", db->struct_size);
    printf("  block_size=%zd\n", db->block_size);
    printf("  header_size=%zd\n", db->header_size);
    printf("  index_size=%zd\n\n", db->index_size);
    /* loop over blocks */
    int i;
    char buf[81];
    char *hdr, *ptr, *hend;
    for (i=0; i<db->n_block; i++) 
    {
        printf("block %d status=%d\n", i, 
                vegas_databuf_block_status(db, i));
                
        // check block field sizes
        struct databuf_index *db_blk_index;
        db_blk_index = (struct databuf_index *)vegas_databuf_index(db, i);
        printf("nheaps in block=%d\n", db_blk_index->num_heaps);
        printf("heap size =%d\n", db_blk_index->heap_size);
        if (db_blk_index->heap_size*db_blk_index->num_heaps > db->block_size)
        {
            printf("ERROR heapsize*nheaps > blocksize!! (%d > %zd)\n",
            db_blk_index->heap_size*db_blk_index->num_heaps, db->block_size);     
        }
                
        if (print_status_mem)
        {
            hdr = vegas_databuf_header(db, i);
            hend = ksearch(hdr, "END");
            if (hend==NULL) {
                printf("header not initialized\n");
            } else {
                hend += 80;
                printf("header:\n");
                for (ptr=hdr; ptr<hend; ptr+=80) {
                    strncpy(buf, ptr, 80);
                    buf[79]='\0';
                    printf("%s\n", buf);
                }
            }
        }
        
        

    }

    exit(0);
}
