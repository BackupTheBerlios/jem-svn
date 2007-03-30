//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 2001-2002 Joerg Baumann. All rights reserved.
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
// gc_normal.c
// 
// Jem/JVM Copying garbage collector implementation
// 
//==============================================================================

#ifdef CONFIG_JEM_ENABLE_GC 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "jemtypes.h"
#include "malloc.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "portal.h"
#include "thread.h"
#include "code.h"
#include "load.h"
#include "gc_normal.h"
#include "gc_impl.h"
#include "gc_move_common.h"
#include "gc_move.h"
#include "exception_handler.h"
#include "zero_Memory.h"


typedef struct gc_normal_mem_s {
	gc_move_common_mem_t    move_common;
	jint                    *heapBorder;	/* pointer to border of heap (last allocated word  + 1) */
	jint                    *heap;		    /* all objects life here */
	jint                    heapSize;		/* heap size */
	jint                    *heapTop;		/* pointer to free heap space */
	jint                    *heapBorder2;	/* pointer to border of heap (last allocated word  + 1) */
	jint                    *heap2;		    /* all objects life here */
	jint                    *heapTop2;		/* pointer to free heap space */
	jint                    *mark;
} gc_normal_mem_t;

#define GCM_NEW(domain) (*(gc_normal_mem_t*)(&domain->gc.untypedMemory))

ObjectHandle(*registerObject) (DomainDesc * domain, ObjectDesc * o);
void gc_normal_checkHeap(DomainDesc * domain);
void profile_sample_heapusage_alloc(DomainDesc * domain, u32 objSize);

ObjectHandle gc_normal_allocDataInDomain(DomainDesc * domain, int objSize, u32 flags)
{
	ObjectDesc      *obj;
	jint            *nextObj;
	jint            *data;
	ObjectHandle    handle;
	jboolean        tried = JNI_FALSE;

    int result = rt_mutex_acquire(&domain->domainHeapLock, TM_INFINITE);
    if (result < 0) {
        printk(KERN_ERR "Error acquiring domain heap allocation lock, code=%d\n", result);
        return NULL;
    }

  try_alloc:
	nextObj = GCM_NEW(domain).heapTop + objSize;
	if ((nextObj > GCM_NEW(domain).heapBorder - getJVMConfig()->heapReserve)) {
		if (curthr()->isInterruptHandlerThread) {
			if (nextObj > GCM_NEW(domain).heapBorder) {
				printk(KERN_ERR "Out of heap space in interrupt handler.\n");
                rt_mutex_release(&domain->domainHeapLock);
                return NULL;
			}
#ifdef CONFIG_JEM_ENABLE_GC
			goto do_alloc;	/* we are in the interrupt handler but have still enough heap reserve */
#else
			printk(KERN_WARNING "Attempt to GC with GC disabled at %s:%d\n" __FILE__, __LINE__);
#endif
		}
		/* not in interrupt handler -> GC possible */

#ifndef CONFIG_JEM_ENABLE_GC
        printk(KERN_WARNING "Attempt to GC with GC disabled at %s:%d\n" __FILE__, __LINE__);
#endif

		printk(KERN_INFO "Domain %p (%s) consumed %d bytes of heap space. Starting GC...\n", domain, domain->domainName,
		       (char *) GCM_NEW(domain).heapTop - (char *) GCM_NEW(domain).heap);
		if (tried) {
			printk(KERN_WARNING "GC did not free enough memory. need %d bytes\n", objSize << 2);
			exceptionHandler(THROW_RuntimeException);
		}
		start_thread_using_code1(domain->gc.gcObject, domain->gc.gcThread, domain->gc.gcCode, (u32) domain);

		tried = JNI_TRUE;
		goto try_alloc;
	}

  do_alloc:
	data = (jint *) GCM_NEW(domain).heapTop;
	GCM_NEW(domain).heapTop = nextObj;
	memset(data, 0, objSize * 4);
	obj = ptr2ObjectDesc((u32 *) data);
	setObjFlags(obj, flags);
	setObjMagic(obj, MAGIC_OBJECT);
	handle = registerObject(domain, obj);

    rt_mutex_release(&domain->domainHeapLock);
	return handle;
}

void gc_normal_walkHeap(DomainDesc * domain, HandleObject_t handleObject, HandleObject_t handleArray, HandleObject_t handlePortal,
		     HandleObject_t handleMemory, HandleObject_t handleService, HandleObject_t handleCAS,
		     HandleObject_t handleAtomVar, HandleObject_t handleDomainProxy, HandleObject_t handleCPUStateProxy,
		     HandleObject_t handleServicePool, HandleObject_t handleStackProxy)
{
	gc_walkContinuesBlock(domain, (u32 *) GCM_NEW(domain).heap, (u32 **) & GCM_NEW(domain).heapTop, handleObject, handleArray,
			      handlePortal, handleMemory, handleService, handleCAS, handleAtomVar, handleDomainProxy,
			      handleCPUStateProxy, handleServicePool, handleStackProxy);
}

static void gc_normal_walkHeap2(DomainDesc * domain, HandleObject_t handleObject, HandleObject_t handleArray,
			     HandleObject_t handlePortal, HandleObject_t handleMemory, HandleObject_t handleService,
			     HandleObject_t handleCAS, HandleObject_t handleAtomVar, HandleObject_t handleDomainProxy,
			     HandleObject_t handleCPUStateProxy, HandleObject_t handleServicePool,
			     HandleObject_t handleStackProxy)
{
	gc_walkContinuesBlock(domain, (u32 *) GCM_NEW(domain).heap2, (u32 **) & GCM_NEW(domain).heapTop2, handleObject, handleArray,
			      handlePortal, handleMemory, handleService, handleCAS, handleAtomVar, handleDomainProxy,
			      handleCPUStateProxy, handleServicePool, handleStackProxy);
}

inline int gc_normal_isInHeap(DomainDesc * domain, ObjectDesc * obj)
{
	return (obj >= (ObjectDesc *) GCM_NEW(domain).heap && obj < (ObjectDesc *) GCM_NEW(domain).heapTop);
}

static inline int gc_normal_ensureInHeap(DomainDesc * domain, ObjectDesc * obj)
{
	if (!gc_normal_isInHeap(domain, obj)) {
		return 0;
	}
	return 1;
}

static inline u32 *gc_normal_allocHeap2(DomainDesc * domain, u32 size)
{
	u32 *data;

	data = (u32 *) GCM_NEW(domain).heapTop2;
	if (data > (u32 *) GCM_NEW(domain).heapBorder2) {
		printk(KERN_ERR "Target heap too small.\n");
	}
	GCM_NEW(domain).heapTop2 += size;

	return data;
}


static void gc_normal_scan_heap2_Object(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentObject(domain, obj, gc_common_move_reference);
}

static void gc_normal_scan_heap2_Array(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentArray(domain, (ArrayDesc *) obj, gc_common_move_reference);
}

static void gc_normal_scan_heap2_Service(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentService(domain, (DEPDesc *) obj, gc_common_move_reference);
}

static void gc_normal_scan_heap2_AtomVar(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentAtomVar(domain, (AtomicVariableProxy *) obj, gc_common_move_reference);
}

static void gc_normal_scan_heap2_CPUState(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentCPUState(domain, (ThreadDescProxy *) obj, gc_common_move_reference);
}

static void gc_normal_scan_heap2_ServicePool(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentServicePool(domain, (ServiceThreadPool *) obj, gc_common_move_reference);
}

void gc_normal_done(DomainDesc * domain)
{
	jemFree(GCM_NEW(domain).heap, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
	GCM_NEW(domain).heap = GCM_NEW(domain).heapBorder = GCM_NEW(domain).heapTop = NULL;

	if (GCM_NEW(domain).heap2) {
		jemFree(GCM_NEW(domain).heap2, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
		GCM_NEW(domain).heap2 = GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heapTop2 = NULL;
	}
}


static void gc_normal_finalizeMemoryCB(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	if ((flags & FORWARD_MASK) != GC_FORWARD) {
		memory_deleted((struct MemoryProxy_s *) obj);
	}
}

static void gc_normal_finalizePortalsCB(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	Proxy *p = (Proxy *) obj;
	if (!((flags & FORWARD_MASK) == GC_FORWARD)) {
		// decrement service refcount
		if (p->targetDomain && (p->targetDomain->id == p->targetDomainID)) {
			if (p->targetDomain != domain) {	/* otherwise dummy portal in own domain */
				service_decRefcount(p->targetDomain, p->index);
			}
		}
	}
}

/*
 * Finalize all unreachable memory objects
 */
void gc_normal_finalizeMemory(DomainDesc * domain)
{
	gc_normal_walkHeap(domain, NULL, NULL, NULL, gc_normal_finalizeMemoryCB, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}


/*
 * Finalize all garbage portals
 */
void gc_normal_finalizePortals(DomainDesc * domain)
{
	gc_normal_walkHeap(domain, NULL, NULL, gc_normal_finalizePortalsCB, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}



void gc_normal_checkHeap(DomainDesc * domain)
{
	gc_normal_walkHeap(domain, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}


void gc_normal_gc(DomainDesc * domain)
{
    jint *htmp;

	printk(KERN_INFO "GC started for domain %p (%s)\n", domain, domain->domainName);

#ifdef CONFIG_JEM_DYNAMIC_HEAP
	/* alloc second heap */
	if (GCM_NEW(domain).heap2 == NULL) {
		GCM_NEW(domain).heap2 = (jint *) jemMalloc(GCM_NEW(domain).heapSize MEMTYPE_HEAP);
		GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heap2 + (GCM_NEW(domain).heapSize >> 2);
		GCM_NEW(domain).heapTop2 = GCM_NEW(domain).heap2;
	}
#endif		

	/* 
	 * Init
	 */
	//freezeThreads(domain);

	/*
	 * Move directly reachable objects onto new heap 
	 */
	walkRootSet(domain, gc_common_move_reference, gc_common_move_reference, gc_common_move_reference,
		    gc_common_move_reference, gc_common_move_reference, gc_common_move_reference);

	/*
	 * All directly reachable objects are now on the new heap
	 * Scan new heap 
	 */
	gc_normal_walkHeap2(domain, gc_normal_scan_heap2_Object, gc_normal_scan_heap2_Array, NULL, NULL, gc_normal_scan_heap2_Service, NULL,
			 gc_normal_scan_heap2_AtomVar, NULL, gc_normal_scan_heap2_CPUState, gc_normal_scan_heap2_ServicePool,
			 gc_normal_scan_heap2_Stack);


	/*
	 * Finish
	 */
	gc_normal_finalizeMemory(domain);
	gc_normal_finalizePortals(domain);

    htmp                        = (jint *) GCM_NEW(domain).heap;
	GCM_NEW(domain).heap        = GCM_NEW(domain).heap2;
	GCM_NEW(domain).heap2       = htmp;

	htmp                        = GCM_NEW(domain).heapBorder;
	GCM_NEW(domain).heapBorder  = GCM_NEW(domain).heapBorder2;
	GCM_NEW(domain).heapBorder2 = htmp;

	GCM_NEW(domain).heapTop     = GCM_NEW(domain).heapTop2;
	GCM_NEW(domain).heapTop2    = GCM_NEW(domain).heap2;
#ifdef CONFIG_JEM_DYNAMIC_HEAP
    jemFree(GCM_NEW(domain).heap2, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
    GCM_NEW(domain).heap2       = NULL;
#endif

	printk(KERN_INFO "GC finished\n");
}

u32 gc_normal_freeWords(DomainDesc * domain)
{
	return (u32 *) GCM_NEW(domain).heapBorder - (u32 *) GCM_NEW(domain).heapTop;
}

u32 gc_normal_totalWords(struct DomainDesc_s * domain)
{
	return (u32 *) GCM_NEW(domain).heapBorder - (u32 *) GCM_NEW(domain).heap;
}

void gc_normal_printInfo(struct DomainDesc_s *domain)
{
	printk(KERN_INFO "     heap(used ): %p...%p (current=%p)\n", GCM_NEW(domain).heap, GCM_NEW(domain).heapBorder,
	       GCM_NEW(domain).heapTop);
	printk(KERN_INFO "     heap(other): %p...%p (current=%p)\n", GCM_NEW(domain).heap2, GCM_NEW(domain).heapBorder2,
	       GCM_NEW(domain).heapTop2);
	printk(KERN_INFO "     total: %d, used: %d, free: %d\n", gc_totalWords(domain) * 4,
	       (gc_totalWords(domain) - gc_freeWords(domain)) * 4, (gc_freeWords(domain)) * 4);
}

void gc_normal_setMark(struct DomainDesc_s *domain)
{
	GCM_NEW(domain).mark = GCM_NEW(domain).heapTop;
}


ObjectDesc *gc_normal_atMark(struct DomainDesc_s *domain)
{
	u32         *data = (u32 *) GCM_NEW(domain).mark;
	ObjectDesc  *obj;
	jint        flags;
	u32         objSize = 0;
	ClassDesc   *c;

	if (data == NULL || data >= (u32 *) GCM_NEW(domain).heapTop) {
		GCM_NEW(domain).mark = NULL;
		return NULL;
	}
 
	obj = ptr2ObjectDesc(data);
	flags = getObjFlags(obj);
	switch (flags & FLAGS_MASK) {
	case OBJFLAGS_ARRAY:
		objSize = gc_objSize2(obj, flags);
		break;
	case OBJFLAGS_OBJECT:
		c = obj2ClassDesc(obj);
		objSize = OBJSIZE_OBJECT(c->instanceSize);
		break;
	case OBJFLAGS_PORTAL:
		objSize = OBJSIZE_PORTAL;
		break;
	case OBJFLAGS_MEMORY:
		objSize = OBJSIZE_MEMORY;
		break;
	case OBJFLAGS_SERVICE:
		objSize = OBJSIZE_SERVICEDESC;
		break;
	case OBJFLAGS_SERVICE_POOL:
		objSize = OBJSIZE_SERVICEPOOL;
		break;
	case OBJFLAGS_ATOMVAR:
		objSize = OBJSIZE_ATOMVAR;
		break;
	case OBJFLAGS_CAS:
		objSize = OBJSIZE_CAS;
		break;
	case OBJFLAGS_DOMAIN:
		objSize = OBJSIZE_DOMAIN;
		break;
	case OBJFLAGS_CPUSTATE:
		objSize = OBJSIZE_THREADDESCPROXY;
		break;
	case OBJFLAGS_FOREIGN_CPUSTATE:
		objSize = OBJSIZE_FOREIGN_THREADDESC;
		break;
	default:
		printk(KERN_ERR "OBJ=%p, FLAGS: %lx mark=%p, heap=%p heaptop=%p\n", obj, flags & FLAGS_MASK, GCM_NEW(domain).mark,
		       GCM_NEW(domain).heap, GCM_NEW(domain).heapTop);
		printk(KERN_ERR "Wrong heap data.\n");
	}
	GCM_NEW(domain).mark += objSize;
	return obj;
}

void gc_normal_init(DomainDesc * domain, u32 heap_bytes)
{
	if (heap_bytes == 0) {
        return;
    }

	if (heap_bytes < getJVMConfig()->heapReserve) {
        printk(KERN_INFO "Heap too small, need at least %d bytes ", getJVMConfig()->heapReserve);
        heap_bytes = getJVMConfig()->heapReserve;
    }

	/* alloc heap mem */
	GCM_NEW(domain).heap        = (jint *) jemMalloc(heap_bytes MEMTYPE_HEAP);
	GCM_NEW(domain).heapSize    = heap_bytes;
	GCM_NEW(domain).heapBorder  = GCM_NEW(domain).heap + (heap_bytes >> 2);
	GCM_NEW(domain).heapTop     = GCM_NEW(domain).heap;

#ifndef CONFIG_JEM_DYNAMIC_HEAP
	GCM_NEW(domain).heap2       = (jint *) jxmalloc(heap_bytes MEMTYPE_HEAP);
	GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heap2 + (heap_bytes >> 2);
	GCM_NEW(domain).heapTop2    = GCM_NEW(domain).heap2;
#endif	

	GCM_MOVE_COMMON(domain).allocHeap2  = gc_normal_allocHeap2;
	GCM_MOVE_COMMON(domain).walkHeap2   = gc_normal_walkHeap2;
	domain->gc.allocDataInDomain        = gc_normal_allocDataInDomain;
	domain->gc.gc                       = gc_normal_gc;
	domain->gc.done                     = gc_normal_done;
	domain->gc.freeWords                = gc_normal_freeWords;
	domain->gc.totalWords               = gc_normal_totalWords;
	domain->gc.printInfo                = gc_normal_printInfo;
	domain->gc.finalizeMemory           = gc_normal_finalizeMemory;
	domain->gc.finalizePortals          = gc_normal_finalizePortals;
	domain->gc.isInHeap                 = gc_normal_isInHeap;
	domain->gc.ensureInHeap             = gc_normal_ensureInHeap;
	domain->gc.walkHeap                 = gc_normal_walkHeap;
	domain->gc.setMark                  = gc_normal_setMark;
	domain->gc.atMark                   = gc_normal_atMark;
}
#endif			




