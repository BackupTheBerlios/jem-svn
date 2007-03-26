//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
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
// File: malloc.c
//
// Jem/JVM memory allocation implementation.
//
//===========================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/queue.h>
#include <native/heap.h>
#include "jemtypes.h"
#include "malloc.h"
#include "messages.h"
#include "jemUPipe.h"
#include "domain.h"

#define MEMTYPE_PARAM , memtype


static RT_HEAP      jemHeap;
unsigned int        jemHeapSz=0;
static unsigned int allocRam = 0;

struct alloc_stat_s {
	u32 heap;
	u32 code;
	u32 stack;
	u32 memobj;
	u32 profiling;
	u32 emulation;
	u32 tmp;
	u32 dcb;
	u32 other;
};
static struct alloc_stat_s alloc_stat;


int jemMallocInit(void)
{
    int     result;

    if ((result = rt_heap_create(&jemHeap, "jemHeap", jemHeapSz, H_PRIO|H_MAPPABLE)) < 0) {
        printk(KERN_ERR "Can not create Jem/JVM heap of size %d, code=%d.\n", jemHeapSz, result);
        return result;
    }

    return 0;
}


int jemDestroyHeap(void)
{
    return rt_heap_delete(&jemHeap);
}


static void *jemMallocInternal(u32 size, u32 ** start MEMTYPE_INFO)
{
	void    *addr = NULL;

    if (rt_heap_alloc(&jemHeap, size, TM_NONBLOCK, &addr)) {
        return NULL;
    }

    allocRam += size;

    switch (memtype) {
    case MEMTYPE_HEAP_PARAM:
        alloc_stat.heap += size;
        break;
    case MEMTYPE_CODE_PARAM:
        alloc_stat.code += size;
        break;
    case MEMTYPE_MEMOBJ_PARAM:
        alloc_stat.memobj += size;
        break;
    case MEMTYPE_PROFILING_PARAM:
        alloc_stat.profiling += size;
        break;
    case MEMTYPE_STACK_PARAM:
        alloc_stat.stack += size;
        break;
    case MEMTYPE_EMULATION_PARAM:
        alloc_stat.emulation += size;
        break;
    case MEMTYPE_TMP_PARAM:
        alloc_stat.tmp += size;
        break;
    case MEMTYPE_DCB_PARAM:
        alloc_stat.dcb += size;
        break;
    case MEMTYPE_OTHER_PARAM:
        alloc_stat.other += size;
        break;
    default:
        printk(KERN_WARNING "Allocation MEMTYPE unknown.\n");
        alloc_stat.other += size;
    }

    memset(addr, 0, size);

	*start = addr;
	return addr;
}


void *jemMalloc(u32 size MEMTYPE_INFO)
{
    u32 *start;
    return jemMallocInternal(size, &start MEMTYPE_PARAM);
}


void jemFree(void *addr, u32 size MEMTYPE_INFO)
{

	switch (memtype) {
	case MEMTYPE_HEAP_PARAM:
		alloc_stat.heap -= size;
		break;
	case MEMTYPE_CODE_PARAM:
		alloc_stat.code -= size;
		break;
	case MEMTYPE_MEMOBJ_PARAM:
		alloc_stat.memobj -= size;
		break;
	case MEMTYPE_PROFILING_PARAM:
		alloc_stat.profiling -= size;
		break;
	case MEMTYPE_STACK_PARAM:
		alloc_stat.stack -= size;
		break;
	case MEMTYPE_EMULATION_PARAM:
		alloc_stat.emulation -= size;
		break;
	case MEMTYPE_TMP_PARAM:
		alloc_stat.tmp -= size;
		break;
	case MEMTYPE_DCB_PARAM:
		alloc_stat.dcb -= size;
		break;
	case MEMTYPE_OTHER_PARAM:
		alloc_stat.other -= size;
		break;
	default:
		printk(KERN_WARNING "Free MEMTYPE unknown\n");
	}

    allocRam -= size;
    rt_heap_free(&jemHeap, addr);
}


char *jemMallocCode(DomainDesc *domain, u32 size)
{
	char    *data;
	char    *nextObj;
	u32     c;
	u32     chunksize = domain->code_bytes;

    int result = rt_mutex_acquire(&domain->domainMemLock, TM_INFINITE);
    if (result < 0) {
        printk(KERN_ERR "Error acquiring domain memory lock, code=%d\n", result);
        return NULL;
    }

	if (domain->cur_code == -1) {
		domain->code[0] = (char *) jemMalloc(chunksize MEMTYPE_CODE);
		domain->codeBorder[0] = domain->code[0] + chunksize;
		domain->codeTop[0] = domain->code[0];
		domain->cur_code = 0;
	}

	c       = domain->cur_code;
	nextObj = domain->codeTop[c] + size;

	if (nextObj > domain->codeBorder[c]) {
		c++;
		if (c == CONFIG_JEM_CODE_FRAGMENTS) {
            printk(KERN_ERR "Out of code space for domain.\n");
            rt_mutex_release(&domain->domainMemLock);
            return NULL;
        }
		domain->code[c]         = (char *) jemMalloc(chunksize MEMTYPE_CODE);
		domain->codeBorder[c]   = domain->code[c] + chunksize;
		domain->codeTop[c]      = domain->code[c];
		domain->cur_code        = c;
		nextObj                 = domain->codeTop[c] + size;
		if (nextObj > domain->codeBorder[c]) {
            printk(KERN_ERR "Can`t allocate %i byte for code space\n", size);
            rt_mutex_release(&domain->domainMemLock);
            return NULL;
        }
	}

	data                = domain->codeTop[c];
	domain->codeTop[c]  = nextObj;

    rt_mutex_release(&domain->domainMemLock);

	return data;
}


