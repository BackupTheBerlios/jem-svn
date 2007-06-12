// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
//==============================================================================

#ifndef ZERO_H
#define ZERO_H

#include "load.h"
#include "thread.h"
#include "zero_Memory.h"

// TEMP TEMP TEMP
#define NR_IRQS 15

extern jint gc_methodindex;

extern ObjectDesc *memoryManagerInstance;
extern DEPDesc *ihandlers[NR_IRQS];
extern DomainDesc *idomains[CONFIG_NR_CPUS][NR_IRQS];
extern ThreadDesc *ithreads[CONFIG_NR_CPUS][NR_IRQS];
extern u32 imissed[NR_IRQS];
extern u32 idelayed[NR_IRQS];
extern u32 iprocessed[CONFIG_NR_CPUS][NR_IRQS];
extern ObjectDesc *ifirstlevel_object[CONFIG_NR_CPUS][NR_IRQS];
extern u32 ifirstlevel_happened[CONFIG_NR_CPUS][NR_IRQS];
extern u32 ifirstlevel_processed[CONFIG_NR_CPUS][NR_IRQS];

struct nameValue_s {
    Proxy *obj;
    char *name;
    struct nameValue_s *next;
};
extern struct nameValue_s *nameValue;

extern code_t extern_panic;

/* CPUState */

static inline ObjectDesc *thread2CPUState(ThreadDesc * thread)
{
    if (thread == NULL)
        return NULL;
    return(ObjectDesc *) (((u32 *) thread) - 1);
}

static inline ThreadDesc *cpuState2thread(ObjectDesc * obj)
{
    if (obj == NULL)
        return NULL;
    if ((getObjFlags(obj) & FLAGS_MASK) == OBJFLAGS_CPUSTATE) {
        return &(((ThreadDescProxy *)obj)->desc);
    } else if ((getObjFlags(obj) & FLAGS_MASK) == OBJFLAGS_FOREIGN_CPUSTATE) {
        return findThreadDesc((ThreadDescForeignProxy*)obj);
    }
    return NULL;
}


/**
 * Class -> jx/zero/VMClass (Object)
 */
static inline ObjectDesc *class2Obj(JClass * cl)
{
    return(ObjectDesc *) & (cl->objectDesc_vtable);
}

/**
 * jx/zero/VMClass (Object) -> Class
 */
static inline JClass *obj2class(ObjectDesc * obj)
{
    return(JClass *) (((u32 *) obj) - 1 - XMOFF);
}

/**
 * Method -> jx/zero/VMMethod (Object)
 */
static inline ObjectDesc *method2Obj(MethodDesc * m)
{
    return(ObjectDesc *) & (m->objectDesc_vtable);
}

/**
 * jx/zero/VMMethod (Object) -> Method
 */
static inline MethodDesc *obj2method(ObjectDesc * obj)
{
    return(MethodDesc *) (((u32 *) obj) - 1 - XMOFF);
}



/*************/
/* functions */

void            init_irq_data(void);
void            init_zero_from_lib(DomainDesc * domain, SharedLibDesc * zeroLib);
ObjectDesc      *init_zero_dep_without_thread(char *ifname, char *depname, MethodInfoDesc * methods, jint size, char *subname);
ObjectDesc      *init_zero_dep(char *ifname, char *depname, MethodInfoDesc * methods, jint size, char *subname);
ClassDesc       *createObjectClassDesc(void);
JClass          *createObjectClass(ClassDesc * java_lang_Object);
void            createArrayObjectVTableProto(DomainDesc * domain);
ClassDesc       *init_zero_class(char *ifname, MethodInfoDesc * methods,
                           jint size, jint instanceSize, jbyte * typeMap,
                           char *subname);
void            installObjectVtable(ClassDesc * c);
void            installInitialNaming(DomainDesc * srcDomain, DomainDesc * dstDomain,
                          Proxy * naming);
Proxy           *getInitialNaming(void);
ObjectDesc      *bootfs_getFile(ObjectDesc * self, ObjectDesc * filename);
void            registerPortal(DomainDesc * domain, ObjectDesc * dep, char *name);
Proxy           *lookupPortal(char *name);
void            start_domain_zero(void *arg);
void            atomicvariable_atomicUpdateUnblock(AtomicVariableProxy * self, ObjectDesc * value,
                                                   CPUStateProxy * cpuStateProxy);
jint            cpuManager_dump(ObjectDesc * self, ObjectDesc * msg, ObjectDesc * ref);
jint            cpuManager_receive(ObjectDesc * self, ObjectDesc * obj);
DomainProxy     *domainManager_createDomain(ObjectDesc * self,
                                        ObjectDesc * dname,
                                        ArrayDesc * cpuObjs,
                                        ArrayDesc * HLSNames,
                                        ObjectDesc * dcodeName,
                                        ArrayDesc * libsName,
                                        ObjectDesc * startClassName,
                                        jint gcinfo0, jint gcinfo1, jint gcinfo2, ObjectDesc* gcinfo3, jint gcinfo4,
                                        jint codeSize,
                                        ArrayDesc * argv,
                                        ObjectDesc * naming,
                                        ArrayDesc * portals,
                                        jint gcImpl,
                                        ArrayDesc *schedinfo);


#endif              /* ZERO_H */
