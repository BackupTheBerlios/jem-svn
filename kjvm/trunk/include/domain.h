// Additional Copyrights:
//  Copyright (C) 1997-2001 The JX Group.
//==============================================================================

#ifndef _DOMAIN_H
#define _DOMAIN_H

#include <linux/types.h>
#include "object.h"
// @aspect include

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
    // @aspect struct GCDesc_s
} GCDesc;


typedef struct DomainDesc_s {
    u32                         magic;
    jint                        maxNumberOfLibs;
    jint                        numberOfLibs;
    struct LibDesc_s             **libs;        /* code used by this domain */
    struct LibDesc_s             **ndx_libs;    /* system width libindex-list */
    jint                        **sfields;
    struct ClassDesc_s           *arrayClasses;
    char                        **codeBorder; /* pointer to border of code segment (last allocated word  + 1) */
    char                        **code;   /* all code lifes here */
    char                        **codeTop;    /* pointer to free code space */
    s32                         cur_code;       /* current code chunk */
    s32                         code_bytes; /* max code memory  */
    char                        *domainName;
    GCDesc                      gc;
    jint                        irqHandler_interruptHandlerIndex;
    int                         advancingThreads;
    u32                         preempted;
    u64                         cputime;
    u32                         currentThreadID;
    struct ThreadDesc_s         *threads;
    struct ObjectDesc_s         *startClassName;
    struct ObjectDesc_s         *dcodeName;
    struct ArrayDesc_s          *libNames;
    struct ArrayDesc_s          *argv;
    struct ArrayDesc_s          *initialPortals;
    struct DEPDesc_s            **services;
    struct ServiceThreadPool_s  **pools;
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
    struct ObjectDesc_s         *naming;    /* the global naming portal used inside this domain */
    u32                         memoryObjectBytes;  /* memory allocated as memory objects */
    u32                         memusage;       /* total memory usage counted in jxmalloc */
    struct Proxy_s              *initialNamingProxy;    /* Naming proxy that can be obtained by calling InitialNaming.getInitialNaming() */
    char                        *scratchMem;    /* memory to store temporary data; use only when no other domain activity (example use: portal parameter copy) */
    u32                         scratchMemSize; /* size of scratch memory */
    u32                         inhibitGCFlag;  /* 1==no GC allowed; 0==GC allowed */
    u32                         portal_statistics_copyout_rcv;
    u32                         portal_statistics_copyin_rcv;
    // @aspect struct DomainDesc_s
} DomainDesc;


typedef void (*domain_f) (struct DomainDesc_s *);
typedef void (*domain1_f) (struct DomainDesc_s *, void *);


int initDomainSystem(void);
void deleteDomainSystem(void);
struct DomainDesc_s *createDomain(char *domainName, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4,
                                  u32 code_bytes, int gcImpl);
jint getNumberOfDomains(void);
void domain_panic(struct DomainDesc_s * domain, char *msg);
void foreachDomain(domain_f func);
char **malloc_tmp_stringtable(struct DomainDesc_s * domain, struct TempMemory_s * mem,
                              u32 number);
//int findClassForMethod(MethodDesc * method, JClass **jclass);
int findMethodAtAddrInDomain(struct DomainDesc_s * domain, u8 * addr,
                             struct MethodDesc_s ** method, struct ClassDesc_s ** classInfo,
                             jint * bytecodePos, jint * lineNumber);
int findMethodAtAddr(u8 * addr, struct MethodDesc_s ** method, struct ClassDesc_s ** classInfo,
                     jint * bytecodePos, jint * lineNumber);
int findProxyCode(struct DomainDesc_s * domain, char *addr, char **method, char **sig, struct ClassDesc_s ** classInfo);
struct DomainDesc_s *findDomain(u32 id);
struct DomainDesc_s *findDomainByName(char *name);
void terminateDomain(struct DomainDesc_s * domain);


#endif

