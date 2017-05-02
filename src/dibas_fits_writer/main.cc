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


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <getopt.h>
extern "C"
{
#include "vegas_error.h"
#include "vegas_status.h"
#include  "fifo.h"
#include "bf_databuf.h"
#include "spead_heap.h"
#include "fitshead.h"
#define STATUS_KEYW "DISKSTAT"
#include "vegas_threads.h"
};

#include "BfFitsIO.h"
#include "mainTest.h"

// #include "vegas_threads.h"
#define FITS_THREAD_CORE 3
#define FITS_PRIORITY (-20)

int run = 0;
int srv_run = 1;
int start_flag=0;

extern "C" void *runGbtFitsWriter(void *);

void usage() {
    fprintf(stderr,
            "Usage: vegasFitsWriter (options) \n"
            "Options:\n"
            "  -t , --test          run a test\n"
            "  -m , --mode          'c' for Cov. Matrix, 'p' for Pulsar\n"
            "  -i n, --instance=nn  instance id\n"
            );
}


pthread_t thread_id = 0;

void signal_handler(int sig)
{
    if (signal(sig, SIG_IGN) == SIG_ERR)
    {
        printf("System error: signal\n");
        run = 0;
    }

    switch (sig)
    {
    case SIGTERM:
        printf("Exiting on SIGTERM\n");
        run = 0;
    break;
    case SIGHUP:
        printf("Got a sighup -- ignored\n");
    break;
    case SIGINT:
        printf("Exiting on a SIGINT\n");
        run = 0;
    break;
    case SIGQUIT:
        printf("Exiting on a SIGQUIT\n");
        run = 0;
    break;
    }
    if (run == 0)
    {
        pthread_cancel(thread_id);
    }
}

const int MAX_CMD_LEN =64;

extern "C" int setup_privileges();

int mainThread(bool cov_mode1,bool cov_mode2,bool cov_mode3, int instance_id, int argc, int multiFITS, char **argv)
{

    // create command fifo based on username and instance_id
    char command_fifo_filename[MAX_CMD_LEN];
    char *user = getenv("USER");
    char fifo_cmd[128];
    sprintf(fifo_cmd, "touch /tmp/fits_fifo_%s_%d", user, instance_id);
    system(fifo_cmd);
    sprintf(command_fifo_filename, "/tmp/fits_fifo_%s_%d", user, instance_id);
    printf("%s\n",command_fifo_filename);

    run = 1;
    int rv;

    signal(SIGHUP, signal_handler);     // hangup
#if !defined(DEBUG)                     // when debugging, wish to use CTRL-C
    signal(SIGINT, signal_handler);     // interrupt
#endif
    signal(SIGQUIT, signal_handler);    // quit
    signal(SIGTERM, signal_handler);    // software termination
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);           // un-zombify child processes
    // If our process parent exits/dies, kernel should send us SIGKILL
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    setup_privileges();

    int fits_fifo_id = open_fifo(command_fifo_filename);
    int cmd = INVALID;   
    
/* Set cpu affinity */
    cpu_set_t cpuset, cpuset_orig;
    sched_getaffinity(0, sizeof(cpu_set_t), &cpuset_orig);
    CPU_ZERO(&cpuset);
    CPU_SET(FITS_THREAD_CORE, &cpuset);
    rv = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if (rv<0) {
        perror("sched_setaffinity");
    }

    /* Set priority */
    rv = setpriority(PRIO_PROCESS, 0, FITS_PRIORITY);
    if (rv<0) {
        perror("set_priority");
    }

    printf("vegas_fits_writer started\n");

    run=1;
    srv_run=1;

    /* Loop over recv'd commands, process them */
    int cmd_wait=1;
    //initialize FIFO loop variables
    int n = 0;
    int n_loop = 1000;
    int scan_num = 0;
    
       
    while (cmd_wait)
    {
        cmd = INVALID;
        // Check to see if threads have exited
        if (thread_id != 0 && pthread_kill(thread_id, 0)!=0)
        {
            
            printf("writer thread exited unexpectedly\n");
            thread_id = 0;
            run = 0;
        }

        // Flush any status/error/etc for logfiles
        fflush(stdout);
        fflush(stderr);

	if (n++ >= n_loop){
		cmd = check_cmd(fits_fifo_id);
		n = 0;
	}	
        // Process the command
        if (cmd == START)
        {
		printf("Start observations\n");
		if (thread_id != 0)
		{
			printf("observations already running!\n");
		}
            	// Start observations
            	// TODO : decide how to behave if observations are running
            	// pass on args 
                else
		{
			if (multiFITS == 0)
			{
				run = 1;
				vegas_thread_args *args = new vegas_thread_args;
            			args->input_buffer = instance_id;
                		args->cov_mode1 = (int)cov_mode1;
                		args->cov_mode2 = (int)cov_mode2;
          	  		args->cov_mode3 = (int)cov_mode3;
            			pthread_create(&thread_id, NULL, runGbtFitsWriter, (void *)args);
			}
			else if (multiFITS == 1)
			{
				run = 1;
                        	vegas_thread_args *args = new vegas_thread_args;
                        	args->input_buffer = instance_id;
                        	args->cov_mode1 = (int)cov_mode1;
                        	args->cov_mode2 = (int)cov_mode2;
                        	args->cov_mode3 = (int)cov_mode3;
                        	pthread_create(&thread_id, NULL, runGbtFitsWriter, (void *)args);
				//run concurrent HI FITS thread
   				cov_mode1 = true;
                        	args->input_buffer = instance_id;
                        	args->cov_mode1 = (int)cov_mode1;
                        	args->cov_mode2 = (int)cov_mode2;
                        	args->cov_mode3 = (int)cov_mode3;
                        	pthread_create(&thread_id, NULL, runGbtFitsWriter, (void *)args);
			}
			
			else if (multiFITS ==2)
			{
		        	run = 1;
                        	vegas_thread_args *args = new vegas_thread_args;
                        	args->input_buffer = instance_id;
                        	args->cov_mode1 = (int)cov_mode1;
                        	args->cov_mode2 = (int)cov_mode2;
                        	args->cov_mode3 = (int)cov_mode3;
                        	pthread_create(&thread_id, NULL, runGbtFitsWriter, (void *)args);
				//run concurrent FRB FITS thread
				cov_mode2 = true;
                        	args->input_buffer = instance_id;
                        	args->cov_mode1 = (int)cov_mode1;
                        	args->cov_mode2 = (int)cov_mode2;
                        	args->cov_mode3 = (int)cov_mode3;
                        	pthread_create(&thread_id, NULL, runGbtFitsWriter, (void *)args);	
			}
                }
        }
        else if ((cmd == STOP) || (cmd == QUIT))
        {
            // Stop observations
            printf("Stop observations\n");
            pthread_kill(thread_id, SIGINT);
	    run = 0;
            cmd_wait=0;
	    continue;
        }
    }

    /* Stop any running threads */

    run = 0;
    if (fits_fifo_id>0)
    {
        close(fits_fifo_id);
    }

    time_t curtime = time(NULL);
    char tmp[256];

    printf("vegas_fits_writer exiting cleanly at %s\n", ctime_r(&curtime,tmp));

    fflush(stdout);
    fflush(stderr);

    /* TODO: remove FIFO */

    exit(0);
}





int main(int argc, char **argv) {

    static struct option long_opts[] = {
        {"help",   0, NULL, 'h'},
        {"test",   0, NULL, 't'},
        {"mode",   1, NULL, 'm'},
        {"instance",   1, NULL, 'i'},
        {0,0,0,0}
    };

    int opt, opti;
    int instance_id = 0;
    int multiFITS = 0;
    bool test = false;
    // hi corr
    bool cov_mode1 = true;
    //cal corr
    bool cov_mode2 = true;
    //frb corr
    bool cov_mode3 = true;
    //pulsar rt-beamformer
    bool cov_mode4 = true;
    //hi+pulsar mode (concurrent FITS writers)
    bool cov_mode5 = true; 
    //frb+pulsar mode (concurrent FITS writers)
    bool cov_mode6 = true;
    char cov_mode1_value = 's';
    char cov_mode2_value = 'c';
    char cov_mode3_value = 'f';
    char cov_mode4_value = 'p';
    char cov_mode5_value = 'a';
    char cov_mode6_value = 'b';
    while ((opt=getopt_long(argc,argv,"htm:i:",long_opts,&opti))!=-1) {
        switch (opt) {
            case 't':
                test = true;
                break;
            case 'm':    
                cov_mode1 = (cov_mode1_value == *optarg);
                cov_mode2 = (cov_mode2_value == *optarg);
                cov_mode3 = (cov_mode3_value == *optarg);
                cov_mode4 = (cov_mode4_value == *optarg);
                cov_mode5 = (cov_mode5_value == *optarg);
		cov_mode6 = (cov_mode6_value == *optarg);
                break;
            case 'i':    
                instance_id = atoi(optarg);
                break;
            case 'h':
            default:
                usage();
                exit(0);
                break;
        }
    }


    if (test)
        mainTest(cov_mode1, argc, argv);
    if(cov_mode1){
        printf("RUNNING SPECTRAL MODE\n");
        mainThread(cov_mode1,false,false,instance_id, argc, multiFITS, argv);
        }
    else if (cov_mode2){
        printf("RUNNING PAF MODE\n");
        mainThread(false,cov_mode2,false,instance_id, argc, multiFITS, argv);
        }
    else if (cov_mode3){
        printf("RUNNING FRB MODE\n");
        mainThread(false,false,cov_mode3,instance_id, argc, multiFITS, argv);
        }
    else if (cov_mode4){
        printf("RUNNING PULSAR MODE\n");
        mainThread(false,false,false, instance_id, argc, multiFITS, argv);
        }
    else if (cov_mode5){
	printf("RUNNING SPECTRAL+PULSAR MODE\n");
	multiFITS = 1;
	mainThread(false,false,false, instance_id, argc, multiFITS, argv);
        }
    else{
        printf("RUNNING FRB+PULSAR MODE\n");
        multiFITS = 2;
        mainThread(false,false,false, instance_id, argc, multiFITS, argv);
        }

    return (0);
}
