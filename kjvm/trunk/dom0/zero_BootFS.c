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
// zero_BootFS.c
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
#include "memfs.h"


#define CALLERDOMAIN (curthr()->mostRecentlyCalledBy->domain)

/*
 * BootFS DEP
 */

static jint bootfs_lookup(ObjectDesc * self, ObjectDesc * filename)
{
    return memfs_lookup(memfs_str2chr(filename));
}

ObjectDesc *bootfs_getFile(ObjectDesc * self, ObjectDesc * filename)
{
    FileDesc *fd;
    MemoryProxyHandle mem;

    if ((fd = memfs_open(curdom(), (char *) memfs_str2chr(filename))) == NULL) {
        return NULL;
    }


    mem = memfs_mmap(fd, curdom(), MEMFS_RO);

    memfs_close(fd);

    return (ObjectDesc *) mem; 
}

static ObjectDesc *bootfs_getReadWriteFile(ObjectDesc * self, ObjectDesc * filename)
{
    FileDesc *fd;
    ObjectDesc *mem;

    if ((fd = memfs_open(curdom(), (char *) memfs_str2chr(filename))) == NULL) {
        exceptionHandler(THROW_NullPointerException);
    }

    mem = (ObjectDesc *) memfs_mmap(fd, curdom(), MEMFS_RW);

    memfs_close(fd);

    return mem;
}

MethodInfoDesc bootfsMethods[] = {
    {"lookup", "", (code_t) bootfs_lookup}
    ,
    {"getFile", "", (code_t) bootfs_getFile}
    ,
    {"getReadWriteFile", "", (code_t) bootfs_getReadWriteFile}
    ,
};


void init_bootfs_portal(void)
{
    init_zero_dep("jx/zero/BootFS", "BootFS", bootfsMethods, sizeof(bootfsMethods), "<jx/zero/BootFS>");
}
