//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
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
//=================================================================================
// jem.c
// 
// Jem/JVM kernel module main.
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/initrd.h>
#include <linux/moduleparam.h>
#include <native/task.h>
#include <native/pipe.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "malloc.h"
#include "jemUPipe.h"
#include "jar.h"
#include "domain.h"
#include "messages.h"

#define VERSION "1.0.0"
#define INITTASKPRIO 98
#define INITTASKSTKSZ 8192
#define DEFAULTHPSZ 33554432


static RT_TASK          initTaskDesc;
static struct jvmConfig jvmConfigData;
extern unsigned int     mqSize;
extern unsigned int     jemHeapSz;

module_param(mqSize, uint, 444);
MODULE_PARM_DESC (mqSize, "Amount of memory to allocate for message queue (default 16K)");
module_param(jemHeapSz, uint, 444);
MODULE_PARM_DESC(jemHeapSz, "Amount of memory to allocate for the JVM.");


static struct jvmMessage configRequest = {
    .cmd    = GETCONFIG,
};

static int readJVMConfig(void)
{
    struct jvmConfigMsg *reply;
    RT_PIPE_MSG         *msg;
    int                 result;

    if ((msg = jemUPAlloc(sizeof(struct jvmMessage), __FILE__, __LINE__)) == NULL) {
        return -1;
    }

    memcpy(P_MSGPTR(msg), &configRequest, sizeof(struct jvmMessage));
    if ((result = jemUPSend(msg, sizeof(struct jvmMessage), P_NORMAL, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    if ((result = jemUPReceiveBlk(&msg, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    reply = (struct jvmConfigMsg *) P_MSGPTR(msg);
    if (reply->cmd != GETCONFIG) {
        printk(KERN_ERR "Jem/JVM - Invalid JVM configuration received.\n");
        return -1;
    }

    memcpy(&jvmConfigData, &reply->data, sizeof(struct jvmConfig));
    jemUPFree(msg);
    return 0;
}


struct jvmConfig *getJVMConfig(void)
{
    return &jvmConfigData;
}


static void jvmInitTask(void *cookie)
{
    if (jemUPHandshake()) return;

    if (readJVMConfig()) return;

    jarInit((char *) initrd_start, initrd_end - initrd_start);

    initDomainSystem();

    printk(KERN_INFO "Jem/JVM initialization complete.\n");

    return;
}

void jem_exit (void)
{
    deleteDomainSystem();
    jemDestroyUPipe();
    jemDestroyHeap();
    printk(KERN_INFO "Jem/JVM is shutdown.\n");
}

int jem_init (void)
{
    int result;

    printk(KERN_INFO "Jem/JVM version %s\n", VERSION);

    if (jemHeapSz == 0) {
        jemHeapSz   = DEFAULTHPSZ;
        printk(KERN_INFO "Jem/JVM - Default heap size of %d will be used.\n", jemHeapSz);
        printk(KERN_INFO "Jem/JVM - Use jemHeapSz parameter to change default.\n");
        printk(KERN_INFO "Jem/JVM - Eg. insmod jemjvm.ko jemHeapSz=33554432\n");
    }

    // Create the jvm heap
    if ((result = jemMallocInit()) < 0) return result;

    // Create the user space message queue
    if ((result = jemInitUPipe()) < 0) {
        jemDestroyHeap();
        return result;
    }

    result = rt_task_create(&initTaskDesc,"kInitTask",INITTASKSTKSZ,INITTASKPRIO,T_JOINABLE);
    if (result) {
        printk(KERN_ERR "Jem/JVM - Failed to create JVM init task, code=%d \n", result);
        jemDestroyUPipe();
        jemDestroyHeap();
        return result;
    }

    result = rt_task_start(&initTaskDesc,&jvmInitTask,NULL);
    if (result) {
        printk(KERN_ERR "Jem/JVM - Failed to start JVM init task, code=%d \n", result);
        jemDestroyUPipe();
        jemDestroyHeap();
        return result;
    }

    return 0;
}

MODULE_AUTHOR("Chris Stone");
MODULE_LICENSE("GPL and additional rights");
MODULE_DESCRIPTION("A Java Virtual Machine for embedded devices.");
MODULE_VERSION(VERSION);
module_init(jem_init);
module_exit(jem_exit);


