//==============================================================================
// jemUPipe.c
// 
// Jem/JVM user space pipe processing.
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/pipe.h>
#include "jemUPipe.h"
#include "messages.h"


#define JEMTIMEOUT 1000

static RT_PIPE      mqDesc;
unsigned int        mqSize=16384;


int jemInitUPipe(void)
{
    int     result;

    // Open the user space pipe
    result = rt_pipe_create(&mqDesc, "jemUPipe", P_MINOR_AUTO, mqSize);
    if (result) {
        printk(KERN_ERR "Jem/JVM - Failed to create JVM user space pipe, code=%d\n", result);
        return result;
    }

    return 0;
}


static struct jvmMessage testRequest = {
    .cmd = TESTMSG,
};

int jemUPHandshake(void)
{
    RT_PIPE_MSG         *msg;
    struct jvmMessage   *testreply;
    int                 result;

    if ((msg = jemUPAlloc(sizeof(struct jvmMessage), __FILE__, __LINE__)) == NULL) {
        return -1;
    }

    memcpy(P_MSGPTR(msg), &testRequest, sizeof(struct jvmMessage));
    if ((result = jemUPSend(msg, sizeof(struct jvmMessage), P_NORMAL, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    printk(KERN_INFO "Jem/JVM waiting for user space handshake.\n");

    if ((result = jemUPReceiveBlk(&msg, __FILE__, __LINE__)) < 0) {
        return -1;
    }

    testreply = (struct jvmMessage *) P_MSGPTR(msg);
    if (testreply->cmd != TESTMSGREPLY) {
        printk(KERN_ERR "Jem/JVM user space pipe handshake failed, cmd=%d.\n", testreply->cmd);
        jemUPFree(msg);
        return -1;
    }
    printk(KERN_INFO "Jem/JVM user space handshake complete, initialization proceeding.\n");

    jemUPFree(msg);
    return 0;
}


int jemDestroyUPipe(void)
{
    return rt_pipe_delete(&mqDesc);
}


RT_PIPE_MSG *jemUPAlloc(size_t size, char *fName, unsigned int fLine)
{
    RT_PIPE_MSG *result;

    if ((result = rt_pipe_alloc(&mqDesc, size)) == NULL)
        printk(KERN_ERR "Jem/JVM - Out of user space message pipe memory at %s:%d\n", fName, fLine);
    return result;
}


int jemUPSend(RT_PIPE_MSG *mbuf, size_t size, int mode, char *fName, unsigned int fLine)
{
    int result;

    if ((result = rt_pipe_send(&mqDesc, mbuf, size, mode)) < 0)
        printk(KERN_ERR "Jem/JVM - Can not send user space request at %s:%d\n", fName, fLine);
    return result;
}


static ssize_t jemUPReceive(RT_PIPE_MSG **bufp, RTIME timeout, char *fName, unsigned int fLine)
{
    ssize_t result;

    if ((result = rt_pipe_receive(&mqDesc, bufp, timeout)) < 0) 
        printk(KERN_ERR "Jem/JVM - Error waiting for user space request answer at %s:%d\n", fName, fLine);
    return result;
}


ssize_t jemUPReceiveBlk(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine)
{
    return jemUPReceive(bufp,TM_INFINITE,fName,fLine);
}


ssize_t jemUPReceiveTO(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine)
{
    return jemUPReceive(bufp,JEMTIMEOUT,fName,fLine);
}


ssize_t jemUPReceiveNB(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine)
{
    return jemUPReceive(bufp,TM_NONBLOCK,fName,fLine);
}


void jemUPFree(RT_PIPE_MSG *buf)
{
    rt_pipe_free(&mqDesc, buf);
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
// at http://www.javadevices.com
//=================================================================================

