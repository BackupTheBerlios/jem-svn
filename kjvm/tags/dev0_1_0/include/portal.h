// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
//==============================================================================

#ifndef _PORTAL_H
#define _PORTAL_H
// @aspect include

#define PORTAL_RETURN_TYPE_NUMERIC   0
#define PORTAL_RETURN_TYPE_REFERENCE 1
#define PORTAL_RETURN_TYPE_EXCEPTION 3

typedef jint(*dep_f) (jint * paramlist);


typedef struct MethodInfoDesc_s {
    char        *name;
    char        *signature;
    code_t      code;
} MethodInfoDesc;

typedef struct DEPTypeDesc_s {
    char        *type;
    jint        numMethods;
    char        **methods;
    code_t      code;
} DEPTypeDesc;

typedef struct ServiceThreadPool_s {
    u32                 magic;
    u32                 flags;
    u32                 refcount;
    struct ThreadDesc_s *firstReceiver;
    struct ThreadDesc_s *firstWaitingSender;
    struct ThreadDesc_s *lastWaitingSender;
    u32                 index;
    // @aspect struct ServiceThreadPool_s
} ServiceThreadPool;

typedef struct DEPDesc_s {
    u32                         magic;
    u32                         flags;
    struct ServiceThreadPool_s  *pool;
    volatile u32                lock;
    struct DomainDesc_s         *domain;
    struct ObjectDesc_s         *obj;
    struct Proxy_s              *proxy;
    struct ClassDesc_s          *interface;
    volatile u32                valid;
    volatile u32                refcount;
    volatile u32                serviceIndex;
    volatile u32                abortFlag;
    u32                         statistics_no_receiver;
    u32                         statistics_handoff;
} DEPDesc;


#define MAGIC_DEP 0xbabeface

typedef struct Proxy_s {
    code_t              *vtable;
    struct DomainDesc_s *targetDomain;
    u32                 targetDomainID;
    u32                 index;
} Proxy;

typedef struct CPUStateProxy_s {
    code_t              *vtable;
    struct ThreadDesc_s *cpuState;
} CPUStateProxy;

typedef struct AtomicVariableProxy_s {
    code_t              *vtable;
    struct ObjectDesc_s *value;
    struct ThreadDesc_s *blockedThread;
    int                 listMode;
} AtomicVariableProxy;

typedef struct CASProxy_s {
    code_t  *vtable;
    u32     index;
} CASProxy;

typedef struct VMObjectProxy_s {
    code_t              *vtable;
    struct DomainDesc_s *domain;
    u32                 domain_id;
    u32                 epoch;
    int                 type;
    struct ObjectDesc_s *obj;
    int                 subObjectIndex;
} VMObjectProxy;

typedef struct CredentialProxy_s {
    code_t              *vtable;
    u32                 signerDomainID;
    struct ObjectDesc_s *value;
} CredentialProxy;

typedef struct DomainProxy_s {
    code_t              *vtable;
    struct DomainDesc_s *domain;
    u32                 domainID;
} DomainProxy;

typedef struct InterceptOutboundInfo_s {
    struct DomainDesc_s      *source;
    struct DomainDesc_s      *target;
    struct MethodDesc_S      *method;
    struct ObjectDesc_s      *obj;
    struct ArrayDesc_s       *paramlist;
} InterceptOutboundInfo;

typedef struct InterceptInboundInfoProxy_s {
    code_t               *vtable;
    struct DomainDesc_s  *source;
    struct DomainDesc_s  *target;
    struct ObjectDesc_s  *method;
    struct ObjectDesc_s  *obj;
    jint                 *paramlist;
    int                  index;
} InterceptInboundInfoProxy;

typedef struct InterceptPortalInfoProxy_s {
    code_t              *vtable;
    struct DomainDesc_s *domain;
    u32                 index;
} InterceptPortalInfoProxy;


void                service_decRefcount(DomainDesc * domain, u32 index);
void                service_incRefcount(DEPDesc * p);
u32                 createService(DomainDesc * domain, ObjectDesc * depObj, ClassDesc * interface,
                              ServiceThreadPool * pool);
void                installVtables(DomainDesc * domain, ClassDesc * c, MethodInfoDesc * methods,
                               int numMethods, ClassDesc * cl);
void                receive_portalcall(u32 poolIndex);
int                 findProxyCodeInDomain(DomainDesc * domain, char *addr, char **method, char **sig,
                                      ClassDesc ** classInfo);
void                addToRefTable(ObjectDesc * src, ObjectDesc * dst);
void                reinit_service_thread(void);
Proxy               *portal_auto_promo(DomainDesc * domain, ObjectDesc * obj);
void                abstract_method_error(ObjectDesc * self);
void                portals_init(void);
void                portal_abort_current_call(DEPDesc * dep, struct ThreadDesc_s * sender);
void                portal_remove_sender(DEPDesc * dep, struct ThreadDesc_s * sender);
struct ObjectDesc_s *copy_reference(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota);

#endif
