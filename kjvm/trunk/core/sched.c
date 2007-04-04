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
//==============================================================================
// sched.c
// 
// Jem/JVM scheduler emulation
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "execjava.h"
#include "portal.h"
#include "thread.h"

#define PORTAL_CALL     1
#define PORTAL_RETURN   2

void Sched_block(u32 state)
{
    RT_TASK_MCB     mcb_r;
    int             result;

    mcb_r.data      = NULL;
    mcb_r.size      = 0;
    curthr()->state = state;

    do {
        result = rt_task_receive(&mcb_r, TM_INFINITE);
        rt_task_reply(result, NULL);
    } while ( mcb_r.opcode != state );

    curthr()->state     = STATE_RUNNABLE;
}


/* this function is called by a portal receiver that wishes to handoff the timeslot to 
 * thread "sender"
 * isRunnable: 1=this thread is runnable, 0=this thread is in state PORTAL_WAIT_FOR_SND
 */

void Sched_portal_handoff_to_sender(ThreadDesc *sender)
{
    RT_TASK_MCB     mcb_s;

    mcb_s.data      = NULL;
    mcb_s.size      = 0;
    mcb_s.opcode    = sender->state;
    curthr()->state = STATE_PORTAL_WAIT_FOR_RET;

    // The following message will cause this task to block
    // until the receiver replies.
    rt_task_send(&sender->task, &mcb_s, NULL, TM_INFINITE);
    curthr()->state = STATE_RUNNABLE;
}

