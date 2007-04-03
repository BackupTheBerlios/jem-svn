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
//==============================================================================
// File: domain.h
//
// Jem/JVM domain interface.
//
//===========================================================================

#ifndef _DOMAIN_H
#define _DOMAIN_H

#include <linux/types.h>
#include <native/mutex.h>
#include <native/sem.h>
#include "jemConfig.h"
#include "object.h"

#define DOMAIN_TERMINATED_EXCEPTION  (-1)
#define DOMAIN_STATE_FREE        0
#define DOMAIN_STATE_CREATING    1
#define DOMAIN_STATE_ACTIVE      2
#define DOMAIN_STATE_TERMINATING 3
#define DOMAIN_STATE_FREEZING    4
#define DOMAIN_STATE_FROZEN      5
#define DOMAIN_STATE_THAWING     6
#define DOMAIN_STATE_ZOMBIE      7
#define SERVICE_ENTRY_FREE     ((DEPDesc*)0x00000000)
#define SERVICE_ENTRY_CHANGING ((DEPDesc*)0xffffffff)
#define MAGIC_DOMAIN 0xd0d0eeee


typedef struct InstanceCounts_s {
	jint objbytes;
	jint arrbytes;
	jint portalbytes;
	jint memproxybytes;
	jint cpustatebytes;
	jint atomvarbytes;
	jint servicebytes;
	jint servicepoolbytes;
	jint casbytes;
	jint tcbbytes;
	jint stackbytes;
} InstanceCounts;


typedef struct GCDescUntypedMemory_s {
	u32 dummy[30];
} GCDescUntypedMemory_t;


typedef void (*HandleObject_t) (struct DomainDesc_s * domain,
				struct ObjectDesc_s *obj, u32 objSize,
				u32 flags);
typedef u32 *(*HandleReference_t) (struct DomainDesc_s *, struct ObjectDesc_s **);


typedef struct GCDesc_s {
    GCDescUntypedMemory_t   untypedMemory;
    struct ObjectDesc_s     *gcObject;
    struct ThreadDesc_s     *gcThread;
    code_t                  gcCode;
    void                    *data;
    struct ObjectDesc_s     **registeredObjects; 
    u64                     gcTime;
    u32                     gcRuns;
    u32                     gcBytesCollected;
    jlong                   memTime;
    u32                     epoch;
    jboolean                active;
    struct ThreadDesc_s     *gcSuspended;
    RT_MUTEX                gcLock;
    ObjectHandle (*allocDataInDomain) (struct DomainDesc_s * domain,
                                      int objsize, u32 flags);
    void (*done) (struct DomainDesc_s * domain);
    void (*gc) (struct DomainDesc_s * domain);
    void (*finalizeMemory) (struct DomainDesc_s * domain);
    void (*finalizePortals) (struct DomainDesc_s * domain);
    jboolean (*isInHeap) (struct DomainDesc_s * domain,
                         struct ObjectDesc_s * obj);
    jboolean (*ensureInHeap) (struct DomainDesc_s * domain,
                             struct ObjectDesc_s * obj);
    void (*walkHeap) (struct DomainDesc_s * domain,
                      HandleObject_t handleObject,
                      HandleObject_t handleArray,
                      HandleObject_t handlePortal,
                      HandleObject_t handleMemory,
                      HandleObject_t handleService,
                      HandleObject_t handleCAS,
                      HandleObject_t handleAtomVar,
                      HandleObject_t handleDomainProxy,
                      HandleObject_t handleCPUStateProxy,
                      HandleObject_t handleServicePool,
                      HandleObject_t handleStackProxy);
    u32 (*freeWords) (struct DomainDesc_s * domain);
    u32 (*totalWords) (struct DomainDesc_s * domain);
    void (*printInfo) (struct DomainDesc_s * domain);
    void (*setMark)(struct DomainDesc_s *domain);
    struct ObjectDesc_s * (*atMark)(struct DomainDesc_s *domain);
} GCDesc;

typedef struct SchedDescUntypedMemory_s {
	u64 dummy[5];
} SchedDescUntypedMemory_t;

typedef struct GlobalSchedDescUntypedMemory_s {
	u64 dummy[5+20];
} GlobalSchedDescUntypedMemory_t;

typedef struct SchedDesc_s {
	GlobalSchedDescUntypedMemory_t  untypedGlobalMemory;
	SchedDescUntypedMemory_t        untypedLocalMemory;
	int                             currentThreadID;
	u64                             (*becomesRunnable) (struct DomainDesc_s *domain);
} SchedDesc;

typedef struct DomainDesc_s {
	u32                         magic;
	jint                        maxNumberOfLibs;
	jint                        numberOfLibs;
	struct LibDesc_s             **libs;		/* code used by this domain */
	struct LibDesc_s             **ndx_libs;	/* system width libindex-list */
	jint                        **sfields;
	struct ClassDesc_s           *arrayClasses;
	char                        *codeBorder[CONFIG_JEM_CODE_FRAGMENTS];	/* pointer to border of code segment (last allocated word  + 1) */
	char                        *code[CONFIG_JEM_CODE_FRAGMENTS];	/* all code lifes here */
	char                        *codeTop[CONFIG_JEM_CODE_FRAGMENTS];	/* pointer to free code space */
	s32                         cur_code;		/* current code chunk */
	s32                         code_bytes;	/* max code memory  */
	char                        *domainName;
	GCDesc                      gc;
	jint                        irqHandler_interruptHandlerIndex;
	int                         advancingThreads;
	u32                         preempted;
	u64                         cputime;
    u32                         currentThreadID;
    struct CPUDesc_s            *cpu[CONFIG_NR_CPUS];	/* CPU Object(s) of this domain */
	struct ThreadDesc_s         *threads;
	struct ObjectDesc_s         *startClassName;
	struct ObjectDesc_s         *dcodeName;
	struct ArrayDesc_s          *libNames;
	struct ArrayDesc_s          *argv;
	struct ArrayDesc_s          *initialPortals;
	struct DEPDesc_s            *services[CONFIG_JEM_MAX_SERVICES];
	struct ServiceThreadPool_s  *pools[CONFIG_JEM_MAX_SERVICES];
    RT_SEM                      serviceSem;
	int                         state;
	u32                         id;
 	struct ThreadDesc_s         *firstThreadInRunQueue;
 	struct ThreadDesc_s         *lastThreadInRunQueue;
 	struct DomainDesc_s         *nextInRunQueue;
	struct ThreadDesc_s         *outboundInterceptorThread;
	code_t                      outboundInterceptorCode;
	struct ObjectDesc_s         *outboundInterceptorObject;
	struct ThreadDesc_s         *inboundInterceptorThread;
	code_t                      inboundInterceptorCode;
	struct ObjectDesc_s         *inboundInterceptorObject;
	struct ThreadDesc_s         *portalInterceptorThread;
	code_t                      createPortalInterceptorCode;
	code_t                      destroyPortalInterceptorCode;
	struct ObjectDesc_s         *portalInterceptorObject;
	jboolean                    memberOfTCB;
	u32                         libMemSize;
	struct ObjectDesc_s         *naming;	/* the global naming portal used inside this domain */
	u32                         memoryObjectBytes;	/* memory allocated as memory objects */
	u32                         memusage;		/* total memory usage counted in jxmalloc */
	struct Proxy_s              *initialNamingProxy;	/* Naming proxy that can be obtained by calling InitialNaming.getInitialNaming() */
	char                        *scratchMem;	/* memory to store temporary data; use only when no other domain activity (example use: portal parameter copy) */
	u32                         scratchMemSize;	/* size of scratch memory */
	u32                         inhibitGCFlag;	/* 1==no GC allowed; 0==GC allowed */
	u32                         portal_statistics_copyout_rcv;
	u32                         portal_statistics_copyin_rcv;
    RT_MUTEX                    domainMemLock;
    RT_MUTEX                    domainHeapLock;
} DomainDesc;


typedef void (*domain_f) (struct DomainDesc_s *);
typedef void (*domain1_f) (struct DomainDesc_s *, void *);


void initDomainSystem(void);
void deleteDomainSystem(void);
struct DomainDesc_s *createDomain(char *domainName, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4,
			 u32 code_bytes, int gcImpl, struct ArrayDesc_s *schedinfo);
jint getNumberOfDomains(void);
void domain_panic(struct DomainDesc_s * domain, char *msg, ...);
void foreachDomain(domain_f func);
void foreachDomain1(domain1_f func, void *arg);
void foreachDomainRUNQ(domain_f func);
char **malloc_tmp_stringtable(struct DomainDesc_s * domain, struct TempMemory_s * mem,
			      u32 number);
int findMethodAtAddrInDomain(struct DomainDesc_s * domain, u8 * addr,
			     struct MethodDesc_s ** method, struct ClassDesc_s ** classInfo,
			     jint * bytecodePos, jint * lineNumber);


#endif

