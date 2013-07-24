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
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

#include "VegasFitsIO.h"

int run = 0;
int srv_run = 1;

extern "C" void *runGbtFitsWriter(void *);

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
const char CONTROL_FIFO[] = "/tmp/vegas_fits_control";

int main(int argc, char **argv)
{
    run = 1;
    int command_fifo;
    int rv;
    char cmd[MAX_CMD_LEN];
    int i;

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
    
    command_fifo = open(CONTROL_FIFO, O_RDONLY | O_NONBLOCK);
    if (command_fifo<0) 
    {
        fprintf(stderr, "vegas_fits_writer: Error opening control fifo %s\n", CONTROL_FIFO);
        perror("open");
        exit(1);
    }
    printf("vegas_fits_writer started\n");

    run=1;
    srv_run=1;
    
    /* Loop over recv'd commands, process them */
    int cmd_wait=1;
    while (cmd_wait && srv_run) 
    {

        // Check to see if threads have exited
        if (thread_id != 0 && pthread_kill(thread_id, 0)!=0) 
        {
            run = 0;
            // printf("writer thread exited unexpectedly\n");
            thread_id = 0;
        }

        // Flush any status/error/etc for logfiles
        fflush(stdout);
        fflush(stderr);

        // Wait for data on fifo
        struct pollfd pfd[2];
        pfd[0].fd = command_fifo;
        pfd[0].events = POLLIN;
        pfd[1].fd = fileno(stdin);
        pfd[1].events = POLLIN;
        rv = poll(pfd, 2, 1000);
        if (rv==0) 
        { 
            continue; 
        }
        else if (rv<0) 
        {
            if (errno!=EINTR) 
            {
                perror("poll");
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

        // If we got POLLHUP, it means the other side closed its
        // connection.  Close and reopen the FIFO to clear this
        // condition.  Is there a better/recommended way to do this?
        if (pfd[0].revents==POLLHUP) 
        { 
            close(command_fifo);
            command_fifo = open(CONTROL_FIFO, O_RDONLY | O_NONBLOCK);
            if (command_fifo<0) 
            {
                fprintf(stderr, 
                        "vegas_fits_writer: Error opening control fifo\n");
                perror("open");
                break;
            }
            continue;
        }

        if (rv==0) 
        { 
            continue; 
        }
        else if (rv<0) 
        {
            if (errno==EAGAIN) 
            { 
                continue; 
            }
            else 
            { 
                perror("read");  
                continue; 
            }
        } 

        // Truncate at newline
        // TODO: allow multiple commands in one read?
        char *ptr = strchr(cmd, '\n');
        if (ptr!=NULL) 
        {
            *ptr='\0'; 
        }

        // Process the command         
        if (strncasecmp(cmd,"START",MAX_CMD_LEN)==0) 
        {
            // Start observations
            // TODO : decide how to behave if observations are running
            printf("Start observations\n");
            if (thread_id != 0) 
            {
                printf("  observations already running!\n");
            } 
            else 
            {
                run = 1;
                pthread_create(&thread_id, NULL, runGbtFitsWriter, 0);
            }
        }       
        else if (strncasecmp(cmd,"STOP",MAX_CMD_LEN)==0 || 
                 strncasecmp(cmd,"QUIT",MAX_CMD_LEN)==0) 
        {
            // Stop observations
            printf("Stop observations\n");
            run = 0;
            if (thread_id && pthread_kill(thread_id, 0) == 0)
            {
                pthread_cancel(thread_id);
                pthread_kill(thread_id, SIGINT);
                pthread_join(thread_id, NULL);
                thread_id = 0;
            }
            
            if (strncasecmp(cmd,"QUIT",MAX_CMD_LEN)==0)
            {
                cmd_wait=0;
                continue;
            }
        }         
        else 
        {
            // Unknown command
            printf("Unrecognized command '%s'\n", cmd);
        }
    }

    /* Stop any running threads */
    run = 0;

    if (command_fifo>0) 
    {
        close(command_fifo);
    }
    
    time_t curtime = time(NULL);
    char tmp[256];

    printf("vegas_fits_writer exiting cleanly at %s\n", ctime_r(&curtime,tmp));

    fflush(stdout);
    fflush(stderr);

    /* TODO: remove FIFO */

    exit(0);
}
