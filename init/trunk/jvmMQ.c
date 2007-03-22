//==============================================================================
// jvmMP.c
// 
// Jem/JVM message pipe processing.
// 
//==============================================================================

#include <native/task.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "jvmConfig.h"
#include "jvmMQ.h"


static int          pipeFD;
static RT_TASK      mqTaskDesc;

#define MQSTACKSZ 16384
#define MQSIZE    16384
#define MQPRIO    90


static void processConfigMessage(void)
{
    struct jvmConfigMsg reply;

    reply.data.codeBytesDom0       = getIntVal("codeBytesDom0");
    reply.data.domScratchMemSz     = getIntVal("domScratchMemSz");
    reply.data.heapBytesDom0       = getIntVal("heapBytesDom0");
    reply.data.maxDomains          = getIntVal("maxDomains");
    reply.data.maxNumberLibs       = getIntVal("maxNumberLibs");
    reply.cmd                      = GETCONFIG;

    write(pipeFD, &reply, sizeof(struct jvmConfigMsg));
}


static void processTestMessage(void)
{
    struct jvmMessage   reply;

    reply.cmd = TESTMSGREPLY;
    write(pipeFD, &reply, sizeof(struct jvmMessage));
}


// A task to process JVM commands from the message pipe
static void mqTask(void * cookie)
{
    struct jvmMessage   msg;

    for (;;) {
        if (read(pipeFD, &msg, sizeof(struct jvmMessage)) == -1) {
            printf("Error reading message pipe, rc=%d\n", errno);
        }
        switch (msg.cmd) {
        case TESTMSG:
            processTestMessage();
            break;
        case GETCONFIG:
            processConfigMessage();
            break;
        default:
            printf("Unknown Jem/JVM message received, cmd=%d.\n", msg.cmd);
            break;
        }
    }
}


int jvmPipeInit(void)
{
    int result;
    int prio;

    if ((pipeFD = open(getStringVal("messagePipeName"), O_RDWR)) < 0) {
        printf("Error opening Jem/JVM message pipe %s, rc=%d\n", getStringVal("messagePipeName"), errno);
        return pipeFD;
    }

    // Create the message queue task.
    if ((prio = getIntVal("MQPriority")) == 0) {
        printf("Default MQPriority is %d\n", MQPRIO);
        prio = MQPRIO;
    }

    result = rt_task_create(&mqTaskDesc,"mqTask",MQSTACKSZ,prio,T_FPU|T_JOINABLE);
    if (result) {
        printf("Failed to create message queue task, code=%d \n", result);
        return result;
    }

    result = rt_task_start(&mqTaskDesc,&mqTask,NULL);
    if (result) {
        printf("Failed to start message queue task, code=%d \n", result);
        return result;
    }

    return 0;
}


void jvmPipeDestroy(void)
{
    rt_task_delete(&mqTaskDesc);
    close(pipeFD);
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

