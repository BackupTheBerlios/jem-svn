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
// jemMQ.c
// 
// Jem/JVM message queue processing.
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/queue.h>
#include "jemMQ.h"
#include "messages.h"


#define JEMTIMEOUT 1000

static RT_QUEUE     mqDesc;
unsigned int        mqSize=16384;


int jemInitMQ(void)
{
    int     result;
    void    *bufp;

    // Open the user space message queue
    result = rt_queue_create(&mqDesc, "jemMQ", (ssize_t) mqSize, Q_UNLIMITED, Q_PRIO|Q_SHARED);
    if (result) {
        printk(KERN_ERR "Jem/JVM - Failed to create JVM message queue, code=%d\n", result);
        return result;
    }

    return 0;
}


int jemQHandshake(void)
{
    struct jvmMessage   *msg;
    int                 result;

    if ((msg = jemQAlloc(sizeof(struct jvmMessage), __FILE__, __LINE__)) == NULL) {
        return -1;
    }

    msg->cmd = TESTMSG;
    if ((result = jemQSend((void *) msg, sizeof(struct jvmMessage), Q_NORMAL, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    printk(KERN_INFO "Jem/JVM waiting for user space handshake.\n");

    if ((result = jemQReceiveBlk((void **) &msg, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    if (msg->cmd != TESTMSGREPLY) {
        printk(KERN_ERR "Jem/JVM user space message queue handshake failed, cmd=%d.\n", msg->cmd);
        jemQFree(msg);
        return -1;
    }
    printk(KERN_INFO "Jem/JVM user space handshake complete, initialization proceeding.\n");

    jemQFree(msg);
    return 0;
}


int jemDestroyMQ(void)
{
    return rt_queue_delete(&mqDesc);
}


void *jemQAlloc(size_t size, char *fName, unsigned int fLine)
{
    void *result;

    if ((result = rt_queue_alloc(&mqDesc, size)) == NULL)
        printk(KERN_ERR "Jem/JVM - Out of message queue memory at %s:%d\n", fName, fLine);
    return result;
}


int jemQSend(void *mbuf, size_t size, int mode, char *fName, unsigned int fLine)
{
    int result;

    if ((result = rt_queue_send(&mqDesc, mbuf, size, mode)) < 0)
        printk(KERN_ERR "Jem/JVM - Can not send user space request at %s:%d\n", fName, fLine);
    return result;
}


static ssize_t jemQReceive(void **bufp, RTIME timeout, char *fName, unsigned int fLine)
{
    ssize_t result;

    if ((result = rt_queue_receive(&mqDesc, bufp, timeout)) < 0) 
        printk(KERN_ERR "Jem/JVM - Error waiting for user space request answer at %s:%d\n", fName, fLine);
    return result;
}


ssize_t jemQReceiveBlk(void **bufp, char *fName, unsigned int fLine)
{
    return jemQReceive(bufp,TM_INFINITE,fName,fLine);
}


ssize_t jemQReceiveTO(void **bufp, char *fName, unsigned int fLine)
{
    return jemQReceive(bufp,JEMTIMEOUT,fName,fLine);
}


ssize_t jemQReceiveNB(void **bufp, char *fName, unsigned int fLine)
{
    return jemQReceive(bufp,TM_NONBLOCK,fName,fLine);
}


void jemQFree(void *buf)
{
    rt_queue_free(&mqDesc, buf);
}


