//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
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
// Thread management
// 
//=================================================================================
// 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "malloc.h"


extern ClassDesc *cpuStateClass;

ThreadDesc   *__current[CONFIG_NR_CPUS] = { NULL };

void        start_initial_thread(void *dummy);
static void destroyCurrentThread(void);

void thread_exit()
{
	destroyCurrentThread();
	printk(KERN_ERR "Looping forever. Should never be reached.\n");
	for (;;);
}

void jemTaskHook(void *cookie)
{
    ThreadDesc  *thread;
    RT_TASK     *task = T_DESC(cookie);

    thread  = (ThreadDesc *) task->thread_base.cookie;
    __current[smp_processor_id()] = thread;
    if (rt_mutex_acquire(&thread->domain->gc.gcLock, TM_INFINITE)) return;
    if (thread->domain->gc.active) {
        if (thread->domain->gc.gcSuspended != NULL) {
            thread->next = thread->domain->gc.gcSuspended;
        }
        thread->domain->gc.gcSuspended = thread;
        rt_mutex_release(&thread->domain->gc.gcLock);
        rt_task_suspend(task);
        return;
    }
    rt_mutex_release(&thread->domain->gc.gcLock);
}

ThreadDesc *createThreadInMem(DomainDesc * domain, thread_start_t thread_start, void *param, 
                              ObjectDesc * entry, u32 stackSize, int state, int prio, char *tName)
{
	int                 result;
	ThreadDesc          *thread;
	ThreadDescProxy     *threadproxy;

	threadproxy             = jemMallocThreadDescProxy(cpuStateClass);
	thread                  = &(threadproxy->desc);
	thread->entry           = (thread_start_t) entry;
	thread->magic           = MAGIC_THREAD;
	thread->domain          = domain;
	thread->threadID        = domain->currentThreadID++;
	thread->nextInDomain    = domain->threads;
	if (domain->threads)
		domain->threads->prevInDomain = thread;
	thread->prevInDomain    = NULL;
	domain->threads         = thread;
	thread->state           = state;
    thread->createParam     = param;
    thread->portalParams    = NULL;
    thread->next            = NULL;

    if ((result = rt_task_create(&thread->task, tName, stackSize, prio, T_FPU)) < 0) {
        jemFreeThreadDesc(thread);
        printk(KERN_ERR "Error creating thread task, rc=%d.\n", result);
        return NULL;
    }

    thread->stack           = thread->task.thread_base.tcb.stackbase;
    thread->stackTop        = thread->stack + thread->task.thread_base.tcb.stacksize;

    if ((result = rt_task_start(&thread->task, thread_start, (void *) thread)) < 0) {
        jemFreeThreadDesc(thread);
        printk(KERN_ERR "Error starting Jem thread task, rc=%d\n", result);
        return NULL;
    }

	return thread;
}


ThreadDesc *createInitialDomainThread(DomainDesc * domain, int state, int prio)
{
    char thName[14];

    sprintf(thName, "dom%3dInitial", domain->id);
	return createThreadInMem(domain, start_initial_thread, NULL, NULL, 1024, state, prio, thName);
}


ThreadDesc *createThread(DomainDesc * domain, thread_start_t thread_start, void *param, int state, 
                         int prio, char *thName)
{
	ThreadDesc *t = createThreadInMem(domain, thread_start, param, NULL, 1024, state, prio, thName);
	return t;
}

void freeThreadMem(ThreadDesc * t)
{
	DomainDesc      *domain;

	domain = t->domain;
	if (t->portalParams)
		jemFree(t->portalParams, PORTALPARAMS_SIZE MEMTYPE_OTHER);
    jemFreeThreadDesc(t);
}

ThreadDesc *findThreadByID(DomainDesc * domain, u32 tid)
{
	ThreadDesc *t;
	for (t = domain->threads; t != NULL; t = t->nextInDomain) {
		if (t->threadID == tid)
			return t;
	}
	return NULL;
}

ThreadDesc *findThreadDesc(ThreadDescForeignProxy * proxy)
{
	DomainDesc *domain;
	if (proxy->domain->domain->id == proxy->domain->domainID) {
		domain = proxy->domain->domain;
	} else {
		/* domain terminated -> no TCBs */
		return NULL;
	}

	if (proxy->gcEpoch != domain->gc.epoch) {
		ThreadDesc *t;
		for (t = domain->threads; t != NULL; t = t->nextInDomain) {
			if (t->threadID == proxy->threadID) {
				/* found thread -> update proxy and return TCB */
				proxy->thread   = t;
				proxy->gcEpoch  = domain->gc.epoch;
				return t;
			}
		}
		/* thread not found (probably terminated) -> return NULL */
		return NULL;
	}
	return proxy->thread;	// TODO: CHECK DOMAIN AND GC EPOCH
}


/* thread must not be in any queue !! caller of terminateThread must ensure this! */
void terminateThread(ThreadDesc * thread)
{
    RT_TASK     *t;

    /* remove thread from domain thread list */
    if (thread->prevInDomain) {
        thread->prevInDomain->nextInDomain = thread->nextInDomain;
    } else {
        thread->domain->threads = thread->nextInDomain;
    }
    if (thread->nextInDomain) {
        thread->nextInDomain->prevInDomain = thread->prevInDomain;
    }

    t   = &thread->task;
    freeThreadMem(thread);
    rt_task_delete(t);
}

static void destroyCurrentThread(void)
{
    terminateThread(curthr());
    rt_task_yield();
}


/*  unblocks the  given Thread   */
inline void threadunblock(ThreadDesc * t)
{
    rt_task_resume(&t->task);
	t->state = STATE_RUNNABLE;
}


/* blocks the current Thread */
inline void threadblock(void)
{
    rt_task_suspend(&(curthr()->task));
    curthr()->state = STATE_BLOCKEDUSER;
}


void reschedule(void)
{
    rt_task_yield();
}


void threadsInit(void)
{
    int result;

	printk(KERN_INFO "Initialize threads system.\n");

    if ((result = rt_task_add_hook(T_HOOK_SWITCH, jemTaskHook)) < 0) {
        printk(KERN_ERR "Error registering Jem task switch hook, rc=%d\n", result);
    }

    if ((result = rt_task_add_hook(T_HOOK_START, jemTaskHook)) < 0) {
        printk(KERN_ERR "Error registering Jem task start hook, rc=%d\n", result);
    }
}


void thread_prepare_to_copy(void)
{
	ThreadDesc *cur = curthr();
	cur->max_copied = curdom()->scratchMemSize / sizeof(struct copied_s);
	cur->copied = (struct copied_s *) curdom()->scratchMem;	
	curthr()->n_copied = 0;
}


