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
//
// Jem/JVM thread interface.
//
//=================================================================================

#ifndef _THREAD_H
#define _THREAD_H

#include <native/task.h>

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

typedef void (*thread_start_t) (void *);


struct copied_s {
	struct ObjectDesc_s  *src;
	u32                 flags;
};


typedef struct {
	u32 dummy[5];
} SchedDescUntypedThreadMemory;


typedef struct ThreadDesc_s  {
	u32                             state;
	struct DomainDesc_s             *domain;
	struct ThreadDesc_s             *nextInDomain;
	struct ThreadDesc_s             *prevInDomain;
	u32                             magic;
	thread_start_t                  entry;
	jboolean                        isInterruptHandlerThread;
	jboolean                        isGCThread;
	jboolean                        isPortalThread;
	u32                             threadID;
    void                            *createParam;
    jint                            *portalParams;
    RT_TASK                         task;
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



void threads_init(void);
ThreadDesc *createThread(DomainDesc * domain, thread_start_t thread_start,
			 void *param, int state, int schedParam);
ThreadDesc *createThreadUsingThreadEntry(DomainDesc * domain, ObjectDesc * entry);
ThreadDesc *createInitialDomainThread(DomainDesc * domain, int state, int schedParam);
ThreadDesc *createThreadInMem(DomainDesc * domain, thread_start_t thread_start, void *param, ObjectDesc * entry, 
                              u32 stackSize, int state, int schedParam);
void receive_dep(void *arg);
void receiveDomainDEP(void *arg);
void thread_exit(void);
void terminateThread(ThreadDesc * t);

ThreadDesc *findThreadDesc(ThreadDescForeignProxy *proxy);

// FIXME jgbauman
u32 start_thread_using_code1(ObjectDesc * obj, ThreadDesc * thread,
			      code_t c, u32 param);


#endif
