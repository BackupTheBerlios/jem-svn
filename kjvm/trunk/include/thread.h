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
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================
//
// Jem/JVM thread interface.
//
//=================================================================================

#ifndef _THREAD_H
#define _THREAD_H

#include <native/task.h>
#include "domain.h"
#include "portal.h"

#define STATE_INIT      1
#define STATE_RUNNABLE  2
#define STATE_ZOMBIE    3
#define STATE_SLEEPING  5
#define STATE_WAITING   6
#define STATE_PORTAL_WAIT_FOR_RCV  7	/* waiting for portal thread to become available */
#define STATE_PORTAL_WAIT_FOR_SND  8	/* waiting for incoming portal call */
#define STATE_PORTAL_WAIT_FOR_RET  9	/* waiting for portal call to return */
#define STATE_BLOCKEDUSER 10
#define STATE_AVAILABLE   11
#define STATE_WAIT_ONESHOT   12
#define STATE_PORTAL_WAIT_FOR_PARAMCOPY 13	/* client waiting for portal param copy */
#define STATE_PORTAL_WAIT_FOR_RETCOPY   14	/* client waiting for portal return value copy */
#define THREAD_PPCB  0
#define THREAD_STATE 0x8
#define MAX_EVENT_THREADSWITCH 2000000
#define MAGIC_THREAD 0xcabaaffe
#define TID(t) (t)->domain->id, (t)->id

typedef void (*thread_start_t) (void *);


struct copied_s {
	struct ObjectDesc_s  *src;
	u32                 flags;
};


typedef struct ThreadDesc_s  {
    u32                             magic;
	u32                             state;
    unsigned long                   *stack;		/* pointer to lowest element in stack (small address) */
    unsigned long                   *stackTop;	/* pointer to topmost element in stack (large address) */
    struct DomainDesc_s             *domain;
    struct ThreadDesc_s             *nextInDomain;
    struct ThreadDesc_s             *prevInDomain;
    struct ThreadDesc_s             *next;
    thread_start_t                  entry;
    jboolean                        isInterruptHandlerThread;
    jboolean                        isGCThread;
    jboolean                        isPortalThread;
    u32                             threadID;
    void                            *createParam;
    jint                            *portalParams;
    RT_TASK                         task;
    struct copied_s                 *copied;	/* only used when thread receives portal calls */
    u32                             n_copied;
    u32                             max_copied;
	struct ThreadDesc_s             *mostRecentlyCalledBy;
	struct ThreadDesc_s             *nextInReceiveQueue;
	struct ThreadDesc_s             *nextInDEPQueue;
	struct ThreadDesc_s             *blockedInServiceThread;
	struct DomainDesc_s             *callerDomain;
	struct DomainDesc_s             *blockedInDomain;	/* this thread is currently waiting for a service of that domain */
	u32                             blockedInDomainID;	/* this thread is currently waiting for a service of that domain (detect terminated domain)*/
	u32                             blockedInServiceIndex;	/* index of the service this thread is waiting for */
	u32                             callerDomainID;
	jint                            *myparams;	
	jint                            depMethodIndex;
	jint                            *depParams;
	struct ObjectDesc_s             *portalParameter;	/* implizit parameter passed during a portal call */
	struct DEPDesc_s                *processingDEP;	/* this thread is currently servicing a Portal */
	struct ObjectDesc_s             *portalReturn;	/* may contain object reference or numeric data ! */
	jint                            portalReturnType;	/* 0=numeric 1=reference 3=exception */
} ThreadDesc;


typedef struct ThreadDescProxy_s {
    code_t              *vtable;
    struct ThreadDesc_s desc;
} ThreadDescProxy;


typedef struct ThreadDescForeignProxy_s {
	code_t                  *vtable;
	struct DomainProxy_s    *domain;   /* used to find domain */
	int                     threadID;  /* used to find thread if thread pointer is invalid */
	struct ThreadDesc_s     *thread;   /* used to find thread fast; is only valid if gcEpoch is equal to gc epoch of target domain */
	int                     gcEpoch;   /* GC epoch of target domain during creation of this object */
} ThreadDescForeignProxy;

typedef struct MappedMemoryProxy_s {
	code_t  *vtable;
	char    *mem;
} MappedMemoryProxy;


extern struct ThreadDesc_s                  *__current[CONFIG_NR_CPUS];


static inline struct ThreadDesc_s *curthr(void)
{
	return __current[smp_processor_id()];
}


static inline struct ThreadDesc_s **curthrP(void)
{
	return &__current[smp_processor_id()];
}


static inline void set_current(struct ThreadDesc_s * t)
{
	__current[smp_processor_id()] = t;
}


static inline struct DomainDesc_s *curdom(void)
{
	return curthr()->domain;
}


void        threadsInit(void);
ThreadDesc  *createThread(DomainDesc * domain, thread_start_t thread_start, void *param, int state, 
                         int prio, char *thName);
ThreadDesc  *createInitialDomainThread(DomainDesc * domain, int state, int schedParam);
ThreadDesc  *createThreadInMem(DomainDesc * domain, thread_start_t thread_start, void *param, 
                              ObjectDesc * entry, u32 stackSize, int state, int prio, char *tName);
void        thread_exit(void);
void        terminateThread(ThreadDesc * t);
ThreadDesc  *findThreadDesc(ThreadDescForeignProxy *proxy);
void        thread_prepare_to_copy(void);


#endif
