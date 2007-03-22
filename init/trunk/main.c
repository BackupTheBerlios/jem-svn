//==============================================================================
// main.c
// 
// Jem/JVM user space component main program.
// 
//==============================================================================

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <native/task.h>
#include "libcli.h"
#include "version.h"
#include "jvmConfig.h"
#include "jvmMQ.h"

static struct cli_def       *cli;
static int                  socketFD;
static struct sockaddr_in   serverAddr;
static RT_TASK              cliTask;
static unsigned long        upTime;

#define SLEEPINTVL 10


// Command to print current uptime
int cmdUptime(struct cli_def * cli, char * command, char * argv[], int argc)
{
    char uptimeS[80];

    sprintf(uptimeS, "Uptime=%ld seconds, resolution is %3d seconds.", upTime, SLEEPINTVL);
    cli_print(cli, uptimeS);
    return CLI_OK;
}


// A task to process CLI commands.
void commandTask(void * cookie)
{
    int connectFD;

    while ((connectFD = accept(socketFD, NULL, 0)))
    {
        cli_loop(cli, connectFD);
        close(connectFD);
    }
}


void cliShutdown(void) {
    close(socketFD);
    cli_done(cli);
    rt_task_delete(&cliTask);
}


void sigShutdownHandler(int cookie)
{
    if (getIntVal("EnableCLI")) {
        // Wait for command line interface to shut down
        cliShutdown();
    }

    jvmPipeDestroy();

    printf("Jem/JVM is shutdown.\n");
    exit(0);
}


int jemCLIinit(void)
{
    int on = 1;

    cli = cli_init();
    cli_set_banner(cli, "Jem/JVM Command Line Interface");
    cli_set_hostname(cli, "Jem/JVM");
    cli_register_command(cli, NULL, "uptime", cmdUptime, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Display uptime.");

    if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error for CLI, result=%d.\n", errno);
        return errno;
    }
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int cliPort = getIntVal((char *) "CLIPORT");
    if (cliPort == 0) cliPort = 8181;
    serverAddr.sin_port = htons(cliPort);
    if (bind(socketFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("Socket bind error for CLI, result=%d.\n", errno);
        return errno;
    }

    if (listen(socketFD, 50) < 0) {
        printf("Socket listen error for CLI, result=%d.\n", errno);
        return errno;
    }

    int result = rt_task_create(&cliTask,"cliTask",0,1,T_FPU|T_JOINABLE);
    if (result) {
        printf("Failed to create cli task, code=%d \n", result);
        return -1;
    }

    result = rt_task_start(&cliTask,&commandTask,NULL);
    if (result) {
        printf("Failed to start cli task, code=%d \n", result);
        return -1;
    }

    return 0;
}

int main (int argc, char *argv[])
{
    sigset_t    set, oldset;
    int         result;

    printf("Jem/JVM version %1d.%1d.%1d, built on: %s, by: %s\n", VERSION_NUM, REVISION_NUM, BUILD_NUM, BUILD_DATE, USER_HOST);
    printf("Copyright (C) 2007, Sombrio Systems Inc.\n");

    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, &oldset);
    signal(SIGQUIT, sigShutdownHandler);
    signal(SIGINT, sigShutdownHandler);

    mlockall(MCL_CURRENT|MCL_FUTURE);

    // Load run time configuration values
    jvmConfig();

    if (getIntVal("EnableCLI")) {
        // Intialize the command line interface
        if (jemCLIinit()) {
            printf("CLI is not enabled, use Ctrl-C to quit.\n");
        }
        else {
            printf("CLI is enabled, use Ctrl-C to quit.\n");
        }
    }
    else {
        printf("CLI is not enabled, use Ctrl-C to quit.\n");
    }

    // Initialize the message queue
    if (jvmPipeInit()) {
        sigShutdownHandler(0);
        exit(1);
    }

    printf("Jem/JVM ready.\n");

    for (;;) {
        sleep(SLEEPINTVL);
        upTime += SLEEPINTVL;
    }
}


//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
//
// Jem is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License, version 2, as published by the Free 
// Software Foundation.
//
// Jem is distributed in the hope that it will be useful, but WITHOUT ANY 
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR 
// A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with 
// Jem; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, 
// Fifth Floor, Boston, MA 02110-1301, USA
//
// As a special exception, if other files instantiate templates or use macros or 
// inline functions from this file, or you compile this file and link it with other 
// works to produce a work based on this file, this file does not by itself cause 
// the resulting work to be covered by the GNU General Public License. However the 
// source code for this file must still be made available in accordance with 
// section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this
// file might be covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.sombrio.com
//=================================================================================

