//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright � 2007 Sombrio Systems Inc. All rights reserved.
// Copyright � 1998-2002 Michael Golm. All rights reserved.
// Copyright � 1997-2001 The JX Group. All rights reserved.
// Copyright � 2001-2002 Joerg Baumann. All rights reserved.
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
// Garbage collector
//=================================================================================

#ifdef ENABLE_GC
#include "all.h"

#include "gc_common.h"
#include "gc_impl.h"
#include "gc_memcpy.h"
#include "gc_move.h"
#include "gc_pa.h"
#include "gc_pgc.h"

//FIXME
inline void gc_memcpy4(u4_t * dst, u4_t * src, size_t size)
{
	gc_memcpy(dst, src, size * 4);
}

struct MemoryProxy_s *gc_impl_shallowCopyMemory(u4_t * dst, struct MemoryProxy_s
						*srcObj)
{
	struct MemoryProxy_s *dstObj;

	dstObj = (struct MemoryProxy_s *) ptr2ObjectDesc(dst);

	setObjFlags(dstObj, OBJFLAGS_MEMORY);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	memory_copy_intradomain(dstObj, srcObj);
	return dstObj;
}

ObjectDesc *gc_impl_shallowCopyObject(u4_t * dst, ObjectDesc * srcObj)
{
	ObjectDesc *dstObj;

	dstObj = ptr2ObjectDesc(dst);

	IF_DBG_GC( {
		  ClassDesc * cl = obj2ClassDesc(srcObj); printf("   MOVE OBJECT %p %s to %p\n", srcObj, cl->name, dstObj);}
	);

	setObjFlags(dstObj, OBJFLAGS_OBJECT);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;

	PGCB(MOVE);
	gc_memcpy4(dstObj->data, srcObj->data, OBJSIZE_OBJECT(obj2ClassDesc(srcObj)->instanceSize));
	PGCE(MOVE);
	return dstObj;
}

void gc_impl_walkContentObject(DomainDesc * domain, ObjectDesc * obj, HandleReference_t handleReference)
{
	ClassDesc *cl;

	CHECK_STACK_SIZE(obj, 20);

	// FIXME urgent
	//ASSERTOBJECT(obj);
	ASSERT(obj->vtable != NULL);
	cl = *(ClassDesc **) (((ObjectDesc *) (obj))->vtable - 1);
	// FIXME cl = obj2ClassDesc(obj);
	ASSERTCLASSDESC(cl);
	IF_DBG_GC(printf("gc_impl_walkContent %p\n", obj));
	FORBITMAP(cl->map, cl->instanceSize, {
		  /* reference slot */
		  handleReference(domain, (ObjectDesc **) & (obj->data[index]));
		  IF_DBG_GC(printf("   REFSLOT %p[%ld]=%p\n", obj, index, (void *) (obj->data[index])));}, {
		  IF_DBG_GC(printf("   NUMSLOT %p[%ld]=%ld\n", obj, index, (u4_t) (obj->data[index])));});
}

DEPDesc *gc_impl_shallowCopyService(u4_t * dst, DEPDesc * srcObj)
{
	DEPDesc *dstObj;

	dstObj = (DEPDesc *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE SERVICE %p (obj = %p;proxy = %p) to %p\n", srcObj, srcObj->obj, srcObj->proxy, dstObj);
	    );

	setObjFlags(dstObj, OBJFLAGS_SERVICE);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	PGCB(MOVE);
	gc_memcpy((u4_t *) dstObj, (u4_t *) srcObj, sizeof(DEPDesc));
	PGCE(MOVE);
	return dstObj;
}

ServiceThreadPool *gc_impl_shallowCopyServicePool(u4_t * dst, ServiceThreadPool * srcObj)
{
	ServiceThreadPool *dstObj;

	dstObj = (ServiceThreadPool *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE SERVICEPOOL %p  to %p\n", srcObj, dstObj);
	    );

	setObjFlags(dstObj, OBJFLAGS_SERVICE_POOL);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	PGCB(MOVE);
	gc_memcpy((u4_t *) dstObj, (u4_t *) srcObj, sizeof(ServiceThreadPool));
	PGCE(MOVE);
	return dstObj;
}

void gc_impl_walkContentService(DomainDesc * domain, DEPDesc * obj, HandleReference_t handleReference)
{
	ThreadDesc *tp;
	ThreadDescProxy *tpr;
	HandleReference_t handler = handleReference;
	//printf("walkContentService %p\n", obj);
	IF_DBG_GC(printf("  going to copy service.(obj=%p,proxy=%p) \n", obj->obj, obj->proxy));
	handleReference(domain, &(obj->obj));
	handleReference(domain, (ObjectDesc **) & (obj->proxy));
#ifdef NEW_PORTALCALL
	handleReference(domain, (ObjectDesc **) & (obj->pool));
#else
	MOVETCB(obj->receiver);

	if (obj->firstWaitingSender) {
/*		sys_panic("WalkService: fws not yet impl");*/
		/* no need to update waiting threads because they do not contain a direct pointer to a movable data structure */
	}
#endif
	IF_DBG_GC(printf("  copied service.(obj=%p,proxy=%p) \n", obj->obj, obj->proxy));
}

void gc_impl_walkContentServicePool(DomainDesc * domain, ServiceThreadPool * obj, HandleReference_t handleReference)
{
	ThreadDesc *tp;
	ThreadDescProxy *tpr;
	HandleReference_t handler = handleReference;
	MOVETCB(obj->firstReceiver);
}


ArrayDesc *gc_impl_shallowCopyArray(u4_t * dst, ArrayDesc * srcObj)
{
	ArrayDesc *dstObj;
	ArrayClassDesc *c;

	dstObj = (ArrayDesc *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE ARRAY %p (%s) to %p\n", srcObj, srcObj->arrayClass->name, dstObj);
	    );

	setObjFlags(dstObj, OBJFLAGS_ARRAY);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	c = (ArrayClassDesc *) (dstObj->arrayClass = srcObj->arrayClass);
	dstObj->size = srcObj->size;
	PGCB(MOVE);
#ifdef ALL_ARRAYS_32BIT
	gc_memcpy4(dstObj->data, srcObj->data, srcObj->size);
#else
	if (ARRAY_8BIT(c)) {
		gc_memcpy(dstObj->data, srcObj->data, srcObj->size);
	} else if (ARRAY_16BIT(c)) {
		gc_memcpy(dstObj->data, srcObj->data, srcObj->size * 2);
	} else {
		gc_memcpy4(dstObj->data, srcObj->data, srcObj->size);
	}
#endif
	PGCE(MOVE);
	return dstObj;
}

void gc_impl_walkContentArray(DomainDesc * domain, ArrayDesc * obj, HandleReference_t handleReference)
{
	if (obj->arrayClass->name[1] == 'L' || obj->arrayClass->name[1] == '[') {
		/* reference array */
		u4_t i;
		for (i = 0; i < obj->size; i++) {
			handleReference(domain, (ObjectDesc **) & (obj->data[i]));
		}
		IF_DBG_GC(printf("     copied array contents %p %s\n", obj, obj->arrayClass->name));
	} else {
		IF_DBG_GC(printf("     don't copy primitive array %p %s\n", obj, obj->arrayClass->name));
	}
}

Proxy *gc_impl_shallowCopyPortal(u4_t * dst, Proxy * srcObj)
{
	Proxy *dstObj;

	dstObj = (Proxy *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE PORTAL %p to %p\n", srcObj, dstObj));

	setObjFlags(dstObj, OBJFLAGS_PORTAL);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	dstObj->targetDomain = srcObj->targetDomain;
	dstObj->targetDomainID = srcObj->targetDomainID;
	dstObj->index = srcObj->index;

	return dstObj;
}


CASProxy *gc_impl_shallowCopyCAS(u4_t * dst, CASProxy * srcObj)
{
	CASProxy *dstObj;

	dstObj = (CASProxy *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE CAS %p (index = %ld) to %p\n", srcObj, srcObj->index, dstObj));

	setObjFlags(dstObj, OBJFLAGS_CAS);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	dstObj->index = srcObj->index;
	return dstObj;
}

DomainProxy *gc_impl_shallowCopyDomain(u4_t * dst, DomainProxy * srcObj)
{
	DomainProxy *dstObj;

	dstObj = (DomainProxy *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE DOMAIN %p (domainID = %ld) to %p\n", srcObj, srcObj->domainID, dstObj));

	setObjFlags(dstObj, OBJFLAGS_DOMAIN);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	dstObj->domain = srcObj->domain;
	dstObj->domainID = srcObj->domainID;
	return dstObj;
}

ObjectDesc *gc_impl_shallowCopyCpuState(u4_t * dst, ThreadDescProxy * srcObj)
{
	ThreadDescProxy *dstObj = (ThreadDescProxy *) ptr2ObjectDesc(dst);
	ThreadDesc *dstThread, *srcThread;

	srcThread = cpuState2thread(srcObj);

	dstThread = &(dstObj->desc);	/* can not use cpuState2thread because dst obj has no flags yet */

	IF_DBG_GC(printf("   MOVE CPUSTATE %p  to %p\n", srcObj, dstObj));

	memcpy(dstThread, srcThread, sizeof(ThreadDesc));



	setObjFlags(dstObj, OBJFLAGS_CPUSTATE);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	return dstObj;
}

AtomicVariableProxy *gc_impl_shallowCopyAtomVar(u4_t * dst, AtomicVariableProxy * srcObj)
{
	AtomicVariableProxy *dstObj;

	dstObj = (AtomicVariableProxy *) ptr2ObjectDesc(dst);

	IF_DBG_GC(printf("   MOVE ATOMIC_VARIABLE %p (value = %p) to %p\n", srcObj, srcObj->value, dstObj));

	setObjFlags(dstObj, OBJFLAGS_ATOMVAR);
#ifdef USE_QMAGIC
	setObjMagic(dstObj, MAGIC_OBJECT);
#endif
	dstObj->vtable = srcObj->vtable;
	dstObj->value = srcObj->value;
	dstObj->blockedThread = srcObj->blockedThread;
	return dstObj;
}

void gc_impl_walkContentAtomVar(DomainDesc * domain, AtomicVariableProxy * obj, HandleReference_t handleReference)
{
	ThreadDesc *tp;
	ThreadDescProxy *tpr;
	HandleReference_t handler = handleReference;
	MOVETCB(obj->blockedThread);
	handleReference(domain, &(obj->value));

}

void gc_impl_walkContentDomainProxy(DomainDesc * domain, DomainProxy * obj, HandleReference_t handleReference)
{
}

void gc_impl_walkContentForeignCPUState(DomainDesc * domain, ThreadDescForeignProxy * obj, HandleReference_t handleReference)
{
	handleReference(domain, (ObjectDesc **) & (obj->domain));
}

#ifdef NEW_PORTALCALL
#ifdef DEBUG
static void check_inwait(DomainDesc * domain, ThreadDesc * t)
{
	int i;
	for (i = 0; i < CONFIG_JEM_MAX_SERVICES; i++) {
		if (domain->pools[i]) {
			if (domain->pools[i]->lastWaitingSender && t->id == domain->pools[i]->lastWaitingSender->id
			    && t->domain->id == domain->pools[i]->lastWaitingSender->domain->id) {
				printf("domain=%d pool=%d; t->state=%s\n", domain->id, i, get_state(t));

			}
		}
	}
}
#endif
#endif

/* object references in thread control block */
void gc_impl_walkContentCPUState(DomainDesc * domain, ThreadDescProxy * obj, HandleReference_t handleReference)
{

	ThreadDesc *tp;
	ThreadDescProxy *tpr;
	ThreadDesc *t = cpuState2thread(obj);

	HandleReference_t handler = handleReference;

#ifdef DBG_GC
	printf("GC: correct CPUState content");
#endif

	/* pointer to context */
	t->contextPtr = &(t->context);

	/* pointer to threads in our domain */
	MOVETCB(t->nextInDomain);
	MOVETCB(t->prevInDomain);
#ifndef NEW_SCHED
	MOVETCB(t->nextInRunQueue);
#else
	Sched_gc_tcb(domain, t, handler);
#endif

	/* pointer to threads in other domains and pointer to this thread in other domains */
	if (t->blockedInServiceThread)
		MOVETCB(t->blockedInServiceThread->mostRecentlyCalledBy);

	/* the reverse of the above */
	if (t->mostRecentlyCalledBy)
		MOVETCB(t->mostRecentlyCalledBy->blockedInServiceThread);


#ifdef NEW_PORTALCALL
	MOVETCB(t->nextInReceiveQueue);
#endif

#ifdef NEW_PORTALCALL
#ifdef DEBUG
	/* check all domains whether this thread is in waiterlist */
	foreachDomain1(check_inwait, t);
#endif
#endif

	if (t->state == STATE_PORTAL_WAIT_FOR_RCV) {
		if (t->blockedInDomain->id == t->blockedInDomainID) {
			DEPDesc *svc = t->blockedInDomain->services[t->blockedInServiceIndex];
#ifdef NEW_PORTALCALL
			ServiceThreadPool *pool = svc->pool;
			printf("Handle sender %d.%d, svc=%d\n", TID(t), t->blockedInServiceIndex);
			if (pool->lastWaitingSender && t->id == pool->lastWaitingSender->id
			    && t->domain->id == pool->lastWaitingSender->domain->id) {
				MOVETCB(pool->lastWaitingSender);
			}
			if (pool->firstWaitingSender && t->id == pool->firstWaitingSender->id
			    && t->domain->id == pool->firstWaitingSender->domain->id) {
				MOVETCB(pool->firstWaitingSender);
			} else {
				ThreadDesc *t0;
				if (pool->firstWaitingSender) {
					for (t0 = pool->firstWaitingSender; t0->nextInDEPQueue != NULL; t0 = t0->nextInDEPQueue) {
						//printf("%d.%d\n", TID(t0->nextInDEPQueue));
						if (t0->nextInDEPQueue->id == t->id
						    && t0->nextInDEPQueue->domain->id == t->domain->id) {
							MOVETCB(t0->nextInDEPQueue);
							goto found;
						}
					}
				}
				sys_panic("TCB %d.%d not found in wait queue of domain %d", TID(t), t->blockedInDomain->id);
			}
		      found:
#else
			sys_panic("move TCB in wait queue NYI");
#endif
		} else {
			printf("Blocked in dead domain %d\n", t->blockedInDomainID);
		}
	}

	if (t->state == STATE_PORTAL_WAIT_FOR_RET) {
		//sys_panic("move TCB with running service execution not yet supported");
		// correct source pointer in receiver domain!!!!
	}

	if (t->state == STATE_PORTAL_WAIT_FOR_PARAMCOPY) {
#ifdef NO_RESTART
		if (t->blockedInDomain->id == t->blockedInDomainID) {
			/* servers reference table that is used to detect cycles during parameter copying */
			u4_t n, i;
			ThreadDesc *svct = t->blockedInServiceThread;
			n = svct->n_copied;
			for (i = 0; i < n; i++) {
				printf("Copy src: %p\n", svct->copied[i].src);
				handler(domain, &(svct->copied[i].src));	/* note: src!! */
			}
		} else {
			/* this thread waits for a terminated domain */
		}
#endif				/* NO_RESTART */
	}
	if (t->state == STATE_PORTAL_WAIT_FOR_RETCOPY) {
#ifdef NO_RESTART
		if (t->blockedInDomain->id == t->blockedInDomainID) {
			/* servers reference table that is used to detect cycles during parameter copying */
			u4_t n, i;
			ThreadDesc *svct = t->blockedInServiceThread;
			n = svct->n_copied;
			for (i = 0; i < n; i++) {
				printf("Copy dst: %p\n", svct->copied[i].dst);
				handler(domain, &(svct->copied[i].dst));	/* note: dst!! */
			}
		} else {
			/* this thread waits for a terminated domain */
		}
#endif				/* NO_RESTART */
	}

	/* correct context pointer */
	t->contextPtr = &(t->context);


#ifdef KERNEL
	if (t->isInterruptHandlerThread) {
		if (!gc_correct_irqHandlers(t, domain, handleReference)) {
			sys_panic("Thread %d.%d marked as interrupt handler but not listed in ithreads", TID(t));
		}
	}
#endif


	/* portal call parameters in TCB */

	/* data for incoming call */
	if (t->portalParameter) {
		handler(domain, (ObjectDesc **) & (t->portalParameter));
	}
	if (t->entry) {
		handler(domain, (ObjectDesc **) & (t->entry));
	}
	if ((t->portalReturnType & PORTAL_RETURN_TYPE_REFERENCE)
	    && (t->portalReturn)) {
		gc_dprintf("move portalReturn\n");
		handler(domain, (ObjectDesc **) & (t->portalReturn));
	}
#if 0
	/* reference table that is used to detect cycles during parameter copying */
	{
		u4_t n, i;
		n = t->n_copied;
		for (i = 0; i < n; i++) {
			handler(domain, &(t->copied[i].dst));
		}
	}
#endif

#if 0				// CHECK THIS
	/* data for outgoing call */
	if (t->state == STATE_PORTAL_WAIT_FOR_RCV || t->state == STATE_PORTAL_WAIT_FOR_RET) {
		/* Situation: thread is blocked waiting for portal call and parameters are not yet copied
		 * to target domain */
		IF_DBG_GC(printf("gc_impl_walkContentCPUState: Move portal call parameters of thread %p\n", t));
		if (t->depParams) {
			ClassDesc *oclass;
			jint **paramlist = (jint **) t->depParams;
			u4_t i, methodIndex, numberArgs;
			jbyte *argTypeMap;
			Proxy *proxy;

			printf("   DEPPARAMS in thread %d.%d\n", TID(t));
			proxy = *(Proxy **) paramlist;
			oclass = obj2ClassDesc((ObjectDesc *) proxy);
			methodIndex = t->depMethodIndex;
			numberArgs = oclass->methodVtable[methodIndex]->numberOfArgs;
			argTypeMap = oclass->methodVtable[methodIndex]->argTypeMap;

			IF_DBG_GC(printf
				  ("%s %s\n", oclass->methodVtable[methodIndex]->name,
				   oclass->methodVtable[methodIndex]->signature));
			for (i = 1; i < numberArgs + 1; i++) {
				if (isRef(argTypeMap, numberArgs, i - 1)) {
					//sys_panic("   PORTALPARAM REF\n");
					printf("   PORTALPARAM REF\n");
					if ((void *) t->depParams[i] == NULL)
						continue;
					handler(domain, (ObjectDesc **) t->depParams + i);
				} else {
					printf("   PORTALPARAM NUM\n");
				}
			}
		}
	} else {
		ASSERT(t->depParams == NULL);
	}
#endif

	/* data for incoming call */
	if (t->isPortalThread) {
		IF_DBG_GC(printf("Move portal call in-parameters of thread %p\n", t));
		{
			ThreadDesc *source = t->mostRecentlyCalledBy;
			if (source != NULL) {
				//u4_t numberArgs = (jint**)source->depNumParams;
				u4_t i;
				ObjectDesc *obj;
				ClassDesc *oclass;
				u4_t methodIndex, numberArgs;
				jbyte *argTypeMap;
				DEPDesc *dep;
				dep = t->processingDEP;
				obj = dep->obj;
				oclass = obj2ClassDesc(obj);
				methodIndex = source->depMethodIndex;
				numberArgs = oclass->methodVtable[methodIndex]->numberOfArgs;
				argTypeMap = oclass->methodVtable[methodIndex]->argTypeMap;
#ifdef DBG_GC
				printf("methodindex=%ld, numberargs: %ld\n", methodIndex, numberArgs);
				printf("source %p \n", source);
				printf("obj %p \n", obj);
				printf("class %p %s method %s\n", oclass, oclass->name, oclass->methodVtable[methodIndex]->name
				       /*, oclass->methodVtable[methodIndex]->signature */
				    );
#endif
				handler(domain, (ObjectDesc **) & (t->myparams[0]));
				if (t->depParams != NULL) {
					for (i = 1; i < numberArgs + 1; i++) {
						if (isRef(argTypeMap, numberArgs, i - 1)) {
							//printf("   INPORTALPARAM REF\n");
							if ((void *) t->depParams[i] == NULL)
								continue;
							handler(domain, (ObjectDesc **) & (t->myparams[i]));
						} else {
							// printf("   INPORTALPARAM NUM\n");
						}
					}
				}
				IF_DBG_GC(printf("DONE inportal %p\n", (void *) (t->myparams[0])));
			}
		}
	} else {
		ASSERT(t->mostRecentlyCalledBy == NULL);
	}
	if (t->processingDEP) {
		if (t->processingDEP->refcount > 1) {
			gc_dprintf("move processingDEP\n");
			handler(domain, (ObjectDesc **) & (t->processingDEP));
		} else {
			t->processingDEP = NULL;
		}
	}
#ifdef STACK_ON_HEAP
	/* correct stack */
	handler(domain, (ObjectDesc **) & (t->stackObj));
#endif				/* STACK_ON_HEAP */
}

void gc_impl_walkContent2(DomainDesc * domain, ObjectDesc * obj, HandleReference_t handleReference)
{
	u4_t flags = getObjFlags(obj);
	switch (flags & FLAGS_MASK) {
	case OBJFLAGS_ARRAY:
		gc_impl_walkContentArray(domain, (ArrayDesc *) obj, handleReference);
		break;
	case OBJFLAGS_OBJECT:
		gc_impl_walkContentObject(domain, obj, handleReference);
		break;
	case OBJFLAGS_SERVICE:
		gc_impl_walkContentService(domain, (DEPDesc *) obj, handleReference);
		break;
	case OBJFLAGS_SERVICE_POOL:
		gc_impl_walkContentServicePool(domain, (DEPDesc *) obj, handleReference);
		break;
	case OBJFLAGS_ATOMVAR:
		gc_impl_walkContentAtomVar(domain, (AtomicVariableProxy *) obj, handleReference);
		break;
	case OBJFLAGS_FOREIGN_CPUSTATE:
		gc_impl_walkContentForeignCPUState(domain, (ThreadDescForeignProxy *) obj, handleReference);
		break;
	case OBJFLAGS_EXTERNAL_STRING:
		ASSERT(domain == domainZero);
	case OBJFLAGS_PORTAL:
	case OBJFLAGS_MEMORY:
	case OBJFLAGS_CAS:
	case OBJFLAGS_DOMAIN:
		break;
	case OBJFLAGS_CPUSTATE:
		gc_impl_walkContentCPUState(domain, obj, handleReference);
		break;
#ifdef STACK_ON_HEAP
	case OBJFLAGS_STACK:
		gc_impl_walkContentStack(domain, obj, handleReference);
		break;
#endif
	default:
		printf("FLAGS: %lx\n", flags);
		dump_data(obj);
		sys_panic("WRONG HEAP DATA");
	}
}

void gc_impl_walkContent(DomainDesc * domain, ObjectDesc * obj, HandleReference_t handleReference)
{
	u4_t flags = getObjFlags(obj);

	CHECK_STACK_SIZE(obj, 20);

	switch (flags & FLAGS_MASK) {
	case OBJFLAGS_ARRAY:
		gc_impl_walkContentArray(domain, (ArrayDesc *) obj, handleReference);
		break;
	case OBJFLAGS_OBJECT:
		gc_impl_walkContentObject(domain, obj, handleReference);
		break;
	case OBJFLAGS_SERVICE:
		gc_impl_walkContentService(domain, (DEPDesc *) obj, handleReference);
		break;
	case OBJFLAGS_SERVICE_POOL:
		gc_impl_walkContentServicePool(domain, (DEPDesc *) obj, handleReference);
		break;
	case OBJFLAGS_ATOMVAR:
		gc_impl_walkContentAtomVar(domain, (AtomicVariableProxy *) obj, handleReference);
		break;
	case OBJFLAGS_FOREIGN_CPUSTATE:
		gc_impl_walkContentForeignCPUState(domain, (ThreadDescForeignProxy *) obj, handleReference);
		break;
#ifdef STACK_ON_HEAP
	case OBJFLAGS_STACK:
		gc_impl_walkContentStack(domain, (StackProxy *) obj, handleReference);
		break;
#endif
	default:
#ifdef DBG_GC
		printf("walkContent: ignore object content with flags %d\n", flags & FLAGS_MASK);
#endif				/* DBG_GC */
	}
}

#endif				/* ENABLE_GC */


