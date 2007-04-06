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
// zero_ComponentManager.c
// 
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
#include <native/timer.h>
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

#define CALLERDOMAIN (curthr()->mostRecentlyCalledBy->domain)

static jint componentManager_load(ObjectDesc * self, ObjectDesc * libnameObj)
{
    char value[128];
    LibDesc *lib;
    DomainDesc *sourceDomain = CALLERDOMAIN;
    if (libnameObj == 0)
        return 0;
    stringToChar(libnameObj, value, sizeof(value));
    lib = load(sourceDomain, value);
    return (jint) lib;  // HACK
}

static void componentManager_registerLib(ObjectDesc * self, ObjectDesc * libnameObj, ObjectDesc * memObj)
{
}

static void componentManager_setInheritThread(ObjectDesc * self, ObjectDesc * classnameObj)
{
    char value[128];
    ClassDesc *cl;
    if (classnameObj == 0)
        return;
    stringToChar(classnameObj, value, sizeof(value));
    cl = findClassDesc(value);
    cl->inheritServiceThread = JNI_TRUE;
}

static MethodInfoDesc componentManagerMethods[] = {
    {"load", "", (code_t) componentManager_load},
    {"registerLib", "", (code_t) componentManager_registerLib},
    {"setInheritThread", "", (code_t) componentManager_setInheritThread},
};

void init_componentmanager_portal(void)
{
    init_zero_dep("jx/zero/ComponentManager", "ComponentManager", componentManagerMethods, sizeof(componentManagerMethods),
              "<jx/zero/ComponentManager>");
}
