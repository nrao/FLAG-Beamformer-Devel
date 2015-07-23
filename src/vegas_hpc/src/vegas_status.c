/* vegas_status.c
 *
 * Implementation of the status routines described 
 * in vegas_status.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#include "vegas_status.h"
#include "vegas_error.h"

int hashpipe_status_semname(int instance_id, char * semid, size_t size)
{
    //static char semid[NAME_MAX-4] = {'\0'};
    size_t length_remaining = size;
    int bytes_written;
    char *s;
    int rc = 1;

    const char * envstr = getenv("HASHPIPE_STATUS_SEMNAME");
    if(envstr) {
        strncpy(semid, envstr, length_remaining);
        semid[length_remaining-1] = '\0';
    } else {
        envstr = getenv("HASHPIPE_KEYFILE");
        if(!envstr) {
            envstr = getenv("HOME");
            if(!envstr) {
                envstr = "/tmp";
            }
        }
        strncpy(semid, envstr, length_remaining);
        semid[length_remaining-1] = '\0';
        // Convert all but the leading / to _
        s = semid + 1;
        while((s = strchr(s, '/'))) {
          *s = '_';
        }
        length_remaining -= strlen(semid);
        if(length_remaining > 0) {
            bytes_written = snprintf(semid+strlen(semid),
                length_remaining, "_hashpipe_status_%d", instance_id&0x3f);
            if(bytes_written < length_remaining) {
              // No truncation
              rc = 0;
            }
        }
    }
#ifdef HASHPIPE_VERBOSE
    fprintf(stderr, "using hashpipe status semaphore '%s'\n", semid);
#endif
    return rc;
}

int vegas_status_attach(struct vegas_status *s) {

    // TBF: needs to be instance aware!
    int instance_id = 0;
    instance_id &= 0x3f;
    //int NAME_MAX = 256;
    //char semid[NAME_MAX] = {'\0'};
    char semid[256] = {'\0'};

    /* Get shared memory block */
    key_t key = hashpipe_status_key(instance_id);
    //if(key == HASHPIPE_KEY_ERROR) {
    if(key == -1) {
        //hashpipe_error(__FUNCTION__, "hashpipe_databuf_key error");
        vegas_error("vegas_status_attach", "hashpipe_databuf_key error");
        return NULL;
    }
    printf("shmget key: %x\n" , key);

    /* Get shared mem id (creating it if necessary) */
    s->shmid = shmget(key, VEGAS_STATUS_SIZE, 0666 | IPC_CREAT);
    //s->shmid = shmget(VEGAS_STATUS_KEY, VEGAS_STATUS_SIZE, 0666 | IPC_CREAT);
    if (s->shmid==-1) { 
        vegas_error("vegas_status_attach", "shmget error");
        return(VEGAS_ERR_SYS);
    }

    /* Now attach to the segment */
    s->buf = shmat(s->shmid, NULL, 0);
    if (s->buf == (void *)-1) {
        printf("shmid=%d\n", s->shmid);
        vegas_error("vegas_status_attach", "shmat error");
        return(VEGAS_ERR_SYS);
    }

    /* Get the locking semaphore.
     * Final arg (1) means create in unlocked state (0=locked).
     */
    // make a uniqueish identifier for the semaphore, ala hashpipe
    if(hashpipe_status_semname(instance_id, semid, NAME_MAX)) {
        vegas_error("vegas_status_attach", "semname truncated");
        return(VEGAS_ERR_SYS);
    }    
    mode_t old_umask = umask(0);
    //s->lock = sem_open(VEGAS_STATUS_SEMID, O_CREAT, 0666, 1);
    printf("vegas_status_attach sem_open w/ %s\n" , semid);
    s->lock = sem_open(semid, O_CREAT, 0666, 1);
    umask(old_umask);
    if (s->lock==SEM_FAILED) {
        vegas_error("vegas_status_attach", "sem_open");
        return(VEGAS_ERR_SYS);
    }

    /* Init buffer if needed */
    vegas_status_chkinit(s);

    return(VEGAS_OK);
}

int vegas_status_detach(struct vegas_status *s) {
    int rv = shmdt(s->buf);
    if (rv!=0) {
        vegas_error("vegas_status_detach", "shmdt error");
        return(VEGAS_ERR_SYS);
    }
    s->buf = NULL;
    return(VEGAS_OK);
}

/* TODO: put in some (long, ~few sec) timeout */
int vegas_status_lock(struct vegas_status *s) {
    return(sem_wait(s->lock));
}

int vegas_status_unlock(struct vegas_status *s) {
    return(sem_post(s->lock));
}

/* Return pointer to END key */
char *vegas_find_end(char *buf) {
    /* Loop over 80 byte cards */
    int offs;
    char *out=NULL;
    for (offs=0; offs<VEGAS_STATUS_SIZE; offs+=VEGAS_STATUS_CARD) {
        if (strncmp(&buf[offs], "END", 3)==0) { out=&buf[offs]; break; }
    }
    return(out);
}

/* So far, just checks for existence of "END" in the proper spot */
void vegas_status_chkinit(struct vegas_status *s) {

    int semval;
    int retval;
    retval = sem_getvalue(s->lock,&semval);
    if (retval) {
        vegas_error("vegas_status_chkinit", "sem_getvalue failed");

    }
    if (semval == 0) {
        printf("Found vegas status semaphore locked in vegas_status_chkinit. releasing\n");
        vegas_status_unlock(s);
    }

    /* Lock */
    vegas_status_lock(s);

    /* If no END, clear it out */
    if (vegas_find_end(s->buf)==NULL) {
        /* Zero bufer */
        memset(s->buf, 0, VEGAS_STATUS_SIZE);
        /* Fill first card w/ spaces */
        memset(s->buf, ' ', VEGAS_STATUS_CARD);
        /* add END */
        strncpy(s->buf, "END", 3);
    }

    /* Unlock */
    vegas_status_unlock(s);
}

/* Clear out vegas status buf */
void vegas_status_clear(struct vegas_status *s) {

    int semval;
    int retval;
    retval = sem_getvalue(s->lock,&semval);
    if (retval) {
        vegas_error("vegas_status_clear", "sem_getvalue failed");
    }
    if (semval == 0) {
        printf("Found vegas status semaphore locked in vegas_status_clear. releasing\n");
        vegas_status_unlock(s);
    }

    /* Lock */
    vegas_status_lock(s);

    /* Zero bufer */
    memset(s->buf, 0, VEGAS_STATUS_SIZE);
    /* Fill first card w/ spaces */
    memset(s->buf, ' ', VEGAS_STATUS_CARD);
    /* add END */
    strncpy(s->buf, "END", 3);

    /* Unlock */
    vegas_status_unlock(s);
}
