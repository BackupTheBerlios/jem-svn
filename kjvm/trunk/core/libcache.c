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
// libcache.c
// 
//==============================================================================
// 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "libcache.h"
#include "zero_Memory.h"
#include "load.h"

static libcache_entry *global_jll_cache = NULL;

libcache_entry *libcache_new_entry(ObjectDesc * string_obj, ObjectDesc * memory_obj)
{
    libcache_entry *new_entry;

    if ((new_entry = jemMalloc(sizeof(libcache_entry) MEMTYPE_OTHER)) == NULL) {
        return NULL;
    }

    new_entry->name = string_obj;
    new_entry->codefile = memory_obj;
    new_entry->next = NULL;

    return new_entry;
}

void libcache_init(void)
{
    global_jll_cache = NULL;
}

char *libcache_lookup_jll(const char *name, jint * size)
{
    libcache_entry *entry;
    ObjectDesc *str;
    char e_str[80];

    for (entry = global_jll_cache; entry != NULL; entry = entry->next) {

        str = entry->name;
        stringToChar(str, e_str, 80);

        if (strcmp(name, e_str) == 0) {

            if (size != NULL)
                *size = memory_size(entry->codefile);

            return (char *) memory_getStartAddress(entry->codefile);
        }

    }

    if (size != NULL)
        *size = 0;
    return NULL;
}

void libcache_register_jll(ObjectDesc * self, ObjectDesc * string_obj, ObjectDesc * memory_obj)
{
    libcache_entry *newjll;
    char e_str[80];

    stringToChar(string_obj, e_str, 80);

    printk(KERN_INFO "Lib register: %s\n", e_str);

    if ((newjll = libcache_new_entry(string_obj, memory_obj)) == NULL) {
        printk(KERN_ERR "No memory for libcache\n");
        return;
    }

    newjll->next = global_jll_cache;
    global_jll_cache = newjll;

    return;
}

