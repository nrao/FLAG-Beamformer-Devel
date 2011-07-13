/* vegas_hpc_hbw.c
 *
 * The main VEGAS HPC program for high-bandwidth modes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <errno.h>

#include "guppi_error.h"
#include "guppi_status.h"
#include "guppi_databuf.h"
#include "guppi_params.h"
#include "guppi_thread_main.h"
#include "guppi_defines.h"
#include "fitshead.h"

/* Thread declarations */
void *guppi_net_thread(void *args);
#if FITS_TYPE == PSRFITS
void *guppi_psrfits_thread(void *args);
#else
void *guppi_sdfits_thread(void *args);
#endif

void *vegas_pfb_thread(void *args);
void *guppi_accum_thread(void *args);

#ifdef RAW_DISK
void *guppi_rawdisk_thread(void *args);
#endif

#ifdef FAKE_NET
void *guppi_fake_net_thread(void *args);
#endif


int main(int argc, char *argv[]) {

    /* thread args */
    struct guppi_thread_args net_args, pfb_args, accum_args, disk_args;
    guppi_thread_args_init(&net_args);
    guppi_thread_args_init(&pfb_args);
    guppi_thread_args_init(&accum_args);
    guppi_thread_args_init(&disk_args);
    net_args.output_buffer = 1;
    pfb_args.input_buffer = net_args.output_buffer;
    pfb_args.output_buffer = 2;
    accum_args.input_buffer = pfb_args.output_buffer;
    accum_args.output_buffer = 3;
    disk_args.input_buffer = accum_args.output_buffer;

    /* Init status shared mem */
    struct guppi_status stat;
    int rv = guppi_status_attach(&stat);
    if (rv!=GUPPI_OK) {
        fprintf(stderr, "Error connecting to guppi_status\n");
        exit(1);
    }

    hputs(stat.buf, "BW_MODE", "low");

    /* Init first shared data buffer */
    struct guppi_databuf *gpu_input_dbuf=NULL;
    gpu_input_dbuf = guppi_databuf_attach(pfb_args.input_buffer);

    /* If attach fails, first try to create the databuf */
    if (gpu_input_dbuf==NULL) 
        gpu_input_dbuf = guppi_databuf_create(24, 32*1024*1024,
                            pfb_args.input_buffer, GPU_INPUT_BUF);

    /* If that also fails, exit */
    if (gpu_input_dbuf==NULL) {
        fprintf(stderr, "Error connecting to gpu_input_dbuf\n");
        exit(1);
    }

    guppi_databuf_clear(gpu_input_dbuf);

    /* Init second shared data buffer */
    struct guppi_databuf *cpu_input_dbuf=NULL;
    cpu_input_dbuf = guppi_databuf_attach(accum_args.input_buffer);

    /* If attach fails, first try to create the databuf */
    if (cpu_input_dbuf==NULL) 
        cpu_input_dbuf = guppi_databuf_create(24, 32*1024*1024,
                            accum_args.input_buffer, CPU_INPUT_BUF);

    /* If that also fails, exit */
    if (cpu_input_dbuf==NULL) {
        fprintf(stderr, "Error connecting to cpu_input_dbuf\n");
        exit(1);
    }

    guppi_databuf_clear(cpu_input_dbuf);

    /* Init third shared data buffer */
    struct guppi_databuf *disk_input_dbuf=NULL;
    disk_input_dbuf = guppi_databuf_attach(disk_args.input_buffer);

    /* If attach fails, first try to create the databuf */
    if (disk_input_dbuf==NULL) 
        disk_input_dbuf = guppi_databuf_create(16, 32*1024*1024,
                            disk_args.input_buffer, DISK_INPUT_BUF);

    /* If that also fails, exit */
    if (disk_input_dbuf==NULL) {
        fprintf(stderr, "Error connecting to disk_input_dbuf\n");
        exit(1);
    }

    guppi_databuf_clear(disk_input_dbuf);

    signal(SIGINT, cc);

    /* Launch net thread */
    pthread_t net_thread_id;
#ifdef FAKE_NET
    rv = pthread_create(&net_thread_id, NULL, guppi_fake_net_thread,
            (void *)&net_args);
#else
    rv = pthread_create(&net_thread_id, NULL, guppi_net_thread,
            (void *)&net_args);
#endif
    if (rv) { 
        fprintf(stderr, "Error creating net thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Launch PFB thread */
    pthread_t pfb_thread_id;

    rv = pthread_create(&pfb_thread_id, NULL, vegas_pfb_thread, (void *)&pfb_args);

    if (rv) { 
        fprintf(stderr, "Error creating PFB thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Launch accumulator thread */
    pthread_t accum_thread_id;

    rv = pthread_create(&accum_thread_id, NULL, guppi_accum_thread, (void *)&accum_args);

    if (rv) { 
        fprintf(stderr, "Error creating accumulator thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Launch RAW_DISK thread, SDFITS disk thread, or PSRFITS disk thread */
    pthread_t disk_thread_id;
#ifdef RAW_DISK
    rv = pthread_create(&disk_thread_id, NULL, guppi_rawdisk_thread, 
        (void *)&disk_args);
#elif FITS_TYPE == PSRFITS
    rv = pthread_create(&disk_thread_id, NULL, guppi_psrfits_thread, 
        (void *)&disk_args);
#elif FITS_TYPE == SDFITS
    rv = pthread_create(&disk_thread_id, NULL, guppi_sdfits_thread, 
        (void *)&disk_args);
#endif
    if (rv) { 
        fprintf(stderr, "Error creating disk thread.\n");
        perror("pthread_create");
        exit(1);
    }

    /* Wait for end */
    run=1;
    while (run) { 
        sleep(1); 
        if (disk_args.finished) run=0;
    }
 
    pthread_cancel(disk_thread_id);
    pthread_cancel(pfb_thread_id);
    pthread_cancel(accum_thread_id);
    pthread_cancel(net_thread_id);
    pthread_kill(disk_thread_id,SIGINT);
    pthread_kill(accum_thread_id,SIGINT);
    pthread_kill(pfb_thread_id,SIGINT);
    pthread_kill(net_thread_id,SIGINT);
    pthread_join(net_thread_id,NULL);
    printf("Joined net thread\n"); fflush(stdout);
    pthread_join(pfb_thread_id,NULL);
    printf("Joined PFB thread\n"); fflush(stdout);
    pthread_join(accum_thread_id,NULL);
    printf("Joined accumulator thread\n"); fflush(stdout);
    pthread_join(disk_thread_id,NULL);
    printf("Joined disk thread\n"); fflush(stdout);

    guppi_thread_args_destroy(&net_args);
    guppi_thread_args_destroy(&pfb_args);
    guppi_thread_args_destroy(&accum_args);
    guppi_thread_args_destroy(&disk_args);

    exit(0);
}
