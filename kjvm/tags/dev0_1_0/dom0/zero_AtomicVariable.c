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
// zero_AtomicVariable.c
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

/*
 * Atomic Variable DEP
 */
ClassDesc *atomicvariableClass = NULL;

static void atomicvariable_set(AtomicVariableProxy * self, ObjectDesc * value)
{
    self->value = value;
}

static ObjectDesc *atomicvariable_get(AtomicVariableProxy * self)
{
    return self->value;
}

void atomicvariable_atomicUpdateUnblock(AtomicVariableProxy * self, ObjectDesc * value, CPUStateProxy * cpuStateProxy)
{
    self->value = value;

    if (self->listMode == 1) {
        ThreadDesc *t;
        for (t = self->blockedThread; t != NULL; t = t->next)
            threadunblock(t);
        self->blockedThread = NULL;
    } else {
        if (cpuStateProxy != NULL && self->blockedThread != NULL) {
            ThreadDesc *cpuState = cpuState2thread((ObjectDesc *) cpuStateProxy);
            if (cpuState->state == STATE_BLOCKEDUSER) {
                self->blockedThread = NULL;
                threadunblock(cpuState);
            }
        }
    }
}

static void atomicvariable_blockIfEqual(AtomicVariableProxy * self, ObjectDesc * test)
{
    if (self->value == test) {
        ThreadDesc *thread = curthr();
        if (self->listMode == 1) {
            thread->next = self->blockedThread;
            self->blockedThread = thread;
        } else {
            self->blockedThread = thread;
        }
        threadblock();
    }
}

static void atomicvariable_blockIfNotEqual(AtomicVariableProxy * self, ObjectDesc * test)
{
    if (self->value != test) {
        ThreadDesc *thread = curthr();
        if (self->listMode == 1) {
            thread->next = self->blockedThread;
            self->blockedThread = thread;
        } else {
            self->blockedThread = thread;
        }
        threadblock();
    }
}

static void atomicvariable_activateListMode(AtomicVariableProxy * self)
{
    self->listMode = 1;
}

MethodInfoDesc atomicvariableMethods[] = {
    {"set", "", (code_t) atomicvariable_set}
    ,
    {"get", "", (code_t) atomicvariable_get}
    ,
    {"atomicUpdateUnblock", "", (code_t) atomicvariable_atomicUpdateUnblock}
    ,
    {"blockIfEqual", "", (code_t) atomicvariable_blockIfEqual}
    ,
    {"blockIfNotEqual", "", (code_t) atomicvariable_blockIfNotEqual}
    ,
    {"activateListMode", "", (code_t) atomicvariable_activateListMode}
    ,
};

static jbyte atomicvariableTypeMap[] = { 1 };

void init_atomicvariable_portal(void)
{
    atomicvariableClass =
        init_zero_class("jx/zero/AtomicVariable", atomicvariableMethods, sizeof(atomicvariableMethods), 1,
                atomicvariableTypeMap, "<jx/zero/AtomicVariable>");
}
