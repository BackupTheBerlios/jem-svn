//===========================================================================
// File: domain.h
//
// Jem/JVM thread interface.
//
//===========================================================================

#ifndef _THREAD_H
#define _THREAD_H

#include <linux/smp.h>

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
#define PORTAL_RETURN_TYPE_NUMERIC   0
#define PORTAL_RETURN_TYPE_REFERENCE 1
#define PORTAL_RETURN_TYPE_EXCEPTION 3
#define PORTAL_RETURN_IS_OBJECT(x) (x & 1)
#define PORTAL_RETURN_IS_EXCEPTION(x) (x == 3)
#define PORTAL_RETURN_SET_EXCEPTION(x) ((x) = 3)
#define MAX_EVENT_THREADSWITCH 2000000
#define MAGIC_THREAD 0xcabaaffe
#define idle_thread cur_idle_thread()
#define LOCK(obj) { register int reg = 1; for(;;) {__asm__ __volatile__ ("xchgl %1,%0" :"=r" (reg), "=m" (*obj) :"r" (reg), "m" (*obj)); if (reg == 0) break; }}
#define UNLOCK(obj) { register int reg = 0;        __asm__ __volatile__ ("xchgl %1,%0" :"=r" (reg), "=m" (*obj) : "r" (reg), "m" (*obj));}
#define TID(t) (t)->domain->id, (t)->id
#define THREAD_NAME_MAXLEN 40


struct copied_s {
	struct ObjectDesc_s  *src;
	u32                 flags;
};


typedef struct {
	u32 dummy[5];
} SchedDescUntypedThreadMemory;


typedef struct ThreadDesc_s  {
	unsigned long                   *contextPtr;
	u64                             *stack;		/* pointer to lowest element in stack (small address) */
	u64                             *stackTop;		/* pointer to topmost element in stack (large address) */
	u32                             state;
	struct DomainDesc_s             *domain;
	struct ThreadDesc_s             *nextInRunQueue;
	struct ThreadDesc_s             *nextInDEPQueue;
	unsigned long                   context[14];
	jint                            *depParams;
	jint                            depMethodIndex;
    jboolean                        depSwitchBack;    /* if true the PortalThread should switch back to the caller */ 
	struct ThreadDesc_s             *linkedDEPThr;	/* if thread is servicing a Portal  this is the Pointer to 
						                the original caller (transitiv)
						                else this points to the servicing Thread (transitiv)
						                if no portal is used, this points to the thread itself */
	struct ThreadDesc_s             *nextInReceiveQueue;
	struct ThreadDesc_s             *nextInGCQueue;
	jint                            curCpuId;		/* the ID of the last used CPU */
	jboolean                        isPortalThread;	/* this thread is a Portalthread */
	struct DEPDesc_s                *processingDEP;	/* this thread is currently servicing a Portal */
	struct DomainDesc_s             *blockedInDomain;	/* this thread is currently waiting for a service of that domain */
	u32                             blockedInDomainID;	/* this thread is currently waiting for a service of that domain (detect terminated domain)*/
	u32                             blockedInServiceIndex;	/* index of the service this thread is waiting for */
	struct ThreadDesc_s             *blockedInServiceThread;	/* service thread that executes our call 
							                 * (needed to update mostRecentlyCalledBy during GC) */
	struct ThreadDesc_s             *mostRecentlyCalledBy; /* needed to update the client thread pointer after svc exec */
	struct DomainDesc_s             *callerDomain;
	u32                             callerDomainID;
	struct ThreadDesc_s             *nextInDomain;
	struct ThreadDesc_s             *prevInDomain;
	struct ThreadDesc_s             *next;     /* general Pointer, used from Java-Level and Atomic Variable */
	jint                            wakeupTime;
	jint                            unblockedWithoutBeingBlocked;
	struct ObjectDesc_s             *schedInfo;	/* may be used by a scheduler to store thread specific infos */
	jint                            debug[2];		/* two debug slots */
	SchedDescUntypedThreadMemory    untypedSchedMemory;
	struct ProfileDesc_s            *profile;
	sigset_t                        sigmask;
	jint                            preempted;
	u32                             magic;
	struct DomainDesc_s             *schedulingDomain;
	char                            name[THREAD_NAME_MAXLEN];
	struct ObjectDesc_s             *portalParameter;	/* implicit parameter passed during a portal call 
					                                can be used to pass credentials to target domain */
	struct ObjectDesc_s             *entry;	/* thread entry object containing run method */
	struct ObjectDesc_s             *portalReturn;	/* may contain object reference or numeric data ! */
	jint                            portalReturnType;	/* 0=numeric 1=reference 3=exception */
	struct copied_s                 *copied;	/* onlu used when thread receives portal calls */
	u32                             n_copied;
	u32                             max_copied;
	u32                             isInterruptHandlerThread;
	u32                             isGCThread;
	jint                            *myparams;
	u32                             id;
	struct StackProxy_s             *stackObj;
	u32                             numberPreempted;
	u64                             cputime;
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

typedef struct StackProxy_s {
	code_t              *vtable;
	u32                 size; 
	struct ThreadDesc_s *thread;
} StackProxy;

struct profile_event_threadswitch_s {
	unsigned long long          timestamp;
	struct ThreadDesc_s         *from;
	struct ThreadDesc_s         *to;
	char                        *ip_from;
	char                        *ip_to;
};


extern struct profile_event_threadswitch_s  *profile_event_threadswitch_samples;
extern u32                                  profile_event_threadswitch_n_samples;
extern struct ThreadDesc_s                  *__idle_thread[CONFIG_NR_CPUS];
extern u32                                  *trace_sched_ip;
extern u32                                  *last_trace_sched_ip;
extern struct ThreadDesc_s                  *__current[CONFIG_NR_CPUS];

typedef void (*thread_start_t) (void *);


static inline struct ThreadDesc_s *curthr(void)
{
	return __current[get_cpu()];
}


static inline struct ThreadDesc_s **curthrP(void)
{
	return &__current[get_cpu()];
}


static inline void set_current(struct ThreadDesc_s * t)
{
	__current[get_cpu()] = t;
}


static inline struct DomainDesc_s *curdom(void)
{
	return curthr()->domain;
}


static inline struct ThreadDesc_s *cur_idle_thread(void)
{
	return __idle_thread[get_cpu()];
}


static inline void check_not_in_runq(struct ThreadDesc_s * thread)
{
}


#ifdef KERNEL
void save(struct irqcontext *sc);
#else
void save(struct sigcontext *sc);
#endif

static inline void stack_push(u32 ** sp, u32 data)
{
	(*sp)--;
	**sp = data;
}




//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
// Copyright (C) 1998-2002 Michael Golm
// Copyright (C) 2001-2002 Meik Felser
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
// As a special exception, if other files instantiate templates or use macros or 
// inline functions from this file, or you compile this file and link it with other 
// works to produce a work based on this file, this file does not by itself cause 
// the resulting work to be covered by the GNU General Public License. However the 
// source code for this file must still be made available in accordance with 
// section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this
// file might be covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================

#endif
