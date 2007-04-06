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
// zero_Domain.c
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

extern ClassDesc *domainClass;

/*
 * Domain DEP
 */

static void domain_clearTCBflag(ObjectDesc * self)
{
}

static jboolean domain_isActive(DomainProxy * self)
{
    jboolean ret = JNI_FALSE;
    if ((self->domain->id == self->domainID)
        && (self->domain->state == DOMAIN_STATE_ACTIVE))
        ret = JNI_TRUE;
    return ret;
}

static jboolean domain_isTerminated(DomainProxy * self)
{
    jboolean ret = JNI_FALSE;
    if (self->domain->id != self->domainID)
        ret = JNI_TRUE;
    else if (self->domain->state == DOMAIN_STATE_FREE)
        ret = JNI_TRUE;
    return ret;
}

static ObjectDesc *domain_getName(DomainProxy * self)
{
    return NULL;
}

static ObjectDesc *domain_getID(DomainProxy * self)
{
    return (ObjectDesc *) self->domainID;
}

MethodInfoDesc domainMethods[] = {
    {"clearTCBflag", "()I", (code_t) domain_clearTCBflag}
    ,
    {"isActive", "()Z", (code_t) domain_isActive}
    ,
    {"isTerminated", "()Z", (code_t) domain_isTerminated}
    ,
    {"getName", "()Ljava/lang/String;", (code_t) domain_getName}
    ,
    {"getID", "()I", (code_t) domain_getID}
    ,
};

static jbyte domainTypeMap[] = { 0 };

void init_domain_portal(void)
{
    domainClass =
        init_zero_class("jx/zero/Domain", domainMethods, sizeof(domainMethods), 1, domainTypeMap, "<jx/zero/Domain>");
}
