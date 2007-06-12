//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
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
// zero_CPUState.c
//
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "vmsupport.h"
#include "load.h"
#include "exception_handler.h"
#include "zero.h"
#include "malloc.h"
#include "gc_alloc.h"

extern ClassDesc *cpuStateClass;
extern ClassDesc *stackClass;
extern ClassDesc *domainClass;
extern ClassDesc *domainZeroClass;

static jint cpuState_getState(ObjectDesc * self)
{
    ThreadDesc *thread = cpuState2thread(self);
    return thread->state;
}
static jboolean cpuState_isPortalThread(ObjectDesc * self)
{
    ThreadDesc *thread = cpuState2thread(self);
    return thread->isPortalThread;
}
static ObjectDesc *cpuState_setNext(ObjectDesc * self, ObjectDesc * next)
{
    ObjectDesc *result;
    ThreadDesc *thread = cpuState2thread(self);
    if (thread == NULL)
        return NULL;
    result = thread2CPUState(thread->next);

    thread->next = cpuState2thread(next);
    return result;
}
static ObjectDesc *cpuState_getNext(ObjectDesc * self)
{
    ObjectDesc *result;
    ThreadDesc *thread = cpuState2thread(self);
    if (thread == NULL)
        return NULL;
    result = thread2CPUState(thread->next);
    return result;
}

MethodInfoDesc cpuStateMethods[] = {
    {"getState", "", (code_t) cpuState_getState}
    ,
    {"isPortalThread", "", (code_t) cpuState_isPortalThread}
    ,
    {"setNext", "", (code_t) cpuState_setNext}
    ,
    {"getNext", "", (code_t) cpuState_getNext}
    ,
};

static jbyte cpuStateTypeMap[] = { 0, 0 };

MethodInfoDesc stackMethods[] = {
};

void init_cpustate_portal(void)
{
    cpuStateClass =
        init_zero_class("jx/zero/CPUState", cpuStateMethods, sizeof(cpuStateMethods), 1, cpuStateTypeMap,
                "<jx/zero/CPUState>");
}
