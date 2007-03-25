//==============================================================================
// 
//==============================================================================

#ifdef CONFIG_JEM_ENABLE_GC 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
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
#include "zero.h"
#include "gc_move.h"
#include "gc_pgc.h"
#include "gc_thread.h"
#include "gc_stack.h"
#include "gc_impl.h"

extern RT_MUTEX     svTableLock;


/*
 * freeze the threads in domain at known checkpoints, so we know the stackmaps 
 */
void freezeThreads(DomainDesc * domain)
{
	ThreadDesc *thread;

	for (thread = domain->threads; thread != NULL; thread = thread->nextInDomain) {
		if (thread == domain->gc.gcThread)
			continue;	/* don't scan my own stack */
		if (thread->isInterruptHandlerThread)
			continue;	/* don't scan interrupt stacks, they can not be interrupted by a GC and so they are not active */
		if (thread->state == STATE_AVAILABLE)
			continue;	/* don't scan stack of available threads */
		check_thread_position(domain, thread);
	}
}


/* 
 * visit all static refernces in class cl 
 */
static void walkClass(DomainDesc * domain, JClass * cl, HandleReference_t handler)
{
	ClassDesc *c = cl->classDesc;

	FORBITMAP(c->staticsMap, c->staticFieldsSize,	/* reference slot */
		  handler(domain, (ObjectDesc **) & (cl->staticFields[index])), {
		  });
}

/* visit all references on the stacks for this domain*/
void walkStacks(DomainDesc * domain, HandleReference_t handler)
{
	ThreadDesc *thread;

	PGCB(STACK);
	for (thread = domain->threads; thread != NULL; thread = thread->nextInDomain) {
		if (thread == domain->gc.gcThread)
			continue;	/* don't scan my own stack */
		if (thread->isInterruptHandlerThread)
			continue;	/* don't scan interrupt stacks, they can not be interrupted by a GC and so they are not active */
		if (thread->state == STATE_AVAILABLE)
			continue;	/* don't scan stack of available threads */
		walkStack(domain, thread, handler);
	}
	PGCE(STACK);
}

/*
 * visit all static references for this domain
 */
void walkStatics(DomainDesc * domain, HandleReference_t handler)
{
	u32  i, k;
	LibDesc *lib;

	PGCB(STATIC);
	for (k = 0; k < domain->numberOfLibs; k++) {
		lib = domain->libs[k];
		for (i = 0; i < lib->numberOfClasses; i++) {
			walkClass(domain, &(lib->allClasses[i]), handler);
		}
	}
	PGCE(STATIC);
}

/* 
 * visit all portals for this domain 
 */
void walkPortals(DomainDesc * domain, HandleReference_t handler)
{
	u32     i;
	DEPDesc *d;
    int     result;

	PGCB(SERVICE);
	/* TODO: perform GC on copy of service table and use locking only to reinstall table 
	 * all entries of original table must be marked as changing
	 */
    result = rt_mutex_acquire(&svTableLock, TM_INFINITE);
    if (result < 0) {
        printk(KERN_ERR "Error acquiring service table lock during GC, code=%d\n", result);
        return;
    }

	for (i = 0; i < CONFIG_JEM_MAX_SERVICES; i++) {
		d = domain->services[i];
		if (d == SERVICE_ENTRY_FREE || d == SERVICE_ENTRY_CHANGING)
			continue;
		handler(domain, (ObjectDesc **) & (domain->services[i]));
	}
	for (i = 0; i < CONFIG_JEM_MAX_SERVICES; i++) {
		if (domain->pools[i]) {
			handler(domain, (ObjectDesc **) & (domain->pools[i]));
		}
	}
    rt_mutex_release(&svTableLock);
	PGCE(SERVICE);
}

/*
 * visit all registered objects (= in use by C core) for this domain 
 */
void walkRegistereds(DomainDesc * domain, HandleReference_t handler)
{
	u32  i;

	PGCB(REGISTERED);
	for (i = 0; i < getJVMConfig()->maxRegistered; i++) {
		if (domain->gc.registeredObjects[i] != NULL) {
			handler(domain, &(domain->gc.registeredObjects[i]));
		}
	}

	PGCE(REGISTERED);
}

/*
 * visit all special objects in this domain for this domain 
 */
void walkSpecials(DomainDesc * domain, HandleReference_t handler)
{
	ThreadDesc *t;

	PGCB(SPECIAL)
	if (domain->startClassName)
		handler(domain, (ObjectDesc **) & (domain->startClassName));
	if (domain->dcodeName != NULL)
		handler(domain, (ObjectDesc **) & (domain->dcodeName));
	if (domain->libNames != NULL)
		handler(domain, (ObjectDesc **) & (domain->libNames));
	if (domain->argv != NULL)
		handler(domain, (ObjectDesc **) & (domain->argv));
	if (domain->initialPortals != NULL)
		handler(domain, (ObjectDesc **) & (domain->initialPortals));
	if (domain->initialNamingProxy != NULL)
		handler(domain, (ObjectDesc **) & (domain->initialNamingProxy));

	/*
	 *  object references in domain control block 
	 */
	if (domain->naming) {
		handler(domain, (ObjectDesc **) & (domain->naming));
	}
	if (domain->outboundInterceptorObject) {
		handler(domain, (ObjectDesc **) & (domain->outboundInterceptorObject));
	}
	if (domain->inboundInterceptorObject) {
		handler(domain, (ObjectDesc **) & (domain->inboundInterceptorObject));
	}

	/*
	 *  TCBs in domain control block 
	 * Copying the first TCB is sufficient as all other TCBs are reachable from this one
	 */
	{
		extern ThreadDesc *switchBackTo;
		ThreadDesc *tp;
		ThreadDescProxy *tpr;

		MOVETCB(domain->threads);
		Sched_gc_rootSet(domain, handler);

		MOVETCB(domain->gc.gcThread);
		if (switchBackTo && switchBackTo->domain == domain) {	/* switchback is domainzero thread when gc is fired off by domainmanager.gc() */
			MOVETCB(switchBackTo);	// thread that was interrupted by GC (oneshot) */
		}

		/* move current thread TCB */
		tpr = thread2CPUState(curthr());
		handler(domain, (ObjectDesc **) & (tpr));
		*(curthrP()) = cpuState2thread(tpr);


	}

	/* TCBs now are objects on the heap and are scanned for references the normal way */


	PGCE(SPECIAL);
}

/*
 * visit all references to interrupt handler objects in this domain
 */
void walkInterrupHandlers(DomainDesc * domain, HandleReference_t handler)
{
	u32  i, j;

	/* Interrupt handler objects */
	PGCB(INTR);
	for (i = 0; i < MAX_NR_CPUS; i++) {	/* FIXME: must synchronize with these CPUs!! */
		for (j = 0; j < NUM_IRQs; j++) {
			if (ifirstlevel_object[i][j] != NULL && idomains[i][j] == domain) {
				handler(domain, &(ifirstlevel_object[i][j]));
			}
		}
	}
	PGCE(INTR);
}

/*
 * visit all non-heap references for this domain 
 */
void walkRootSet(DomainDesc * domain, HandleReference_t stackHandler, HandleReference_t staticHandler,
		 HandleReference_t portalHandler, HandleReference_t registeredHandler, HandleReference_t specialHandler,
		 HandleReference_t interruptHandlerHandler)
{
	if (stackHandler)
		walkStacks(domain, stackHandler);
	if (staticHandler)
		walkStatics(domain, staticHandler);
	if (portalHandler)
		walkPortals(domain, portalHandler);
	if (registeredHandler)
		walkRegistereds(domain, registeredHandler);
	if (specialHandler)
		walkSpecials(domain, specialHandler);
	if (interruptHandlerHandler)
		walkInterrupHandlers(domain, interruptHandlerHandler);
}

#endif				/* ENABLE_GC */



//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
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

