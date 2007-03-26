//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
// Copyright © 2001-2002 Meik Felser. All rights reserved.
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


void        idle(void *x);
ThreadDesc  *createThreadInMem(DomainDesc * domain, thread_start_t thread_start, void *param,
                               ObjectDesc * e, u32 stackSize, int state, int schedParam, char *tName);
ThreadDesc  *specialAllocThreadDesc(DomainDesc * domain);

void        start_initial_thread(void *dummy);
static void destroyCurrentThread(void);

void thread_exit()
{
	destroyCurrentThread();
	printk(KERN_ERR "Looping forever. Should never be reached.\n");
	for (;;);
}


ThreadDesc *createInitialDomainThread(DomainDesc * domain, int state, int schedParam)
{
	return createThreadInMem(domain, start_initial_thread, NULL, NULL, 1024, state, schedParam);
}


ThreadDesc *createThread(DomainDesc * domain, thread_start_t thread_start, void *param, int state, int schedParam)
{
	ThreadDesc *t = createThreadInMem(domain, thread_start, param, NULL, 1024, state, schedParam);
	return t;
}

ThreadDesc *createThreadInMem(DomainDesc * domain, thread_start_t thread_start, void *param, 
                              ObjectDesc * entry, u32 stackSize, int state, int prio, char *tName)
{
	u32 *sp1;
	ThreadDesc *thread;
	ObjectDesc *obj;
	ThreadDescProxy *threadproxy;

	threadproxy             = allocThreadDescProxyInDomain(domain, cpuStateClass);
	thread                  = &(threadproxy->desc);
	thread->entry           = entry;
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

    rt_task_spawn(&thread->task, tName, stackSize, prio, T_FPU, thread_start, (void *) thread);

	return thread;
}

void freeThreadMem(ThreadDesc * t)
{
	DomainDesc *domain;

	domain = t->domain;
	if (t->portalParams)
		jemFree(t->portalParams, PORTALPARAMS_SIZE MEMTYPE_OTHER);
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
		printf("new GC epoch -> find TCB\n");
		for (t = domain->threads; t != NULL; t = t->nextInDomain) {
			if (t->id == proxy->threadID) {
				/* found thread -> update proxy and return TCB */
				proxy->thread = t;
				proxy->gcEpoch = domain->gc.epoch;
				printf("new GC epoch -> found %p\n", t);
				return t;
			}
		}
		/* thread not found (probably terminated) -> return NULL */
		return NULL;
	}
	return proxy->thread;	// TODO: CHECK DOMAIN AND GC EPOCH
}

#ifdef SMP
static spinlock_t lock = SPIN_LOCK_UNLOCKED;
#endif

static void destroyCurrentThread()
{
	ThreadDesc **t;
	DomainDesc *domain;

	DISABLE_IRQ;

	domain = curdom();

	/* should call terminateThread(curthr()) */

	curthr()->state = STATE_ZOMBIE;

	terminateThread_internal(curthr());

	/* check if there are any other threads or services or other connections from the outside into this domain 
	 * otherwise: domain is no longer useful and can be terminated (garbage collected) */
	/*if (! Sched_threadsExist(curdom())) {
	   domain->state = DOMAIN_STATE_ZOMBIE;
	   } */
#ifndef NEW_SCHED
	Sched_destroy_switch_to_nextThread(domain);
#else
	Sched_destroyed_current(domain);
#endif

	RESTORE_IRQ;		/* dummy */
	sys_panic("switched back to DEAD thread");
}

/* thread must not be in any queue !! caller of terminateThread must ensure this! */
void terminateThread(ThreadDesc * thread)
{
	DomainDesc *domain;

	DISABLE_IRQ;

#ifdef SMP
	spin_lock(&lock);
#endif

	terminateThread_internal(thread);


	RESTORE_IRQ;
}

/* used for bot current and other thread */
void terminateThread_internal(ThreadDesc * thread)
{
	ASSERTCLI;

	/* remove thread from domain thread list */
	if (thread->prevInDomain) {
		thread->prevInDomain->nextInDomain = thread->nextInDomain;
	} else {
		thread->domain->threads = thread->nextInDomain;
	}
	if (thread->nextInDomain) {
		thread->nextInDomain->prevInDomain = thread->prevInDomain;
	}
#ifdef SMP
	spin_unlock(&lock);
#endif
	Sched_destroyed(thread);

	freeThreadMem(thread);

}

void print_esp()
{
	int a;
	printf("ESP=%p\n", &a);
}

void setThreadName(ThreadDesc * thread, char *n1, char *n2)
{
	int l;
	ASSERTCLI;
	l = strlen(n1);
	strncpy(thread->name, n1, THREAD_NAME_MAXLEN);
	if (l >= THREAD_NAME_MAXLEN || n2 == NULL)
		return;
	strncpy(thread->name + l, n2, THREAD_NAME_MAXLEN - l);
}


/*******/
void threadyield()
{
	ThreadDesc *next;

	DISABLE_IRQ;

	ASSERT(curthr()->state == STATE_RUNNABLE);

#ifdef KERNEL
#ifdef CHECK_SERIAL_IN_YIELD
	{
		static int less_checks;
		if ((less_checks++ % 20) == 0)
			check_serial();
	}
#endif
#endif

	//printf ("YIELD %d.%d of Domain %s\n",TID(curthr()), curthr()->domain->domainName);
#ifndef NEW_SCHED
	Sched_yielded(curthr());
	Sched_switch_to_nextThread();
#else
	Sched_yield();
#endif
	//printf ("back from YIELD %d.%d (Domain %s)\n",TID(curthr()), curthr()->domain->domainName);

	RESTORE_IRQ;
}

/*  unblocks the  given Thread 
  ! be sure the IRQs are disabled  */
inline void threadunblock(ThreadDesc * t)
{
	ASSERTTHREAD(t);
	ASSERTCLI;
	t->state = STATE_RUNNABLE;
#ifndef NEW_SCHED
	Sched_unblocked(t);
#else
	Sched_unblock(t);
#endif
}

/* unblocks the  given Thread
   but deactivates the Interrupts at first */
void locked_threadunblock(ThreadDesc * t)
{
#ifdef KERNEL
	DISABLE_IRQ;
	threadunblock(t);
	RESTORE_IRQ;
#else
	threadunblock(t);
	//    sys_panic("unblock should not be called");
#endif
}

/* blocks the current Thread */
inline void threadblock()
{
	//ASSERTCLI;
	DISABLE_IRQ;

	SCHED_BLOCK_USER;


	RESTORE_IRQ;
}

/* 
   blocks the current Thread
   but deactivates the Interrupts at first
 */
void locked_threadblock()
{
#ifdef KERNEL
	DISABLE_IRQ;
	threadblock();
	RESTORE_IRQ;
#else
	sys_panic("block should not be called");
#endif
}

void ports_outb_p(ObjectDesc * self, jint port, jbyte value);
jbyte ports_inb_p(ObjectDesc * self, jint port);

static void rtc_irq_ack()
{
	ports_outb_p(NULL, 0x70, 0xc);
	ports_inb_p(NULL, 0x71);
}

void irq_happened(u32 irq, DEPDesc * dep /*u32 eip, u32 cs, u32 eflags */ )
{
	/* printf("*****IRQ****** %p %p %p\n", eip, cs, eflags); */
	/* printf("*****IRQ %d %p******\n", irq, dep); */
	/*rtc_irq_ack(); */
}
void irq_missed(u32 irq, DEPDesc * dep)
{
	/* printf("*****IRQ****** %p %p %p\n", eip, cs, eflags); */
	printf("*****IRQMISSED %ld %p******\n", irq, dep);
	printStackTrace("IRQMISS: ", curthr(), (u32 *) & irq - 2);

	monitor(NULL);
}

void irq_picnotok(jint mask, jint irq, jint andmask)
{
	printf("*****PIC NOT OK: mask=0x%08lx  andmask=0x%08lx irq=%ld******\n", mask, andmask, irq);
	monitor(NULL);
}

void irq_irrnotok(jint irq)
{
	printf("*****IRR NOT OK: irq=%ld******\n", irq);
	monitor(NULL);
}

#if 0
/* send message from lowlevel irq handler to handler thread */
ThreadDesc *irq_send_msg(DEPDesc * dep)
{
	ThreadDesc *target;
	ASSERTDEP(dep);
	/*printf("SEND IRQ MESSAGE\n"); */
#ifdef NO_DEP_LOCKING
	spinlock(&dep->lock);
#endif
#ifdef DBG_IRQ
	printf("irq_send_msg 0x%lx\n", dep);
#endif
	if ((target = dep->firstWaitingReceiver) != NULL) {
		dep->firstWaitingReceiver = dep->firstWaitingReceiver->nextInDEPQueue;
		if (dep->firstWaitingReceiver == NULL) {
			dep->lastWaitingReceiver = NULL;
		}
#ifdef DBG_IRQ
		printf("irq_sm: 0x%lx  ip=%p eflags=%p recv_msg=%p\n", target, target->context[PCB_EIP],
		       target->context[PCB_EFLAGS], recv_msg);
#endif

#ifdef PROFILE_EVENT_THREADSWITCH
		target->name = "IRQT";
		profile_event_threadswitch_to(target);
#endif

		return target;
	}
	sys_panic("delayed irq handling not yet supported\n");
	return NULL;
}
#endif


/* do not need thread */
void receiveDomainDEP(void *arg)
{
	thread_exit();
}


/*****/


int disable_threadswitching = 0;


/*
 * Reschedule in interrupt context 
 */
#ifdef JAVASCHEDULER
void reschedule()
{
//     sim_timer_irq();
	sys_panic("JAVASCHEDULER defined: reschedule not implemented");
}
#else
void reschedule()
{
	ThreadDesc *next;

	if (disable_threadswitching) {
		destroy_switch_to(curthrP(), curthr());
	} else {
#ifndef NEW_SCHED
		Sched_reschedule();	/* if this fkt returns: there was no other Thread */
#else
		Sched_preempted();	/* if this fkt returns: there was no other Thread */
#endif
		//dprintf("No next thread to schedule.\n");
		destroy_switch_to(curthrP(), curthr());
		sys_panic("reschedule1: should not be reached");
	}
}
#endif				/* JAVASCHEDULER */

void softint_handler()
{
	printf("SOFTINTHANDLER\n");
	reschedule();
}

void irq_handler_new()
{
#ifdef NOPREEMPT
#ifdef DEBUG
	if (nopreempt_check(curthr()->context[PCB_EIP])) {
		//      printf(" interrupted in %p\n", curthr()->context[PCB_EIP]);
	}
#endif
#endif
#ifdef ROLLFORWARD_ON_PREEMPTION
	if (nopreempt_check(curthr()->context[PCB_EIP])) {
		curthr()->context[PCB_EIP] = nopreempt_adjust(curthr()->context[PCB_EIP]);
		printf(" forward to eip=0x%08lx\n", curthr()->context[PCB_EIP]);
#ifdef KERNEL
		cli in curthr->context
#else
		sigaddset(&(curthr()->sigmask), SIGALRM);
#endif
		destroy_switch_to(curthrP(), curthr());
		//reschedule();
		sys_panic("should not be reached");
	}
#endif				/* ROLLFORWARD_ON_PREEMPTION */


	reschedule();
	sys_panic("should not be reached");
}

#ifdef KERNEL
void save(struct irqcontext *sc)
{
#else
void save(struct sigcontext *sc)
{
#endif

	ThreadDesc *thread = curthr();


	thread->context[PCB_GS] = sc->gs;
	thread->context[PCB_ES] = sc->es;
	thread->context[PCB_FS] = sc->fs;

	thread->context[PCB_EDI] = sc->edi;
	thread->context[PCB_ESI] = sc->esi;
	thread->context[PCB_EBP] = sc->ebp;
	thread->context[PCB_ESP] = sc->esp;
	thread->context[PCB_EBX] = sc->ebx;
	thread->context[PCB_EDX] = sc->edx;
	thread->context[PCB_ECX] = sc->ecx;
	thread->context[PCB_EAX] = sc->eax;
	thread->context[PCB_EIP] = sc->eip;
	thread->context[PCB_EFLAGS] = sc->eflags;
#ifndef KERNEL
	thread->preempted = 1;
	sigdelset(&thread->sigmask, SIGALRM);
#endif
}

#ifdef KERNEL
void save_timer(struct irqcontext_timer *sc)
{
	ThreadDesc *thread = curthr();

	thread->context[PCB_GS] = sc->gs;
	thread->context[PCB_ES] = sc->es;
	thread->context[PCB_FS] = sc->fs;

	thread->context[PCB_EDI] = sc->edi;
	thread->context[PCB_ESI] = sc->esi;
	thread->context[PCB_EBP] = sc->ebp;
	thread->context[PCB_ESP] = sc->esp;
	thread->context[PCB_EBX] = sc->ebx;
	thread->context[PCB_EDX] = sc->edx;
	thread->context[PCB_ECX] = sc->ecx;
	thread->context[PCB_EAX] = sc->eax;
	thread->context[PCB_EIP] = sc->eip;
	thread->context[PCB_EFLAGS] = sc->eflags;
}
#endif




void threads_init()
{
	printf("init threads system\n");

	threads_profile_init();

#ifndef NEW_SCHED
	runq_init();
#else
	sched_init();
#endif
	/* create idle thread. Do NOT insert it into runqueue */
	__idle_thread[0] = createThread(domainZero, idle, (void *) 0, STATE_RUNNABLE, SCHED_CREATETHREAD_NORUNQ);
	setThreadName(idle_thread, "Idle", NULL);

#ifndef KERNEL
	threads_emulation_init();
#endif
	printf("init threads system completed\n");
}

#ifdef SMP
void smp_idle_threads_init()
{
	int i;

#ifdef DEBUG			// current thread is not consistent, so we have to deactivate the check
	check_current = 0;
#endif
	/* create idle Threads */
	for (i = 1; i < num_processors_online; i++)
		if (__idle_thread[online_cpu_ID[i]] == NULL)
			__idle_thread[online_cpu_ID[i]] = createThread(domainZero, idle, (void *) 0);
#ifdef DEBUG
	check_current = 1;
#endif

#ifdef JAVASCHEDULER
	smp_scheduler_init();
#endif
}
#endif

#ifdef DEBUG
int check_current = 1;

#ifdef DEBUG
char *emergency_stack[1024];
#endif

ThreadDesc *curthr()
{
	int cpuID = get_processor_id();
#if 0
#ifdef CHECK_CURRENT
	u32 *sp;
	ASSERTTHREAD(__current[cpuID]);
	if (check_current) {
		sp = (u32 *) & cpuID;
		if (sp <= __current[cpuID]->stack || sp >= __current[cpuID]->stackTop) {
			cli();

			printf("CURRENT NOT CONSISTENT!!!\n");
			printf("THREAD: %p\n", __current[cpuID]);
			printf("ESP: %p\n", &sp + 1);
			printNStackTrace("CUR: ", __current[cpuID], (u32 *) & cpuID + 1, 7);
			sys_panic("current not consistent %p.", __current[cpuID]);
		}
		check_not_in_runq(__current[cpuID]);
		checkStackTrace(__current[cpuID], (u32 *) & cpuID + 1);
	}
#endif
#endif				/* 0 */

	ASSERTDOMAIN(__current[cpuID]->domain);

	return __current[cpuID];
}

ThreadDesc **curthrP()
{
	int cpuID = get_processor_id();
#ifdef CHECK_CURRENT
	u32 *sp;
	if (check_current) {
		sp = (u32 *) & cpuID;
		ASSERTTHREAD(__current[cpuID]);
		if (sp <= __current[cpuID]->stack || sp >= __current[cpuID]->stackTop) {
			printStackTrace("CURP: ", __current[cpuID], (u32 *) & cpuID + 1);
			sys_panic("current not consistent %p.", __current[cpuID]);
		}
		check_not_in_runq(__current[cpuID]);
		checkStackTrace(__current[cpuID], (u32 *) & cpuID + 1);
	}
#endif
	return &__current[cpuID];
}

DomainDesc *curdom()
{
	int cpuID = get_processor_id();
#ifdef CHECK_CURRENT
	ASSERTTHREAD(__current[cpuID]);
	if (check_current) {
		check_not_in_runq(__current[cpuID]);
	}
#endif
	if (__current[cpuID] == NULL)
		return NULL;
	return __current[cpuID]->domain;
}

void set_current(ThreadDesc * c)
{
	int cpuID = get_processor_id();
#ifdef CHECK_CURRENT
	if (check_current) {
		check_not_in_runq(c);
	}
#endif
	__current[cpuID] = c;
	ASSERTTHREAD(__current[cpuID]);
}

#endif

jint switch_to(ThreadDesc ** current, ThreadDesc * to)
{
	u32 result;

	DISABLE_IRQ;

	//printf("switch from %d.%d TO %d.%d\n", TID(*current), TID(to));
	//if (to->id ==9) printStackTraceNew("TO");
	ASSERTTHREAD(to);
	ASSERT(current != NULL);
	ASSERTTHREAD((*current));
	//ASSERT(curthr() == idle_thread || curthr()->state != STATE_RUNNABLE); /* because curthr is NOT added to runq */  

	result = internal_switch_to(current, to);

	RESTORE_IRQ;

	return result;
}

void thread_prepare_to_copy()
{
	ThreadDesc *cur = curthr();
	ASSERTCLI;
	cur->max_copied = curdom()->scratchMemSize / sizeof(struct copied_s);
	//cur->max_copied = 500;
	cur->copied = curdom()->scratchMem;	//jxmalloc(sizeof(struct copied_s)*cur->max_copied);
	curthr()->n_copied = 0;
}

#ifdef CPU_USAGE_STATISTICS
static u8_t lasttime = 0;
void profile_cputime()
{
	u8_t t, diff;
	t = get_tsc();
	if (lasttime == 0) {
		lasttime = t;
	}
	diff = t - lasttime;
	curdom()->cputime += diff;
	curthr()->cputime += diff;
	lasttime = t;
}
#endif				/* CPU_USAGE_STATISTICS */


