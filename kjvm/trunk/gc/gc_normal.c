//==============================================================================
// gc_normal.c
// 
// Jem/JVM Copying garbage collector implementation
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#ifdef CONFIG_JEM_ENABLE_GC 

#include "gc_normal.h"
#include "gc_impl.h"
#include "gc_pgc.h"
#include "gc_move_common.h"


typedef struct gc_normal_mem_s {
	gc_move_common_mem_t    move_common;
	jint                    *heapBorder;	/* pointer to border of heap (last allocated word  + 1) */
	jint                    *heap;		    /* all objects life here */
	jint                    *heapFreePtr;	/* start of allocated (not aligned) memory */
	jint                    *heapSize;		/* heap size */
	jint                    *heapTop;		/* pointer to free heap space */
	jint                    *heapBorder2;	/* pointer to border of heap (last allocated word  + 1) */
	jint                    *heap2;		    /* all objects life here */
	jint                    *heap2FreePtr;	/* start of allocated (not aligned) memory */
	jint                    *heapTop2;		/* pointer to free heap space */
	jint                    *mark;
} gc_normal_mem_t;

#define GCM_NEW(domain) (*(gc_normal_mem_t*)(&domain->gc.untypedMemory))

/* align heap at page borders to use mprotect for debugging the GC */
#define HEAP_BLOCKSIZE            4096
#define HEAP_BLOCKADDR_N_NULLBITS 12
#define HEAP_BLOCKADDR_MASK       0xfffff000

ObjectHandle(*registerObject) (DomainDesc * domain, ObjectDesc * o);
extern unsigned char callnative_special_end[], callnative_special_portal_end[], callnative_static_end[], thread_exit_end[];
void return_from_java0(ThreadDesc * next, ContextDesc * restore);
void return_from_java1(long param, ContextDesc * restore, ThreadDesc * next);
void return_from_java2(long param1, long param2, ThreadDesc * next, ContextDesc * restore);
extern unsigned char return_from_javaX_end[], never_return_end[];
void never_return(void);
jint cpuManager_receive(ObjectDesc * self, Proxy * portal);
extern unsigned char cpuManager_receive_end[];
void profile_sample_heapusage_alloc(DomainDesc * domain, u32 objSize);
extern Proxy *initialNamingProxy;

ObjectHandle gc_normal_allocDataInDomain(DomainDesc * domain, int objSize, u32 flags)
{
	ObjectDesc *obj;
	jint *nextObj;
	jint *data;
	ObjectHandle handle;
	jboolean tried = JNI_FALSE;

#ifdef CONFIG_JEM_PROFILE
	jlong memTimeNext;
	memTimeNext = ((domain->gc.memTime / memTimeStep) + 1) * memTimeStep;
	domain->gc.memTime += sizeof(jint) * objSize;
	if (domain->domainName != NULL && (strcmp(domain->domainName, "DomainZero") == 0))
		memTimeNext = domain->gc.memTime + 10;
#endif

	GC_LOCK;

  try_alloc:
	nextObj = GCM_NEW(domain).heapTop + objSize;
	if ((nextObj > GCM_NEW(domain).heapBorder - HEAP_RESERVE)
#ifdef CONFIG_JEM_PROFILE
	    || (domain->gc.memTime > memTimeNext)
#endif
	    ) {
#ifdef CONFIG_JEM_PROFILE
		gc_dprintf("\nDomain %p (%s) reached memtime %lld (%lld). Starting GC...\n", domain, domain->domainName,
			   memTimeNext, domain->gc.memTime);
#endif

#if defined(KERNEL) || defined(JAVASCHEDULER)
		if (curthr()->isInterruptHandlerThread) {
			if (nextObj > GCM_NEW(domain).heapBorder) {
				sys_panic("no GC in interrupt handler possible!");
			}
#ifdef ENABLE_GC
			goto do_alloc;	/* we are in the interrupt handler but have still enough heap reserve */
#else
			sys_panic("GC is disabled");
#endif
		}
		/* not in interrupt handler -> GC possible */
#endif				/* KERNEL || JAVASCHEDULER */

#ifndef ENABLE_GC
		sys_panic("Attempt to GC with GC disabled");
#endif

#ifdef CHECK_HEAP_BEFORE_GC
		gc_checkHeap(domain, JNI_FALSE);
#endif				/* CHECK_HEAP_BEFORE_GC */

#ifdef VERBOSE_GC
		printf("\nDomain %p (%s) consumed %d bytes of heap space. Starting GC...\n", domain, domain->domainName,
		       (char *) GCM_NEW(domain).heapTop - (char *) GCM_NEW(domain).heap);
#endif
#ifdef PROFILE_AGING
		paGCStart(domain, memTimeNext);
#endif
		/*GC_UNLOCK; */
#ifdef ENABLE_GC
		if (tried) {
			printf("GC did not free enough memory. need %d bytes", objSize << 2);
			printStackTraceNew("TERMINATE THREAD");
			exceptionHandler(THROW_RuntimeException);
			//sys_panic("GC did not free enough memory. need %d bytes", objSize << 2);
			//domain_panic(curdom(),"GC did not free enough memory. need %d bytes", objSize << 2);
		}
		if (domain->gc.gcThread == NULL)
			domain_panic(curdom(), "GC but no GC thread availabke");
		start_thread_using_code1(domain->gc.gcObject, domain->gc.gcThread, domain->gc.gcCode, (u32) domain);
#endif

		//executeInterface(domain, "jx/zero/GarbageCollector", "gc", "()V", domain->gcProxy, 0, 0);
		/*GC_LOCK; */

#ifdef VERBOSE_GC
		printf("    Live bytes: %d\n", (char *) GCM_NEW(domain).heapTop - (char *) GCM_NEW(domain).heap);
#endif
#ifdef PROFILE_AGING
		// prevent gc from running to often while profiling aging
		memTimeNext = domain->gc.memTime + 1;
#endif
		tried = JNI_TRUE;
		goto try_alloc;
	}
#if defined(KERNEL) || defined(JAVASCHEDULER)
      do_alloc:
#endif

#ifdef PROFILE_AGING
	paNew(domain, objSize, ptr2ObjectDesc(GCM_NEW(domain).heapTop));
#endif

#ifdef PROFILE_SAMPLE_HEAPUSAGE
	profile_sample_heapusage_alloc(domain objSize);
#endif				/* PROFILE_SAMPLE_HEAPUSGAE */

	data = (jint *) GCM_NEW(domain).heapTop;
	GCM_NEW(domain).heapTop = nextObj;

	ASSERT(data != NULL);
	memset(data, 0, objSize * 4);

	obj = ptr2ObjectDesc(data);
	setObjFlags(obj, flags);
#ifdef USE_QMAGIC
	setObjMagic(obj, MAGIC_OBJECT);
#endif

	handle = registerObject(domain, obj);

#ifdef CHECK_HEAP_AFTER_ALLOC
	{
		// last object on heap is not properly initialized (no vtable)
		u32 *tmpptr = GCM_NEW(domain).heapTop;
		GCM_NEW(domain).heapTop -= objSize;
		gc_normal_checkHeap(domain);
		GCM_NEW(domain).heapTop = tmpptr;
	}
#endif				/* CHECK_HEAP_AFTER_ALLOC */

	GC_UNLOCK;
	return handle;
}

void gc_normal_walkHeap(DomainDesc * domain, HandleObject_t handleObject, HandleObject_t handleArray, HandleObject_t handlePortal,
		     HandleObject_t handleMemory, HandleObject_t handleService, HandleObject_t handleCAS,
		     HandleObject_t handleAtomVar, HandleObject_t handleDomainProxy, HandleObject_t handleCPUStateProxy,
		     HandleObject_t handleServicePool, HandleObject_t handleStackProxy)
{
	gc_walkContinuesBlock(domain, GCM_NEW(domain).heap, (u32 **) & GCM_NEW(domain).heapTop, handleObject, handleArray,
			      handlePortal, handleMemory, handleService, handleCAS, handleAtomVar, handleDomainProxy,
			      handleCPUStateProxy, handleServicePool, handleStackProxy);
}

static void gc_normal_walkHeap2(DomainDesc * domain, HandleObject_t handleObject, HandleObject_t handleArray,
			     HandleObject_t handlePortal, HandleObject_t handleMemory, HandleObject_t handleService,
			     HandleObject_t handleCAS, HandleObject_t handleAtomVar, HandleObject_t handleDomainProxy,
			     HandleObject_t handleCPUStateProxy, HandleObject_t handleServicePool,
			     HandleObject_t handleStackProxy)
{
	gc_walkContinuesBlock(domain, GCM_NEW(domain).heap2, (u32 **) & GCM_NEW(domain).heapTop2, handleObject, handleArray,
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
		ClassDesc *cl = obj2ClassDesc(obj);

		printf("%p\n", obj);
		printf("heap1:%p..%p\n", GCM_NEW(domain).heap, GCM_NEW(domain).heapTop);
		printf("heap2: %p..%p\n", GCM_NEW(domain).heap2, GCM_NEW(domain).heapTop2);
		printf("%s\n", cl->name);
		printf("OBJECT OUTSIDE HEAP");
		//sys_panic("OBJECT OUTSIDE HEAP");
		return 0;
	}
	return 1;
}

static inline u32 *gc_normal_allocHeap2(DomainDesc * domain, u32 size)
{
	u32 *data;

	data = GCM_NEW(domain).heapTop2;
	if (data > (u32 *) GCM_NEW(domain).heapBorder2) {
		sys_panic("target heap too small????");
	}
	GCM_NEW(domain).heapTop2 += size;

	IF_DBG_GC(printf("target heap size: %ld bytes\n", (u32) GCM_NEW(domain).heapTop2 - (u32) GCM_NEW(domain).heap2));

	return data;
}





// FIXME #undef DBG_GC

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
static void gc_normal_scan_heap2_ForeignCPUState(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	gc_impl_walkContentForeignCPUState(domain, (ThreadDescForeignProxy *) obj, gc_common_move_reference);
}
static void gc_normal_scan_heap2_ServicePool(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
#ifdef STACK_ON_HEAP
	gc_impl_walkContentServicePool(domain, obj, gc_common_move_reference);
#endif
}
static void gc_normal_scan_heap2_Stack(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
#ifdef STACK_ON_HEAP
	gc_impl_walkContentStack(domain, obj, gc_common_move_reference);
#endif
}

void gc_normal_done(DomainDesc * domain)
{
	u32 size;

	/* free heap */
	//  printf("free heap %d\n", GCM_NEW(domain).heapSize);

#ifdef MPROTECT_HEAP
	if (mprotect(GCM_NEW(domain).heapFreePtr, GCM_NEW(domain).heapSize, PROT_READ | PROT_WRITE) == -1) {
		perror("unprotecting  heap semispace");
		sys_panic("");
	}

	jxfree(GCM_NEW(domain).heapFreePtr, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
	GCM_NEW(domain).heap = GCM_NEW(domain).heapBorder = GCM_NEW(domain).heapTop = NULL;

	if (GCM_NEW(domain).heap2) {
		if (mprotect
		    (GCM_NEW(domain).heap2, (char *) (GCM_NEW(domain).heapBorder2) - (char *) (GCM_NEW(domain).heap2),
		     PROT_READ | PROT_WRITE) == -1) {
			perror("unprotecting  heap semispace");
			sys_panic("");
		}
		jxfree(GCM_NEW(domain).heap2FreePtr, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
		GCM_NEW(domain).heap2 = GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heapTop2 = NULL;
	}
#else
	jxfree(GCM_NEW(domain).heap, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
	GCM_NEW(domain).heap = GCM_NEW(domain).heapBorder = GCM_NEW(domain).heapTop = NULL;

	if (GCM_NEW(domain).heap2) {
		jxfree(GCM_NEW(domain).heap2, GCM_NEW(domain).heapSize MEMTYPE_HEAP);
		GCM_NEW(domain).heap2 = GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heapTop2 = NULL;
	}
#endif				/* MPROTECT_HEAP */

}


static void gc_normal_finalizeMemoryCB(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	//MemoryProxy *memobj = (MemoryProxy*)obj;

	if ((flags & FORWARD_MASK) == GC_FORWARD) {
		//printf("LIVEMEM: %p %ld refcount=%ld %p %p \n", memobj->mem, memobj->size, memobj->dz->refcount, obj, ObjectDesc2ptr(obj)));
	} else {
		//printf("DEADMEM: %p %ld refcount=%ld\n", memobj->mem, memobj->size, memobj->dz->refcount);
		ASSERTMEMORY(obj);
		memory_deleted((struct MemoryProxy_s *) obj);
	}
}

static void gc_normal_finalizePortalsCB(DomainDesc * domain, ObjectDesc * obj, u32 objSize, u32 flags)
{
	Proxy *p = (Proxy *) obj;
	if (!((flags & FORWARD_MASK) == GC_FORWARD)) {
		// decrement service refcount
		ASSERTCLI;
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
	//  printf("FINALIZE MEM\n");
	gc_normal_walkHeap(domain, NULL, NULL, NULL, gc_normal_finalizeMemoryCB, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}


/*
 * Finalize all garbage portals
 */
void gc_normal_finalizePortals(DomainDesc * domain)
{
	//  printf("FINALIZE PORTALS\n");
	gc_normal_walkHeap(domain, NULL, NULL, gc_normal_finalizePortalsCB, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}



void gc_normal_checkHeap(DomainDesc * domain)
{
	gc_normal_walkHeap(domain, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

// FIXME
void gc_normal_init1(DomainDesc * domain)
{
#ifndef KERNEL
#ifdef MPROTECT_HEAP
	if (mprotect
	    (GCM_NEW(domain).heap2, (char *) (GCM_NEW(domain).heapBorder2) - (char *) (GCM_NEW(domain).heap2),
	     PROT_READ | PROT_WRITE) == -1) {
		perror("unprotecting  new heap semispace");
		sys_panic("");
	}
#endif				/* MPROTECT_HEAP */
#endif
}

void gc_normal_gc(DomainDesc * domain)
{

#ifdef VERBOSE_GC
	printf("GARBAGE COLLECTOR started for domain %p (%s)\n", domain, domain->domainName);
#endif

#ifdef DELAY_ALLOC_SECOND_HEAP
	/* alloc second heap */
	if (GCM_NEW(domain).heap2 == NULL) {
		u32 heapSize = GCM_NEW(domain).heapSize;
		char *start;
#ifdef USE_MPROTECT
		GCM_NEW(domain).heap2 =
		    (jint *) jxmalloc_align(heapSize >> BLOCKADDR_N_NULLBITS, HEAP_BLOCKSIZE, &start MEMTYPE_HEAP);
#else
		GCM_NEW(domain).heap2 = (jint *) jxmalloc(heapSize MEMTYPE_HEAP);
#endif
		GCM_NEW(domain).heap2FreePtr = start;
		GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heap2 + (heapSize >> 2);
		GCM_NEW(domain).heapTop2 = GCM_NEW(domain).heap2;
	}
#endif				/* DELAY_ALLOC_SECOND_HEAP */

	/*  printf("HEAPS %p - %p (%p), %p - %p (%p)\n", 
	   GCM_NEW(domain).heap, GCM_NEW(domain).heapTop, GCM_NEW(domain).heapBorder,
	   GCM_NEW(domain).heap2, GCM_NEW(domain).heapTop2, GCM_NEW(domain).heapBorder2); */

#if 0
#ifdef KERNEL
	DISABLE_IRQ;
#else
#ifdef DEBUG
	{
		sigset_t set, oldset;
		sigemptyset(&set);
		sigprocmask(SIG_BLOCK, &set, &oldset);
		if (!sigismember(&oldset, SIGALRM))
			sys_panic("signal must be blocked during GC");

	}
#endif	 /*DEBUG*/
#endif
#endif
#ifdef PROFILE_GC
	    pgcNewRun(domain);
#endif
	PGCB(GC);

	gc_normal_init1(domain);
	/* 
	 * Init
	 */
#ifdef CHECK_HEAPUSAGE
# ifdef DBG_GC
	/* check whether heap is consistent */
	printf("Checking Heap...");
	if (gc_checkHeap(domain, JNI_FALSE) == JNI_TRUE)
		printf("OK\n");
	else
		printf("FAILED\n");
# else
	gc_checkHeap(domain, JNI_FALSE);
# endif
#endif				/*CHECK_HEAPUSAGE */

	freezeThreads(domain);
	IF_DBG_GC( {
		  printf("    Test map...\n"); walkStacks(domain, NULL);	// FIXME
		  }
	);

	//print_memobj(domain->id);
	/*
	 * Move directly reachable objects onto new heap 
	 */
	walkRootSet(domain, gc_common_move_reference, gc_common_move_reference, gc_common_move_reference,
		    gc_common_move_reference, gc_common_move_reference, gc_common_move_reference);

	/*
	 * All directly reachable objects are now on the new heap
	 * Scan new heap 
	 */
	IF_DBG_GC(printf("Scanning new heap ...\n"));
	PGCB(HEAP);
	gc_normal_walkHeap2(domain, gc_normal_scan_heap2_Object, gc_normal_scan_heap2_Array, NULL, NULL, gc_normal_scan_heap2_Service, NULL,
			 gc_normal_scan_heap2_AtomVar, NULL, gc_normal_scan_heap2_CPUState, gc_normal_scan_heap2_ServicePool,
			 gc_normal_scan_heap2_Stack);
#if 0
	gc_normal_walkHeap2(domain, gc_normal_scan_heap3_Object, gc_normal_scan_heap3_Array, NULL, NULL, gc_normal_scan_heap3_Service, NULL,
			 gc_normal_scan_heap3_AtomVar);
	gc_normal_walkHeap2(domain, gc_normal_scan_heap2_Object, gc_normal_scan_heap2_Array, NULL, NULL, gc_normal_scan_heap2_Service, NULL,
			 gc_normal_scan_heap2_AtomVar);
	gc_normal_walkHeap2(domain, gc_normal_scan_heap2_Object, gc_normal_scan_heap2_Array, NULL, NULL, gc_normal_scan_heap2_Service, NULL,
			 gc_normal_scan_heap2_AtomVar);
	gc_normal_walkHeap2(domain, gc_normal_scan_heap2_Object, gc_normal_scan_heap2_Array, NULL, NULL, gc_normal_scan_heap2_Service, NULL,
			 gc_normal_scan_heap2_AtomVar);
#endif

	PGCE(HEAP);

	//gc_normal_scan_for_garbage(domain);
	/*
	 * Finish
	 */
#ifdef CHECK_HEAP_BEFORE_GC
	gc_checkHeap(domain, JNI_FALSE);
	printf("HEAP OK\n");
#endif				/* CHECK_HEAP_BEFORE_GC */

	IF_DBG_GC(printf("Finalize memory objects ...\n"));
	gc_normal_finalizeMemory(domain);
	gc_normal_finalizePortals(domain);


	IF_DBG_GC(printf("Invalidating all objects of old heap"));
#ifdef CHECK_HEAPUSAGE
	gc_checkHeap(domain, JNI_TRUE);	/* invalidate all objects */
#endif
	{
		jint *htmp = GCM_NEW(domain).heap;
		GCM_NEW(domain).heap = GCM_NEW(domain).heap2;
		GCM_NEW(domain).heap2 = htmp;

		htmp = GCM_NEW(domain).heapBorder;
		GCM_NEW(domain).heapBorder = GCM_NEW(domain).heapBorder2;
		GCM_NEW(domain).heapBorder2 = htmp;

		GCM_NEW(domain).heapTop = GCM_NEW(domain).heapTop2;
		GCM_NEW(domain).heapTop2 = GCM_NEW(domain).heap2;	/* heap2 is empty */

	}
	PGCE(SCAN);
#ifndef KERNEL

#ifdef CHECK_HEAPUSAGE
	memset(GCM_NEW(domain).heap2, 0xff, (GCM_NEW(domain).heapBorder2 - GCM_NEW(domain).heap2) * 4);
#endif
	IF_DBG_GC(printf("Protecting %p..%p\n", GCM_NEW(domain).heap2, GCM_NEW(domain).heapBorder2));
	PGCB(PROTECT);
	//printf("Protecting %p..%p\n", GCM_NEW(domain).heap2, GCM_NEW(domain).heapBorder2);

#ifdef MPROTECT_HEAP
	if (mprotect(GCM_NEW(domain).heap2, (char *) (GCM_NEW(domain).heapBorder2) - (char *) (GCM_NEW(domain).heap2), PROT_NONE)
	    == -1) {
		perror("protecting old heap semispace");
		sys_panic("");
	}
#endif				/* MPROTECT_HEAP */

	PGCE(PROTECT);
#endif

#ifdef DEBUG
#ifdef DBG_GC
	printf("Checking new heap\n");
	gc_normal_checkHeap(domain);
	walkStacks(domain, NULL);	// FIXME
#endif

#ifdef VERBOSE_GC
#ifdef PROFILE_HEAPUSAGE
	printHeapUsage(domain);
#endif
#endif

#endif				/* DEBUG */
	PGCE(GC);

#ifdef VERBOSE_GC
	printf("GC finished\n");
#endif				/* VERBOSE_GC */
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
	printf("     heap(used ): %p...%p (current=%p)\n", GCM_NEW(domain).heap, GCM_NEW(domain).heapBorder,
	       GCM_NEW(domain).heapTop);
	printf("     heap(other): %p...%p (current=%p)\n", GCM_NEW(domain).heap2, GCM_NEW(domain).heapBorder2,
	       GCM_NEW(domain).heapTop2);
	printf("     total: %ld, used: %ld, free: %ld\n", gc_totalWords(domain) * 4,
	       (gc_totalWords(domain) - gc_freeWords(domain)) * 4, (gc_freeWords(domain)) * 4);
}

#ifdef NEW_COPY
void gc_normal_setMark(struct DomainDesc_s *domain)
{
	GCM_NEW(domain).mark = GCM_NEW(domain).heapTop;
}


ObjectDesc *gc_normal_atMark(struct DomainDesc_s *domain)
{
	u32 *data = GCM_NEW(domain).mark;
	ObjectDesc *obj;
	jint flags;
	u32 objSize = 0;
	ClassDesc *c;

	if (data == NULL || data >= GCM_NEW(domain).heapTop) {
		GCM_NEW(domain).mark = NULL;
		return NULL;
	}

	ASSERT(data <= GCM_NEW(domain).heapTop && data >= GCM_NEW(domain).heap);

	obj = ptr2ObjectDesc(data);
	flags = getObjFlags(obj);
	switch (flags & FLAGS_MASK) {
	case OBJFLAGS_ARRAY:
		objSize = gc_objSize2(obj, flags);
		break;
	case OBJFLAGS_OBJECT:
		c = obj2ClassDesc(obj);
		ASSERTCLASSDESC(c);
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
		printf("OBJ=%p, FLAGS: %lx mark=%p, heap=%p heaptop=%p\n", obj, flags & FLAGS_MASK, GCM_NEW(domain).mark,
		       GCM_NEW(domain).heap, GCM_NEW(domain).heapTop);
#ifdef DEBUG
		flags = getObjFlags(obj);
		if ((flags & FORWARD_MASK) == GC_FORWARD) {
			printf("Forward to %p\n", flags & FORWARD_PTR_MASK);
			flags = getObjFlags((ObjectDesc *) (flags & FORWARD_PTR_MASK));
		}
		dump_data(obj);
#endif
		sys_panic("WRONG HEAP DATA");
	}
	GCM_NEW(domain).mark += objSize;
	return obj;
}
#endif

void gc_normal_init(DomainDesc * domain, u32 heap_bytes)
{
	u32 heapSize;
	u32 *start;

	ASSERT(sizeof(GCDescUntypedMemory_t) >= sizeof(gc_normal_mem_t));

	if (heap_bytes == 0)
		sys_panic("gc_new is not suitable for domain Zero");

	if (heap_bytes <= HEAP_RESERVE + 1000)
		sys_panic("heap too small. need at least %d bytes ", HEAP_RESERVE);

	/* alloc heap mem */
#ifdef MPROTECT_HEAP
	heapSize = (heap_bytes + HEAP_BLOCKSIZE - 1) & HEAP_BLOCKADDR_MASK;
	if (HEAP_BLOCKSIZE % BLOCKSIZE != 0)
		sys_panic("heapalign must be multiple of blocksize");
	GCM_NEW(domain).heap = (jint *) jxmalloc_align(heapSize >> BLOCKADDR_N_NULLBITS, HEAP_BLOCKSIZE, &start MEMTYPE_HEAP);
#else
	heapSize = heap_bytes;
	GCM_NEW(domain).heap = (jint *) jxmalloc(heap_bytes MEMTYPE_HEAP);
#endif


#ifdef MPROTECT_HEAP
	GCM_NEW(domain).heapFreePtr = start;
#endif
	GCM_NEW(domain).heapSize = heapSize;
	GCM_NEW(domain).heapBorder = GCM_NEW(domain).heap + (heap_bytes >> 2);
	GCM_NEW(domain).heapTop = GCM_NEW(domain).heap;


	//printf("domain=%ld Heapsize=%d reserve=%d\n", domain->id, (GCM_NEW(domain).heapBorder - GCM_NEW(domain).heapTop)*4, HEAP_RESERVE);

	if ((GCM_NEW(domain).heapBorder - GCM_NEW(domain).heapTop) * 4 < HEAP_RESERVE) {
		printf("domain=%ld Heapsize=%d reserve=%d\n", domain->id,
		       (GCM_NEW(domain).heapBorder - GCM_NEW(domain).heapTop) * 4, HEAP_RESERVE);
		sys_panic("HEAP TOO SMALL");
	}
#ifndef DELAY_ALLOC_SECOND_HEAP
#ifdef MPROTECT_HEAP
	GCM_NEW(domain).heap2 = (jint *) jxmalloc_align(heapSize >> BLOCKADDR_N_NULLBITS, HEAP_BLOCKSIZE, &start MEMTYPE_HEAP);
	GCM_NEW(domain).heap2FreePtr = start;
#else
	GCM_NEW(domain).heap2 = (jint *) jxmalloc(heap_bytes MEMTYPE_HEAP);
#endif
	GCM_NEW(domain).heapBorder2 = GCM_NEW(domain).heap2 + (heap_bytes >> 2);
	GCM_NEW(domain).heapTop2 = GCM_NEW(domain).heap2;
#endif				/* DELAY_ALLOC_SECOND_HEAP */


#ifndef GC_USE_ONLY_ONE
	GCM_MOVE_COMMON(domain).allocHeap2 = gc_normal_allocHeap2;
	GCM_MOVE_COMMON(domain).walkHeap2 = gc_normal_walkHeap2;
	domain->gc.allocDataInDomain = gc_normal_allocDataInDomain;
	domain->gc.gc = gc_normal_gc;
	domain->gc.done = gc_normal_done;
	domain->gc.freeWords = gc_normal_freeWords;
	domain->gc.totalWords = gc_normal_totalWords;
	domain->gc.printInfo = gc_normal_printInfo;
	domain->gc.finalizeMemory = gc_normal_finalizeMemory;
	domain->gc.finalizePortals = gc_normal_finalizePortals;
	domain->gc.isInHeap = gc_normal_isInHeap;
	domain->gc.ensureInHeap = gc_normal_ensureInHeap;
	domain->gc.walkHeap = gc_normal_walkHeap;
#ifdef NEW_COPY
	domain->gc.setMark = gc_normal_setMark;
	domain->gc.atMark = gc_normal_atMark;
#endif
#endif				/* GC_USE_ONLY_ONE */
}
#endif			




//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
// Copyright (C) 1998-2002 Michael Golm
// Copyright (C) 2001-2002 Joerg Baumann
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

