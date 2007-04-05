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
// zero_CAS.c
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
 * CAS
 */

ClassDesc *casClass = NULL;

static jboolean cas_casObject(CASProxy * self, ObjectDesc * obj, ObjectDesc * compare, ObjectDesc * setTo)
{
    jboolean ret = JNI_TRUE;
    u32 o = obj->data[self->index];

    if (o != (u32) compare) {
        ret = JNI_FALSE;
    } else {
        obj->data[self->index] = (u32) setTo;
    }

    return ret;
}

MethodInfoDesc casMethods[] = {
    {"casObject", "", (code_t) cas_casObject}
    ,
    {"casInt", "", (code_t) cas_casObject}
    ,
    {"casBoolean", "", (code_t) cas_casObject}
    ,
    {"casShort", "", (code_t) cas_casObject}
    ,
    {"casByte", "", (code_t) cas_casObject}
    ,
};


static jbyte casTypeMap[] = { 0 };

void init_cas_portal(void)
{
    casClass = init_zero_class("jx/zero/CAS", casMethods, sizeof(casMethods), 1, casTypeMap, "<jx/zero/CAS>");
}
