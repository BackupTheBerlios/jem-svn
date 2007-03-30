//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
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
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================
// gc.c
// 
// Jem/JVM base garbage collector implementation
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/timer.h>

#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "portal.h"
#include "thread.h"
#include "malloc.h"
#include "load.h"
#include "gc.h"
#include "gc_normal.h"
#include "gc_compacting.h"
#include "gc_move.h"
#include "exception_handler.h"


#ifndef CONFIG_JEM_ENABLE_GC

/** THIS FUNCTION MUST NOT BE INTERRUPTED BY A GC */
ObjectHandle registerObject(DomainDesc * domain, ObjectDesc * obj)
{
	return (ObjectHandle) obj;
}

/** THIS FUNCTION MUST NOT BE INTERRUPTED BY A GC */
ObjectDesc *unregisterObject(DomainDesc * domain, ObjectHandle handle)
{
	return (ObjectDesc *) handle;
}

u32 gc_mem(void)
{
	return 0;
}

void gc_init(DomainDesc * domain, u8 * memu, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4, int gcImpl)
{
}

void gc_done(DomainDesc * domain)
{
}

void gc_printInfo(DomainDesc * domain)
{
	printk(KERN_INFO "No gc.\n");
}


jboolean isRef(jbyte * map, int total, int num)
{
	return false;
}

void rswitches(void)
{
}

#else				/* ENABLE_GC */


/** THIS FUNCTION MUST NOT BE INTERRUPTED BY A GC */
ObjectHandle nonatomic_registerObject(DomainDesc * domain, ObjectDesc * o)
{
	int i;

	for (i = 0; i < getJVMConfig()->maxRegistered; i++) {
		if (domain->gc.registeredObjects[i] == NULL) {
			domain->gc.registeredObjects[i] = o;
			goto ok;
		}
	}

	printk(KERN_ERR "Maximal number of registered objects reached for this domain.\n");

  ok:
	return &(domain->gc.registeredObjects[i]);
}

/** THIS FUNCTION MUST NOT BE INTERRUPTED BY A GC */
ObjectDesc *nonatomic_unregisterObject(DomainDesc * domain, ObjectHandle o)
{
	ObjectDesc *obj;
	obj = *o;
	*o = NULL;
	return obj;
}


/*
 * Find objects given a class name
 *  TODO: use a GENERIC way to walk the heap (scan_heap,finalize_memory)
 */
static void gc_findOnHeapCB(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	ClassDesc *c = obj2ClassDesc(obj);
	if (strcmp(domain->gc.data, c->name) == 0) {
		u32 k = 2;
		u32 offs;
		k++;
		offs = k;
		for (; k < objSize - 1; k++) {	/* -1, bacause OBJSIZE_OBJECT contains unused size field */
			int l;
			char *fieldname = "???";
			char *fieldtype = "???";
			for (l = 0; l < c->numberFields; l++) {
				if (c->fields[l].fieldOffset == k - offs) {
					fieldname = c->fields[l].fieldName;
					fieldtype = c->fields[l].fieldType;
					break;
				}
			}
		}
	}
}

void gc_findOnHeap(DomainDesc * domain, char *classname)
{
	int l;

	ClassDesc *c = findClass(domain, classname)->classDesc;
	for (l = 0; l < c->numberFields; l++);
	domain->gc.data = classname;
	domain->gc.walkHeap(domain, gc_findOnHeapCB, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}


void gc_in(ObjectDesc * o, DomainDesc * domain)
{
    u64 gcStartTime, gcEndTime;
    u32 heapBytesBefore;

	if (domain->inhibitGCFlag)
		printk(KERN_ERR "GC currently inhibited in domain %d.\n", domain->id);

	printk(KERN_INFO "GC in  %d (%s) started\n", domain->id, domain->domainName);

	gcStartTime         = rt_timer_tsc();
	heapBytesBefore     = gc_freeWords(domain) * 4;
    if (rt_mutex_acquire(&domain->gc.gcLock, TM_INFINITE)) return;
    domain->gc.active   = JNI_TRUE;
    rt_mutex_release(&domain->gc.gcLock);
	domain->gc.gc(domain);
	gcEndTime = rt_timer_tsc();
	domain->gc.gcTime += rt_timer_tsc2ns(gcEndTime - gcStartTime) / 1000;
	domain->gc.gcRuns++;
	domain->gc.gcBytesCollected += gc_freeWords(domain) * 4 - heapBytesBefore;

	/* start a new GC epoch for this domain */
	domain->gc.epoch++;

	printk(KERN_INFO "GC in  %d finished\n", domain->id);
}

void gc_printInfo(DomainDesc * domain)
{
	printk(KERN_INFO "   GC Info for %s (%3d)\n", domain->domainName, domain->id);
	domain->gc.printInfo(domain);
	printk(KERN_INFO "      Runs: %3d, Time: %10d, Collected Bytes: %d\n", domain->gc.gcRuns, (u32) domain->gc.gcTime,
	       (u32) domain->gc.gcBytesCollected);
}

static void gc_zero_panic(DomainDesc * domain)
{
	printk(KERN_ERR "gc_zero_panic called");
}

u32 gc_mem(void)
{
	return getJVMConfig()->maxRegistered * sizeof(ObjectDesc *);
}

void gc_zero_init(DomainDesc * domain)
{
	domain->gc.allocDataInDomain = (ObjectHandle(*)(DomainDesc *, int, u32)) gc_zero_panic;
	domain->gc.gc = (void (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.done = (void (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.freeWords = (u32 (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.totalWords = (u32 (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.printInfo = (void (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.finalizeMemory = (void (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.finalizePortals = (void (*)(DomainDesc *)) gc_zero_panic;
	domain->gc.isInHeap = (jboolean(*)(DomainDesc *, ObjectDesc *)) gc_zero_panic;
	domain->gc.walkHeap = (void (*) (DomainDesc *, HandleObject_t, HandleObject_t, HandleObject_t, HandleObject_t,
                                     HandleObject_t, HandleObject_t, HandleObject_t, HandleObject_t, HandleObject_t,
                                     HandleObject_t, HandleObject_t)) gc_zero_panic;
}

void gc_init(DomainDesc * domain, u8 * mem, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4, int gcImpl)
{
	domain->gc.registeredObjects = (ObjectDesc **) mem;
	memset(domain->gc.registeredObjects, 0, getJVMConfig()->maxRegistered * sizeof(ObjectDesc *));
	mem += getJVMConfig()->maxRegistered * sizeof(ObjectDesc *);

    domain->gc.active       = JNI_FALSE;
    domain->gc.gcSuspended  = NULL;

	gc_zero_init(domain);

	switch (gcImpl) {
	case GC_IMPLEMENTATION_NEW:
		gc_normal_init(domain, gcinfo0);
		break;
#ifdef GC_COMPACTING_IMPL
	case GC_IMPLEMENTATION_COMPACTING:
		gc_compacting_init(domain, gcinfo0);
		break;
#endif
#ifdef GC_BITMAP_IMPL
	case GC_IMPLEMENTATION_BITMAP:
		gc_bitmap_init(domain, gcinfo0);
		break;
#endif
	default:
		printk(KERN_ERR "Unknown gc implementation GCImpl=%d\n", gcImpl);
		exceptionHandlerMsg(THROW_RuntimeException, "unknown gc implementation");
	}
}

void gc_done(DomainDesc * domain)
{
	/* finalize memory objects */
	domain->gc.finalizeMemory(domain);
	domain->gc.finalizePortals(domain);
	domain->gc.done(domain);
}
#endif				/* ENABLE_GC */


