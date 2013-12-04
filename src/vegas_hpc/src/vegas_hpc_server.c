/* vegas_hpc_server.c
 *
 * Persistent process that will await commands on a FIFO
 * and spawn datataking threads as appropriate.  Meant for
 * communication w/ VEGAS manager, etc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

#include "fitshead.h"
#include "vegas_error.h"
#include "vegas_status.h"
#include "vegas_databuf.h"
#include "vegas_params.h"
#include "pfb_gpu.h"

#include "vegas_thread_main.h"

extern int g_debug_accumulator_thread; // For debugging REMOVE THIS before official release

#define vegas_DAQ_CONTROL "/tmp/vegas_daq_control"

void usage() {
    fprintf(stderr,
            "Usage: vegas_daq_server [options]\n"
            "Options:\n"
            "  -h, --help         This message\n"
            "  -r, --resize-obuf  HPC will resize the output buffer block when starting a scan\n"
            "  -d, --accumulator-debug HPC will emit debugging info for each spectra\n"
           );
}

/* Override "usual" SIGINT stuff */
int srv_run=1;
void srv_cc(int sig) { srv_run=0; run=0; }
void srv_quit(int sig) { srv_run=0; }

/* privilege management when running as root/suid */
int   setup_privileges();

/* Thread declarations */
void *vegas_net_thread(void *args);
void *vegas_null_thread(void *args);
void *vegas_pfb_thread(void *args);
void *vegas_accum_thread(void *args);
void *vegas_rawdisk_thread(void *args);
void *vegas_sdfits_thread(void *args);

/* Useful thread functions */

int check_thread_exit(struct vegas_thread_args *args, int nthread) {
    int i, rv=0;
    for (i=0; i<nthread; i++) 
        rv += args[i].finished;
    return(rv);
}

// keyword listing and default values.
struct KeywordValues keywords[] =
{
    { "net_thread_mask", 0x0 },
    { "pfb_thread_mask",   0x0 },
    { "accum_thread_mask",  0x0 },
    { "psrfits_thread_mask", 0x0 },
    { "sdfits_thread_mask", 0x0 },    
    { "rawdisk_thread_mask", 0x0 },
    { "null_thread_mask", 0x0 },
    
    { "net_thread_priority", 0x0 },
    { "pfb_thread_priority",   0x0 },
    { "accum_thread_priority", 0x0  },
    { "sdfits_thread_priority",  0x0 },
    { "psrfits_thread_priority", 0x0 },
    { "rawdisk_thread_priority", 0x0 },
    { "null_thread_priority", 0x0 },
    { NULL, 0x0 },
};

#define NET_THREAD 0

#define HBW_ACCUM_THREAD 1
#define HBW_DISK_THREAD 2

#define LBW_PFB_THREAD 1
#define LBW_ACCUM_THREAD 2
#define LBW_DISK_THREAD 3

#define MONITOR_NULL_THREAD 1

#define HBW_NET_BUFFER (2)
#define LBW_NET_BUFFER (1)
#define LBW_PFB_BUFFER (2)

/// For the accumulator case, the manager always looks in buffer 3 for accumulator output
#define ACCUM_BUFFER (3)

/** Resize the blocks in the disk input buffer, based on the exposure parameter */
void configure_accumulator_buffer_size(struct vegas_status *stat, struct vegas_databuf *dbuf_acc)
{
    // When the manager is present, it will adjust the block size, if not we set it here.
#if 1
    /* Resize the blocks in the disk input buffer, based on the exposure parameter */
    struct vegas_params vegas_p;
    struct sdfits sf;
    int64_t num_exp_per_blk;
    int64_t disk_block_size;
    
    vegas_status_lock(stat);
    vegas_read_obs_params(stat->buf, &vegas_p, &sf);
    vegas_read_subint_params(stat->buf, &vegas_p, &sf);
    vegas_status_unlock(stat);
    
    num_exp_per_blk = (int)(ceil(DISK_WRITE_INTERVAL / sf.data_columns.exposure));
    disk_block_size = num_exp_per_blk * (sf.hdr.nchan * sf.hdr.nsubband * 4 * 4);
    if (disk_block_size > (int64_t)(32*1024*1024))
    {
        disk_block_size = disk_block_size > (int64_t)(32*1024*1024);
    }
    vegas_conf_databuf_size(dbuf_acc, disk_block_size);
#endif
}

void init_hbw_mode(struct vegas_thread_args *args, int *nthread) 
{
    *nthread = 0;
    vegas_thread_args_init(&args[NET_THREAD]); // net
    *nthread = *nthread + 1;
    vegas_thread_args_init(&args[HBW_ACCUM_THREAD]); // accum
    *nthread = *nthread + 1;

    args[NET_THREAD].output_buffer = HBW_NET_BUFFER;
    args[HBW_ACCUM_THREAD].input_buffer = args[NET_THREAD].output_buffer;
    args[HBW_ACCUM_THREAD].output_buffer = ACCUM_BUFFER;
     
#if !defined(EXT_DISK)
        vegas_thread_args_init(&args[HBW_DISK_THREAD]); // fits writer
        *nthread = *nthread + 1;
        args[HBW_DISK_THREAD].input_buffer = args[HBW_ACCUM_THREAD].output_buffer;
#endif
}


void init_lbw_mode(struct vegas_thread_args *args, int *nthread) {
    *nthread = 0;
    vegas_thread_args_init(&args[NET_THREAD]); // net
    *nthread = *nthread + 1;
    vegas_thread_args_init(&args[LBW_PFB_THREAD]); // pfb
    *nthread = *nthread + 1;
    vegas_thread_args_init(&args[LBW_ACCUM_THREAD]); // accum
    *nthread = *nthread + 1;

    args[NET_THREAD].output_buffer = LBW_NET_BUFFER;
    args[LBW_PFB_THREAD].input_buffer = args[NET_THREAD].output_buffer;
    args[LBW_PFB_THREAD].output_buffer = LBW_PFB_BUFFER;
    args[LBW_ACCUM_THREAD].input_buffer = args[LBW_PFB_THREAD].output_buffer;
    args[LBW_ACCUM_THREAD].output_buffer = ACCUM_BUFFER;
    
#if !defined(EXT_DISK)
        vegas_thread_args_init(&args[LBW_DISK_THREAD]); // fits writer
        *nthread = *nthread + 1;
        args[LBW_DISK_THREAD].input_buffer = args[HBW_ACCUM_THREAD].output_buffer;
#endif
}

void init_monitor_mode(struct vegas_thread_args *args, int *nthread) {
    vegas_thread_args_init(&args[NET_THREAD]); // net
    vegas_thread_args_init(&args[MONITOR_NULL_THREAD]); // null
    args[NET_THREAD].output_buffer = HBW_NET_BUFFER;
    args[MONITOR_NULL_THREAD].input_buffer = args[NET_THREAD].output_buffer;
    *nthread = 2;
}


void start_lbw_mode(struct vegas_thread_args *args, pthread_t *ids) 
{
    // TODO error checking...
    int rv;
    mask_to_cpuset(&args[NET_THREAD].cpuset, get_config_key_value("net_thread_mask", keywords));
    rv = pthread_create(&ids[NET_THREAD], NULL, vegas_net_thread, (void*)&args[NET_THREAD]);
    mask_to_cpuset(&args[LBW_PFB_THREAD].cpuset, get_config_key_value("pfb_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_PFB_THREAD], NULL, vegas_pfb_thread, (void*)&args[LBW_PFB_THREAD]);
    mask_to_cpuset(&args[LBW_ACCUM_THREAD].cpuset, get_config_key_value("accum_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_ACCUM_THREAD], NULL, vegas_accum_thread, (void*)&args[LBW_ACCUM_THREAD]);
#ifdef RAW_DISK
    mask_to_cpuset(&args[LBW_DISK_THREAD].cpuset, get_config_key_value("rawdisk_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_DISK_THREAD], NULL, vegas_rawdisk_thread, (void *)&args[LBW_DISK_THREAD]);
#elif defined NULL_DISK
    mask_to_cpuset(&args[LBW_DISK_THREAD].cpuset, get_config_key_value("null_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_DISK_THREAD], NULL, vegas_null_thread, (void *)&args[LBW_DISK_THREAD]);
#elif defined EXT_DISK
    rv = 0;
#elif FITS_TYPE == PSRFITS
    mask_to_cpuset(&args[LBW_DISK_THREAD].cpuset, get_config_key_value("psrfits_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_DISK_THREAD], NULL, vegas_psrfits_thread, (void *)&args[LBW_DISK_THREAD]);
#elif FITS_TYPE == SDFITS
    mask_to_cpuset(&args[LBW_DISK_THREAD].cpuset, get_config_key_value("sdfits_thread_mask", keywords));
    rv = pthread_create(&ids[LBW_DISK_THREAD], NULL, vegas_sdfits_thread, (void *)&args[LBW_DISK_THREAD]);
#endif

}

extern void *_ZN15VegasFitsThread3runEP17vegas_thread_args(void *);

void start_hbw_mode(struct vegas_thread_args *args, pthread_t *ids) 
{
    // TODO error checking...
    int rv;
    mask_to_cpuset(&args[NET_THREAD].cpuset, get_config_key_value("net_thread_mask", keywords));    
    rv = pthread_create(&ids[NET_THREAD], NULL, vegas_net_thread, (void*)&args[NET_THREAD]);
    mask_to_cpuset(&args[HBW_ACCUM_THREAD].cpuset, get_config_key_value("accum_thread_mask", keywords));
    rv = pthread_create(&ids[HBW_ACCUM_THREAD], NULL, vegas_accum_thread, (void*)&args[HBW_ACCUM_THREAD]);

#ifdef RAW_DISK
    mask_to_cpuset(&args[HBW_DISK_THREAD].cpuset, get_config_key_value("rawdisk_thread_mask", keywords));
    rv = pthread_create(&ids[HBW_DISK_THREAD], NULL, vegas_rawdisk_thread, (void *)&args[HBW_DISK_THREAD]);
#elif defined NULL_DISK
    mask_to_cpuset(&args[HBW_DISK_THREAD].cpuset, get_config_key_value("null_thread_mask", keywords));
    rv = pthread_create(&ids[HBW_DISK_THREAD], NULL, vegas_null_thread, (void *)&args[HBW_DISK_THREAD]);
#elif defined EXT_DISK
    rv = 0;
#elif FITS_TYPE == PSRFITS
    mask_to_cpuset(&args[HBW_DISK_THREAD].cpuset, get_config_key_value("psrfits_thread_mask", keywords));
    rv = pthread_create(&ids[HBW_DISK_THREAD], NULL, vegas_psrfits_thread, (void *)&args[HBW_DISK_THREAD]);
#elif FITS_TYPE == SDFITS
    mask_to_cpuset(&args[HBW_DISK_THREAD].cpuset, get_config_key_value("sdfits_thread_mask", keywords));
    rv = pthread_create(&ids[HBW_DISK_THREAD], NULL, vegas_sdfits_thread, (void *)&args[HBW_DISK_THREAD]);
#endif

}

void start_monitor_mode(struct vegas_thread_args *args, pthread_t *ids) {
    // TODO error checking...
    int rv;
    mask_to_cpuset(&args[NET_THREAD].cpuset, get_config_key_value("net_thread_mask", keywords));
    rv = pthread_create(&ids[NET_THREAD], NULL, vegas_net_thread, (void*)&args[NET_THREAD]);
    mask_to_cpuset(&args[MONITOR_NULL_THREAD].cpuset, get_config_key_value("null_thread_mask", keywords));
    rv = pthread_create(&ids[MONITOR_NULL_THREAD], NULL, vegas_null_thread, (void*)&args[MONITOR_NULL_THREAD]);
}

void stop_threads(struct vegas_thread_args *args, pthread_t *ids, unsigned nthread) 
{
    unsigned i;
    for (i=0; i<nthread; i++) pthread_cancel(ids[i]);
    for (i=0; i<nthread; i++) pthread_kill(ids[i], SIGINT);
    for (i=0; i<nthread; i++) pthread_join(ids[i], NULL);
    for (i=0; i<nthread; i++) vegas_thread_args_destroy(&args[i]);
    for (i=0; i<nthread; i++) ids[i] = 0;
}

int main(int argc, char *argv[]) {

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"resize-obuf", 0, NULL, 'r'},
        {"accumulator-debug", 0, NULL, 'd'},
        {"init-gpu", 0, NULL, 'g'},
        {0,0,0,0}
    };
    int opt, opti;
    /* resize output buffer based on status memory setup */
    int do_dbuf_resize = 0;
    int initialize_gpu_at_startup = 0;
    
    while ((opt=getopt_long(argc,argv,"hrdg",long_opts,&opti))!=-1) {
        switch (opt) {
            default:
            case 'h':
                usage();
                exit(0);
                break;
            case 'r':
                do_dbuf_resize =1;
                break;
            case 'd':
                g_debug_accumulator_thread=1;
                break;
            case 'g':
                initialize_gpu_at_startup = 1;
                break;
        }
    }
    
    prctl(PR_SET_PDEATHSIG,SIGTERM); /* Ensure that if parent (the manager) dies, to kill this child too. */

    /* retain privileges to retain CAP_SYS_NICE for scheduler/affinity control, while dropping
       root privilege (If run by root vs. setuid root, then root is retained.) to real user.
    */
    setup_privileges();
    
    /* Create FIFO */
    int rv = mkfifo(vegas_DAQ_CONTROL, 0666);
    if (rv!=0 && errno!=EEXIST) {
        fprintf(stderr, "vegas_daq_server: Error creating control fifo\n");
        perror("mkfifo");
        exit(1);
    }

    /* Open command FIFO for read */
#define MAX_CMD_LEN 1024
    char cmd[MAX_CMD_LEN];
    int command_fifo;
    command_fifo = open(vegas_DAQ_CONTROL, O_RDONLY | O_NONBLOCK);
    if (command_fifo<0) {
        fprintf(stderr, "vegas_daq_server: Error opening control fifo\n");
        perror("open");
        exit(1);
    }

    /* Attach to shared memory buffers */
    struct vegas_status stat;
    struct vegas_databuf *dbuf_net=NULL, *dbuf_pfb=NULL, *dbuf_acc=NULL;
    rv = vegas_status_attach(&stat);
    const int netbuf_id = 1;
    const int pfbbuf_id = 2;
    const int accbuf_id = 3;
    
    if (rv!=VEGAS_OK) {
        fprintf(stderr, "Error connecting to vegas_status\n");
        exit(1);
    }
    dbuf_net = vegas_databuf_attach(netbuf_id);
    if (dbuf_net==NULL) {
        fprintf(stderr, "Error connecting to vegas_databuf (raw net)\n");
        exit(1);
    }
    vegas_databuf_clear(dbuf_net);
    dbuf_pfb = vegas_databuf_attach(pfbbuf_id);
    if (dbuf_pfb==NULL) {
        fprintf(stderr, "Error connecting to vegas_databuf (pfb output)\n");
        exit(1);
    }
    vegas_databuf_clear(dbuf_pfb);

    dbuf_acc = vegas_databuf_attach(accbuf_id);
    if (dbuf_acc==NULL) {
        fprintf(stderr, "Error connecting to vegas_databuf (accum output)\n");
        exit(1);
    }
    vegas_databuf_clear(dbuf_acc);

    /* Thread setup */
#define MAX_THREAD 8
    int i;
    int nthread_cur = 0;
    struct vegas_thread_args args[MAX_THREAD];
    pthread_t thread_id[MAX_THREAD];
    for (i=0; i<MAX_THREAD; i++) 
    {
        thread_id[i] = 0;
    }
    memset(args, 0, sizeof(args));

    /* Print start time for logs */
    time_t curtime = time(NULL);
    char tmp[256];
    printf("\nvegas_daq_server started at %s", ctime_r(&curtime,tmp));
    fflush(stdout);

    read_thread_configuration(keywords);
    /* hmm.. keep this old signal stuff?? */
    run=1;
    srv_run=1;
    signal(SIGINT, srv_cc);
    signal(SIGTERM, srv_quit);
    
    if (initialize_gpu_at_startup)
    {
        init_cuda_context();
        vegas_status_lock(&stat);
        hputs(stat.buf, "GPUCTXIN", "TRUE");
        vegas_status_unlock(&stat);
    }
    /* Loop over recv'd commands, process them */
    int cmd_wait=1;
    while (cmd_wait && srv_run) {

        // Check to see if threads have exited, if so, stop them
        if (check_thread_exit(args, nthread_cur)) {
            run = 0;
            stop_threads(args, thread_id, nthread_cur);
            nthread_cur = 0;
            vegas_status_lock(&stat);
            hputs(stat.buf, "SCANSTAT", "stopped");
            vegas_status_unlock(&stat);
        }

        // Heartbeat, status update
        time_t curtime;
        char timestr[32];
        char *ctmp;
        time(&curtime);
        ctime_r(&curtime, timestr);
        ctmp = strchr(timestr, '\n');
        if (ctmp!=NULL) { *ctmp = '\0'; } else { timestr[0]='\0'; }
        vegas_status_lock(&stat);
        hputs(stat.buf, "DAQPULSE", timestr);
        hputs(stat.buf, "DAQSTATE", nthread_cur==0 ? "stopped" : "running");
        vegas_status_unlock(&stat);

        // Flush any status/error/etc for logfiles
        fflush(stdout);
        fflush(stderr);

        // Wait for commands on fifo or stdin
        struct pollfd pfd[2];
        pfd[0].fd = command_fifo;
        pfd[0].events = POLLIN;
        pfd[1].fd = fileno(stdin);
        pfd[1].events = POLLIN;

        rv = poll(pfd, 2, 1000);
        if (rv==0) { continue; }
        else if (rv<0) {
            if (errno!=EINTR) perror("poll");
            continue;
        }

        // If we got POLLHUP, it means the other side closed its
        // connection.  Close and reopen the FIFO to clear this
        // condition.  Is there a better/recommended way to do this?
        if (pfd[0].revents==POLLHUP) 
        { 
            close(command_fifo);
            command_fifo = open(vegas_DAQ_CONTROL, O_RDONLY | O_NONBLOCK);
            if (command_fifo<0) {
                fprintf(stderr, 
                        "vegas_daq_server: Error opening control fifo\n");
                perror("open");
                break;
            }
            continue;
        }
        // clear the command
        memset(cmd, 0, MAX_CMD_LEN);
        for (i=0; i<2; ++i)
        {
            rv = 0;
            if (pfd[i].revents & POLLIN)
            {
                if (read(pfd[i].fd, cmd, MAX_CMD_LEN-1)<1)
                    continue;
                else
                {
                    rv = 1;
                    break;
                }
            }
        }


        // rv = read(command_fifo, cmd, MAX_CMD_LEN-1);
        if (rv==0) { continue; }
        else if (rv<0) {
            if (errno==EAGAIN) { continue; }
            else { perror("read");  continue; }
        } 

        // Truncate at newline
        // TODO: allow multiple commands in one read?
        char *ptr = strchr(cmd, '\n');
        if (ptr!=NULL) *ptr='\0'; 

        // Process the command 
        if (strncasecmp(cmd,"QUIT",MAX_CMD_LEN)==0) {
            // Exit program
            stop_threads(args, thread_id, nthread_cur);
            cmd_wait=0;
            printf("Stop observations\n");
            vegas_status_lock(&stat);
            hputs(stat.buf, "SCANSTAT", "stopped");
            vegas_status_unlock(&stat);
            nthread_cur = 0;
            printf("Exit\n");
            run = 0;            
            continue;
        }     
        else if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0 ||
                strncasecmp(cmd,"MONITOR",MAX_CMD_LEN)==0) {
            // Start observations
            // TODO : decide how to behave if observations are running
            printf("Start observations\n");

            if (nthread_cur>0) {
                printf("  observations already running!\n");
            } else {

                // Figure out which mode to start
                char obs_mode[32];
                if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0) {
                    vegas_status_lock(&stat);
                    vegas_read_obs_mode(stat.buf, obs_mode);
                    vegas_status_unlock(&stat);
                } else {
                    strncpy(obs_mode, cmd, 32);
                }
                printf("  obs_mode = %s\n", obs_mode);

                /* Resize the blocks in the disk input buffer, based on the exposure parameter */
                if (do_dbuf_resize)
                    configure_accumulator_buffer_size(&stat, dbuf_acc);

                // Clear out data bufs
                vegas_databuf_clear(dbuf_net);
                vegas_databuf_clear(dbuf_pfb);
                vegas_databuf_clear(dbuf_acc);


                // Do it
                run = 1;
                if (strncasecmp(obs_mode, "HBW", 4)==0) {
                    vegas_status_lock(&stat);
                    hputs(stat.buf, "BW_MODE", "high");
                    hputs(stat.buf, "SWVER", "1.4");
                    hputs(stat.buf, "SCANSTAT", "running");
                    vegas_status_unlock(&stat);
                    init_hbw_mode(args, &nthread_cur);
                    start_hbw_mode(args, thread_id);
                    
                } else if (strncasecmp(obs_mode, "LBW", 4)==0) {                
                    vegas_status_lock(&stat);
                    hputs(stat.buf, "BW_MODE", "low");
                    hputs(stat.buf, "SWVER", "1.4");
                    hputs(stat.buf, "SCANSTAT", "running");
                    vegas_status_unlock(&stat);
                    init_lbw_mode(args, &nthread_cur);
                    start_lbw_mode(args, thread_id);
                } else if (strncasecmp(obs_mode, "MONITOR", 8)==0) {
                    init_monitor_mode(args, &nthread_cur);
                    start_monitor_mode(args, thread_id);
                    vegas_status_lock(&stat);
                    hputs(stat.buf, "SCANSTAT", "stopped");
                    vegas_status_unlock(&stat);
                } else {
                    printf("  unrecognized obs_mode!\n");
                }

            }

        } 
        
        else if (strncasecmp(cmd,"STOP",MAX_CMD_LEN)==0) {
            // Stop observations
            printf("Stop observations\n");
            run = 0;
            stop_threads(args, thread_id, nthread_cur);
            vegas_status_lock(&stat);
            hputs(stat.buf, "SCANSTAT", "stopped");
            vegas_status_unlock(&stat);
            nthread_cur = 0;
        } 
        else if (strncasecmp(cmd, "INIT_GPU", MAX_CMD_LEN)==0) {
            init_cuda_context();
            vegas_status_lock(&stat);
            hputs(stat.buf, "GPUCTXIN", "TRUE");
            vegas_status_unlock(&stat);            
        }
        else {
            // Unknown command
            printf("Unrecognized command '%s'\n", cmd);
        }
    }

    /* Stop any running threads */
    run = 0;
    stop_threads(args, thread_id, nthread_cur);

    if (command_fifo>0) close(command_fifo);

    vegas_status_lock(&stat);
    hputs(stat.buf, "DAQSTATE", "exiting");
    hputs(stat.buf, "SCANSTAT", "stopped");
    vegas_status_unlock(&stat);

    curtime = time(NULL);
    printf("vegas_daq_server exiting cleanly at %s\n", ctime_r(&curtime,tmp));

    fflush(stdout);
    fflush(stderr);

    /* TODO: remove FIFO */

    exit(0);
}
