//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 Christopher Stone. 
// Copyright (C) 1997-2001 The JX Group.
// Copyright (C) 1998-2002 Michael Golm.
// Copyright (C) 2001-2002 Meik Felser. 
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
// Portal invocation and data copy between domains
//=================================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "zero_Memory.h"
#include "zero.h"
#include "gc_alloc.h"
#include "gc_impl.h"
#include "sched.h"
#include "exception_handler.h"
#include "vmsupport.h"


extern ClassDesc    *portalInterface;

static char         *copy_content_reference_internal(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota);
static char         *copy_content_object(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota);
static ObjectDesc   *copy_shallow_portal(DomainDesc * src, DomainDesc * dst, Proxy * obj, u32 * quota, jboolean addRef);
static u32          direct_send_portal(Proxy * proxy, ...);
static ObjectDesc   *copy_shallow_domainproxy(DomainDesc * src, DomainDesc * dst, DomainProxy * obj, u32 * quota);
static void         connectServiceToPool(DEPDesc * svc, ServiceThreadPool * pool);
static void         disconnectServiceFromPool(DEPDesc * svc);
static ObjectDesc   *copy_shallow_object(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota);
static void         receive_dep(void *arg);
static ObjectDesc   *copy_shallow_reference_internal(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota);

/* returns index into domains service table */
u32 createService(DomainDesc * domain, ObjectDesc * depObj, ClassDesc * interface, ServiceThreadPool * pool)
{
    DEPDesc *dep;
    u32 index;

    jem_mutex_acquire();
    for (index = 0; index < CONFIG_JEM_MAX_SERVICES; index++) {
        if (domain->services[index] == SERVICE_ENTRY_FREE) {
            domain->services[index] = SERVICE_ENTRY_CHANGING;
            break;
        }
    }
    jem_mutex_release();

    if (index == CONFIG_JEM_MAX_SERVICES) {
        printk(KERN_ERR "domain can not create more services\n");
        return 0;
    }

    dep = allocServiceDescInDomain(domain);
    dep->domain = domain;
    dep->obj = depObj;
    dep->interface = interface;
    if (pool)
        connectServiceToPool(dep, pool);
    else
        dep->pool = NULL;
    dep->flags          = DEPFLAG_NONE;
    dep->valid          = 1;
    dep->serviceIndex   = index;
    printk(KERN_INFO "Created DEP 0x%lx in domain 0x%lx\n", (long unsigned int) dep, (long unsigned int) domain);
    domain->services[index] = dep;
    return index;
}

static ThreadDesc *createServiceThread(DomainDesc * domain, int poolIndex, char *name)
{
    ServiceThreadPool *pool = domain->pools[poolIndex];
    ThreadDesc *thread = createThread(domain, receive_dep, (void *) poolIndex, STATE_RUNNABLE,
                                      getJVMConfig()->defaultServicePrio, "SVCPool");
    thread->nextInReceiveQueue  = pool->firstReceiver;
    pool->firstReceiver         = thread;
    return thread;
}


void abstract_method_error(ObjectDesc * self)
{
    ClassDesc *oclass;
    printk(KERN_ERR "ABSTRACT METHOD CALLED at object %p\n", self);
    printk(KERN_ERR "** PLEASE RECOMPILE ALL CLASSES **\n");
    oclass = obj2ClassDesc(self);
    printk(KERN_ERR "  ClassName: %s\n", oclass->name);

    exceptionHandlerInternal("Abstract method called.");
}

static void abstract_method_error_proxy(void)
{
    printk(KERN_ERR "THIS PROXY METHOD IS ABSTRACT\n");
    exceptionHandlerInternal("Abstract method called.");
}


static Proxy *createPortalInDomain(DomainDesc * domain, ClassDesc * depClass, DomainDesc * targetDomain, u32 targetDomainID,
                u32 depIndex)
{
    code_t *vtable;
    jint j;
    Proxy *proxy;

    if (depClass->proxyVtable == NULL) {
        vtable = jemMallocVtable(domain, depClass->vtableSize + 1);
        vtable[0] = (code_t) depClass;
        vtable++;
        for (j = 0; j < depClass->vtableSize; j++) {
            if (depClass->vtableSym[j * 3][0] == '\0') {
                /* hole */
                vtable[j] = (code_t) abstract_method_error_proxy;
            } else {
                vtable[j] = (code_t) direct_send_portal;
            }
        }
        depClass->proxyVtable = vtable;
    }

    proxy = allocProxyInDomain(domain, depClass, targetDomain, targetDomainID, depIndex);
    return proxy;
}


/* cl==NULL means create table for java_lang_Object */
/* methods==NULL and numMethods==0 means create call to exception_handler */
void installVtables(DomainDesc * domain, ClassDesc * c, MethodInfoDesc * methods, int numMethods, ClassDesc * cl)
{
    int j, k;
    int failure = 0;

    for (j = 0; j < c->vtableSize; j++) {
        c->vtable[j] = (code_t) abstract_method_error;
        c->methodVtable[j] = NULL;
    }

    installObjectVtable(c);

    if (methods == NULL && numMethods == 0)
        return;

    for (j = 0; j < c->vtableSize; j++) {
        if (c->vtableSym[j * 3][0] == '\0')
            continue;   /* hole */
        for (k = 0; k < numMethods; k++) {
            if (strcmp(methods[k].name, c->vtableSym[j * 3 + 1]) == 0) {
                c->vtable[j] = methods[k].code;
                if (cl != NULL) {
                    c->methodVtable[j] = jemMallocMethoddesc(domain);
                    memset(c->methodVtable[j], 0, sizeof(MethodDesc));
                    c->methodVtable[j]->name = cl->methodVtable[j]->name;
                    c->methodVtable[j]->signature = cl->methodVtable[j]->signature;
                    c->methodVtable[j]->numberOfArgs = cl->methodVtable[j]->numberOfArgs;
                    c->methodVtable[j]->argTypeMap = cl->methodVtable[j]->argTypeMap;
                    c->methodVtable[j]->returnType = cl->methodVtable[j]->returnType;
                }
                break;
            }
        }
        if (c->vtable[j] == (code_t) abstract_method_error) {
            printk(KERN_ERR " NOT FOUND: class %s contains no %s %s\n", c->name, cl->methodVtable[j]->name,
                   cl->methodVtable[j]->signature);
            failure = 1;
        }
    }
    if (failure) {
        printk(KERN_ERR "Some DomainZero Portal methods are not implemented.");
    }
}





/*********
 *  Portal invocation
 ****/

static inline void portal_add_sender(DEPDesc * dep, ThreadDesc * thread)
{
    thread->nextInDEPQueue = NULL;

    //mutex_acquire(&dep->pool->poolLock, TM_INFINITE);
    if (dep->pool->lastWaitingSender == NULL) {
        dep->pool->lastWaitingSender = dep->pool->firstWaitingSender = thread;
    } else {
        dep->pool->lastWaitingSender->nextInDEPQueue = thread;
        dep->pool->lastWaitingSender = thread;
    }
    //mutex_release(&dep->pool->poolLock);
}


void portal_remove_sender(DEPDesc * dep, ThreadDesc * sender)
{
    ThreadDesc **t;
    for (t = &(dep->pool->firstWaitingSender); *t != NULL; t = &((*t)->nextInDEPQueue)) {
        if (*t == sender) {
            *t = (*t)->nextInDEPQueue;
            return;
        }
    }
    printk(KERN_ERR "SENDER NOT IN QUEUE\n");
}


void portal_abort_current_call(DEPDesc * dep, ThreadDesc * sender)
{
    dep->abortFlag = 1;
}


static inline ThreadDesc *portal_dequeue_sender(ServiceThreadPool * pool)
{
    ThreadDesc *ret = pool->firstWaitingSender;
    if (ret == NULL)
        return NULL;

    //rt_mutex_acquire(&pool->poolLock, TM_INFINITE);
    pool->firstWaitingSender = pool->firstWaitingSender->nextInDEPQueue;
    if (pool->firstWaitingSender == NULL) {
        pool->lastWaitingSender = NULL;
    }
    ret->nextInDEPQueue = NULL;
    //rt_mutex_release(&pool->poolLock);
    return ret;
}


//void receive_portalcall(ServiceThreadPool * pool)
void receive_portalcall(u32 poolIndex)
{
    ThreadDesc          *source;
    jint                methodIndex;
    ObjectDesc          *ret;
    ObjectDesc          *obj;
    code_t              code;
    ClassDesc           *targetClass;
    jint                numberArgs;
    jbyte               *argTypeMap;
    jint                returnType;
    u32                 quota;
    jint                *myparams;
    int                 i;
    DEPDesc             *svc = NULL;
    MethodDesc          *method;
    u32                 serviceIndex;
    ServiceThreadPool   *pool;
    DomainDesc          *domain = curdom();

    curthr()->isPortalThread = JNI_TRUE;

    thread_prepare_to_copy();

    //myparams = jemMalloc(getJVMConfig()->serviceParamsSz MEMTYPE_OTHER);
    curthr()->myparams = myparams;

    for (;;) {      /* receive loop */
        curthr()->processingDEP = NULL;
        curthr()->mostRecentlyCalledBy = NULL;
    /**************
     *  Wait until there is a sender.
     **************/
        pool = curdom()->pools[poolIndex];
        curthr()->state = STATE_PORTAL_WAIT_FOR_SND;
        //sem_p(&(curdom()->serviceSem), TM_INFINITE);
        source = portal_dequeue_sender(pool);
        curthr()->state = STATE_RUNNABLE;

    /**************
     *  There is a waiting sender.
     **************/
        source->state   = STATE_PORTAL_WAIT_FOR_PARAMCOPY;
        serviceIndex    = source->blockedInServiceIndex;
        svc             = curdom()->services[serviceIndex];

        quota = getJVMConfig()->receivePortalQuota; /*  4 kB portal parameter quota , new quota for each new call */
        curthr()->n_copied = 0;

        obj                             = svc->obj;
        curthr()->processingDEP         = svc;
        curthr()->mostRecentlyCalledBy  = source;
        source->blockedInServiceThread  = curthr();
        methodIndex                     = source->depMethodIndex;
        code                            = (code_t) obj->vtable[methodIndex];

        /* Copy parameters from caller domain */
        targetClass = obj2ClassDesc(obj);
        method      = targetClass->methodVtable[methodIndex];
        numberArgs  = method->numberOfArgs;
        argTypeMap  = method->argTypeMap;
        returnType  = method->returnType;

        /* COPY IMPLICIT PORTAL PARAMETER FROM CALLER */
        if (source->portalParameter != NULL) {
          restart:
            curthr()->n_copied = 0;
            curthr()->portalParameter = copy_reference(source->domain, curdom(), source->portalParameter, &quota);
            if (curthr()->portalParameter == (struct ObjectDesc_s *) 0xffffffff) {
                source = curthr()->mostRecentlyCalledBy;
                goto restart;
            }
        }

        curthr()->myparams[0] = (jint) obj;
        for (i = 1; i < numberArgs + 1; i++) {
            ClassDesc *cl = NULL;
            if (source->depParams[i] == (jint) NULL) {
                curthr()->myparams[i] = (jint) NULL;
                continue;
            }
            if (isRef(argTypeMap, numberArgs, i - 1)) {
                if (source->depParams[i] != 0) {
                    cl = obj2ClassDesc(source->depParams[i]);

                    /* copy object to target domain */
                  restart0:
                    curthr()->n_copied = 0;
                    curthr()->myparams[i] = (jint) copy_reference(source->domain, curdom(), (ObjectDesc *)
                                              source->depParams[i], &quota);
                    if (curthr()->myparams[i] == 0xffffffff) {
                        source = curthr()->mostRecentlyCalledBy;
                        goto restart0;
                    }
                }
            } else {    /* no reference */
                curthr()->myparams[i] = source->depParams[i];
            }
        }

        source->depParams   = NULL; // do not need params any longer
        svc                 = curdom()->services[serviceIndex];
        pool                = curdom()->pools[poolIndex];

        if (!svc->valid) {
            /* service has been deactivated */
            exceptionHandler(THROW_RuntimeException);
        }

        /* remember caller domain to check if it still exists when we return from the service execution */
        curthr()->callerDomain      = source->domain;
        curthr()->callerDomainID    = curthr()->callerDomain->id;

        source->state = STATE_PORTAL_WAIT_FOR_RET;

        ret = (ObjectDesc *) callnative_special_portal(&(curthr()->myparams[1]), (ObjectDesc *) curthr()->myparams[0], code, numberArgs);   /* irqs are enabled in this function */

        /* check if caller domain was terminated while we executed the service */
        if (curthr()->callerDomainID != curthr()->callerDomain->id
            || curthr()->callerDomain->state != DOMAIN_STATE_ACTIVE) {
            /* caller disappeared, ignore all return values */
            continue;
        }

        // refresh source, because it could be moved during GC
        source = curthr()->mostRecentlyCalledBy;

        /* first check if the caller is still alive */
        if (svc->abortFlag == 1) {
            /* aborted: either caller disappeared or aborted call -> ignore all return values */
            svc->abortFlag = 0;
            continue;
        }

        // refresh svc, because it could be moved during GC
        svc                 = curdom()->services[serviceIndex];
        pool                = curdom()->pools[poolIndex];

        /* remove references to parameter objects -> allow GC to collect thgem */
        memset(myparams, 0, numberArgs * 4);

        /* Send reply to caller.
         */

        quota = getJVMConfig()->receivePortalQuota;

        /* copy return value to caller domain */
        source->state = STATE_PORTAL_WAIT_FOR_RETCOPY;
        if (returnType == 1) {
            ObjectDesc *ret0;
            if (code != (code_t) memoryManager_alloc && code != (code_t) memoryManager_allocAligned &&
                code != (code_t) bootfs_getFile) {  /* memory proxy has already been allocated in client heap */
              restart1:
                curthr()->n_copied = 0;
                ret0 = (ObjectDesc *) copy_reference(curdom(), source->domain, (ObjectDesc *)
                                 ret, &quota);
                if (ret0 == (ObjectDesc *) 0xffffffff) {    /* restart */
                    source = curthr()->mostRecentlyCalledBy;
                    goto restart1;
                }
                ret = ret0;

            }
            source->portalReturnType = 1;
        } else {
            source->portalReturnType = 0;
        }
        source->portalReturn = (ObjectDesc *) ret;

        curthr()->processingDEP = NULL;
        curthr()->mostRecentlyCalledBy = NULL;

        if (!domain->gc.isInHeap(curdom(), (ObjectDesc *) svc))
            printk(KERN_ERR "svc not in heap\n");
        if (!domain->gc.isInHeap(curdom(), (ObjectDesc *) svc->pool))
            printk(KERN_ERR "svc->pool  not in heap\n");

        // signal processing complete to waiting sender
        Sched_portal_handoff_to_sender(source);
    }           /* end of receive loop */
}

static u32 direct_send_portal(Proxy * proxy, ...)
{
    u32         methodIndex;
    jint        **paramlist;
    ClassDesc   *oclass;
    DomainDesc  *targetDomain;
    DEPDesc     *svc;

    /* MUST BE THE FIRST INSTRUCTION IN THIS FUNCTION!! */
    asm volatile ("movl %%ecx, %0":"=r" (methodIndex));

    paramlist       = (jint **) &proxy;
    proxy           = *(Proxy **) paramlist;
    targetDomain    = proxy->targetDomain;

    if (targetDomain == curdom()) {
        /* short circuit portal call inside one domain */
        /* works when method tables in both domains are identical! */
        ObjectDesc  *obj = targetDomain->services[proxy->index]->obj;
        code_t      c;
        jint        numParams;

        c = obj->vtable[methodIndex];
        if (proxy->targetDomainID != targetDomain->id) {
            exceptionHandler(THROW_RuntimeException);   /* domain was terminated */
        }
        numParams = obj2ClassDesc(obj)->methodVtable[methodIndex]->numberOfArgs;
        return callnative_special((jint *) paramlist + 1, obj, c, numParams);
    }

    if (proxy->targetDomainID != targetDomain->id) {
        /* target domain was terminated and DCB reused */
        curthr()->portalReturnType = PORTAL_RETURN_TYPE_EXCEPTION;
        goto finished;
    }

    if (targetDomain->state != DOMAIN_STATE_ACTIVE) {
        curthr()->portalReturnType = PORTAL_RETURN_TYPE_EXCEPTION;
        goto finished;
    }

    svc = targetDomain->services[proxy->index];
    if (svc == SERVICE_ENTRY_CHANGING) {
        curthr()->portalReturnType = PORTAL_RETURN_TYPE_EXCEPTION;
        goto finished;
    }

    curthr()->blockedInDomain       = targetDomain;
    curthr()->blockedInDomainID     = targetDomain->id;
    curthr()->blockedInServiceIndex = proxy->index;

    /* make parameters available to receiver  */
    curthr()->depParams         = (jint *) paramlist;
    curthr()->depMethodIndex    = methodIndex;
    oclass                      = obj2ClassDesc((ObjectDesc *) proxy);

    portal_add_sender(svc, curthr());   /* append this sender to the waiting threads */
    //rt_sem_v(&(targetDomain->serviceSem)); // signal a sender waiting to target domain

    // wait for a receiver to pick up this request and process it
    Sched_block(STATE_PORTAL_WAIT_FOR_RET);

  /********
   * Return from portal call
   ********/
  finished:
    curthr()->blockedInDomain = NULL;
    curthr()->blockedInServiceIndex = 0;
    curthr()->blockedInServiceThread = NULL;
    curthr()->depParams = NULL;

    if (curthr()->portalReturnType == PORTAL_RETURN_TYPE_EXCEPTION) {
        exceptionHandler((jint *) curthr()->portalReturn);
    } else {
        /* normal return with reference (already copied) or numeric */
        curthr()->portalReturnType = 0;
        return (jint) curthr()->portalReturn;   //ret;
    }

    return 0;  // To satisfy compiler
}


/* -1 failure,
   0 success */
int findProxyCodeInDomain(DomainDesc * domain, char *addr, char **method, char **sig, ClassDesc ** classInfo)
{
    int h, i, j;

    for (h = 0; h < domain->numberOfLibs; h++) {
        LibDesc *lib = domain->libs[h];
        JClass *allClasses = lib->allClasses;
        for (i = 0; i < lib->numberOfClasses; i++) {
            ClassDesc *classDesc = allClasses[i].classDesc;
            if (classDesc->proxyVtable == NULL)
                continue;
            for (j = 0; j < classDesc->vtableSize; j++) {
                if (classDesc->proxyVtable[j] == NULL)
                    continue;
            }
        }
    }
    return -1;
}

void addToRefTable(ObjectDesc * src, ObjectDesc * dst)
{
    ThreadDesc *c = curthr();
    if (c->n_copied == c->max_copied) {
        printk(KERN_ERR "too many objects copied");
        return;
    }
    c->copied[c->n_copied].src = src;
    c->copied[c->n_copied].flags = getObjFlags(src);
    /* set forward pointer to find all objects that are already copied */
    setObjFlags(src, (u32) dst | GC_FORWARD);
    c->n_copied++;
}

// copy either object, portal, or array

static void correctFlags(DomainDesc * domain, struct copied_s *copied, u32 n_copied)
{
    ObjectDesc *obj;
    u32 i;
    for (i = 0; i < n_copied; i++) {
        obj = copied[i].src;
        setObjFlags(obj, copied[i].flags);
    }
}
static void correctFlags2(DomainDesc * domain, struct copied_s *copied, u32 n_copied)
{
    ObjectDesc *obj;
    u32 i;
    for (i = 0; i < n_copied; i++) {
        obj = copied[i].src;
        setObjFlags(obj, copied[i].flags);
    }
}

ObjectDesc *copy_reference(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota)
{
    ObjectDesc *newobj, *ref0;

    if (src == dst)
        return ref;
    /*restart: */
    curthr()->n_copied = 0;
    dst->gc.setMark(dst);
    ref0 = copy_shallow_reference_internal(src, dst, ref, quota);
    if (ref0 == (ObjectDesc *) 0xffffffff) {
        correctFlags2(dst, curthr()->copied, curthr()->n_copied);   /* correct flags after restarting */
        goto restart;
    }

    for (;;) {
        newobj = dst->gc.atMark(dst);
        if (newobj == NULL)
            break;
        if (copy_content_reference_internal(src, dst, newobj, quota) == (char *) 0xffffffff) {
            correctFlags2(dst, curthr()->copied, curthr()->n_copied);   /* correct flags after restarting */
            goto restart;
        }
    }

    correctFlags(dst, curthr()->copied, curthr()->n_copied);
    curthr()->n_copied = 0;
    return ref0;

  restart:
    return (ObjectDesc *) 0xffffffff;
}


Proxy *portal_auto_promo(DomainDesc * domain, ObjectDesc * obj)
{
    ClassDesc           *cl;
    ClassDesc           *ifclass;
    ClassDesc           *superclass;
    Proxy               *proxy;
    u32                 i;
    u32                 depIndex;
    ServiceThreadPool   *pool;
    DEPDesc             *d = NULL;

    cl = obj2ClassDesc(obj);

    if (implements_interface(cl, portalInterface)) {
        // try to find a service description for this object
        //rt_mutex_acquire(&svTableLock, TM_INFINITE);
        for (i = 0; i < CONFIG_JEM_MAX_SERVICES; i++) {
            d = domain->services[i];
            if (d == SERVICE_ENTRY_FREE || d == SERVICE_ENTRY_CHANGING) {
                d = NULL;
                continue;
            }
            if (d->obj == obj) {
                break;
            }
        }
        //rt_mutex_release(&svTableLock);
        if (d != NULL)
            return d->proxy;


        // find portal interface
        ifclass = NULL;
        superclass = cl;
        do {
            for (i = 0; i < superclass->numberOfInterfaces; i++) {
                if (implements_interface(superclass->interfaces[i], portalInterface)) {
                    // found a portal interface
                    if (ifclass != NULL && ifclass != superclass->interfaces[i]) {
                        printk
                            (KERN_WARNING "Class %s [problem in superclass %s] implements more than one portal interface: %s and %s\n",
                             cl->name, superclass->name, ifclass->name, superclass->interfaces[i]->name);
                        /* check whether the portal interfaces are in an inheritance relation */
                        /* if yes use the subtype; otherwise throw exception */
                        if (check_assign(ifclass, superclass->interfaces[i])) {
                            ifclass = superclass->interfaces[i];
                        } else if (check_assign(superclass->interfaces[i], ifclass)) {
                            // do not change ifclass
                        } else {
                            exceptionHandlerMsg(THROW_RuntimeException,
                                        "Class implements more than one portal interface");
                        }
                    } else {
                        ifclass = superclass->interfaces[i];
                    }
                }
            }
        } while ((superclass = superclass->superclass) != NULL);


        if (curthr()->processingDEP && obj2ClassDesc(curthr()->processingDEP->obj)->inheritServiceThread) {
            printk(KERN_WARNING "INHERIT!!!\n");
            pool = curthr()->processingDEP->pool;
        } else {
            int i, index;
            pool = allocServicePoolInDomain(domain);
            index = -1;
            for (i = 0; i < CONFIG_JEM_MAX_SERVICES; i++) {
                if (domain->pools[i] == NULL) {
                    domain->pools[i] = pool;
                    index = i;
                    break;
                }
            }
            if (index == -1)
                exceptionHandlerMsg(THROW_RuntimeException, "no free pool slot");
            pool->index = index;
            printk(KERN_INFO "INITPOOL %d %d=%p\n", domain->id, index, domain->pools[index]);
            createServiceThread(domain, index, ifclass->name);
        }
        depIndex = createService(domain, obj, ifclass, pool);
        proxy = createPortalInDomain(domain, ifclass, domain, domain->id, depIndex);
        domain->services[depIndex]->proxy = proxy;

        proxy->targetDomain = domain;
        proxy->targetDomainID = domain->id;
        proxy->index = depIndex;
        return proxy;
    }
    printk(KERN_ERR "DOES NOT IMPLEMENT PORTAL INTERFACE");
    return NULL;
}

/* restart copy operation after GC */

static char *restartCopy(void)
{
    return (char *) 0xffffffff;
}


// copy object
static ObjectDesc *copy_shallow_object(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota)
{
    ClassDesc *cl = NULL;
    ObjectDesc *targetObj;
    u32 gcEpoch;

    cl = obj2ClassDesc(obj);

    if (implements_interface(cl, portalInterface)) {
        Proxy *proxy = portal_auto_promo(src, obj);
        /*  check if proxy type can be substituted for obj! */
        // guarantee that destination domain does not contain the service class
        if (findClass(dst, cl->name) != NULL) {
            printk(KERN_ERR "Target domain %d (%s) has loaded class %s\n", dst->id, dst->domainName, cl->name);
            exceptionHandlerMsg(THROW_RuntimeException, "Target domain has loaded service class");
        }
        return copy_shallow_portal(src, dst, proxy, quota, 1);
    }

    {
        u32 objsize = OBJSIZE_OBJECT(cl->instanceSize);
        if (*quota < objsize) {
            printk(KERN_ERR "quota < objsize\n");
            return NULL;
        }
        *quota -= objsize;
    }

    gcEpoch = dst->gc.epoch;
    targetObj = allocObjectInDomain(dst, cl);
    if (gcEpoch != dst->gc.epoch)
        return (ObjectDesc *) restartCopy();
    addToRefTable(obj, targetObj);
    memcpy(targetObj->data, obj->data, cl->instanceSize * 4);

    return targetObj;
}

static char *copy_content_object(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota)
{
    jbyte *addr;
    jint k;
    jbyte b = 0;
    ClassDesc *cl = NULL;
    ObjectDesc *ref;

    cl = obj2ClassDesc(obj);

    addr = cl->map;
    for (k = 0; k < cl->instanceSize; k++) {
        if (k % 8 == 0)
            b = *addr++;
        if (b & 1) {
            /* reference slot */
            ref = copy_shallow_reference_internal(src, dst, (ObjectDesc *) obj->data[k], quota);
            if (ref == (ObjectDesc *) 0xffffffff) {
                return (char *) 0xffffffff;
            }
            obj->data[k] = (jint) ref;
        } else {
            /* already copied in copy_shallow */
        }
        b >>= 1;
    }
    return NULL;
}

static ObjectDesc *copy_shallow_portal(DomainDesc * src, DomainDesc * dst, Proxy * obj, u32 * quota, jboolean addRef)
{
    ClassDesc *c;
    Proxy *o;
    u32 gcEpoch;
    u32 objsize = OBJSIZE_PORTAL;

    if (obj == NULL)
        return NULL;

    if ((obj->targetDomainID == dst->id) && !(obj->targetDomainID == 0 && obj->targetDomain == NULL && obj->index == 0)) {
        ObjectDesc *tobj = dst->services[obj->index]->obj;
        return tobj;
    }

    c = obj2ClassDesc(obj);

    if (*quota < objsize) {
        printk(KERN_ERR "quota < objsize\n");
        return NULL;
    }
    *quota -= objsize;

    gcEpoch = dst->gc.epoch;
    o = allocProxyInDomain(dst, NULL, obj->targetDomain, obj->targetDomainID, obj->index);
    if (gcEpoch != dst->gc.epoch)
        return (ObjectDesc *) restartCopy();
    if (addRef)
        addToRefTable((ObjectDesc *) obj, (ObjectDesc *) o);
    o->vtable = obj->vtable;
    return (ObjectDesc *) o;
}

static ObjectDesc *copy_shallow_domainproxy(DomainDesc * src, DomainDesc * dst, DomainProxy * obj, u32 * quota)
{
    DomainProxy *o;
    u32 gcEpoch;

    if (obj == NULL)
        return NULL;
    if (src == dst)
        return (ObjectDesc *) obj;

    {
        u32 objsize = OBJSIZE_DOMAIN;
        if (*quota < objsize) {
            printk(KERN_ERR "quota < objsize\n");
            return NULL;
        }
        *quota -= objsize;
    }

    gcEpoch = dst->gc.epoch;
    o = allocDomainProxyInDomain(dst, obj->domain, obj->domainID);
    if (gcEpoch != dst->gc.epoch)
        return (ObjectDesc *) restartCopy();
    addToRefTable((ObjectDesc *) obj, (ObjectDesc *) o);
    o->vtable = obj->vtable;
    return (ObjectDesc *) o;
}

static ObjectDesc *copy_shallow_foreign_cpustate(DomainDesc * src, DomainDesc * dst, ThreadDescForeignProxy * obj, u32 * quota)
{
    printk(KERN_INFO "Copy foreign cpustate not yet implemented\n");
    return NULL;
}

static ObjectDesc *copy_shallow_cpustate(DomainDesc * src, DomainDesc * dst, ThreadDescProxy * obj, u32 * quota)
{
    ThreadDescForeignProxy *o;
    u32 gcEpoch;

    if (obj == NULL)
        return NULL;
    if (src == dst)
        return (ObjectDesc *) obj;

    {
        u32 objsize = OBJSIZE_FOREIGN_THREADDESC;
        if (*quota < objsize) {
            printk(KERN_ERR "quota < objsize\n");
            return NULL;
        }
        *quota -= objsize;
    }

    gcEpoch = dst->gc.epoch;
    o = allocThreadDescForeignProxyInDomain(dst, obj);
    if (gcEpoch != dst->gc.epoch)
        return (ObjectDesc *) restartCopy();

    addToRefTable((ObjectDesc *) obj, (ObjectDesc *) o);
    o->vtable = obj->vtable;
    return (ObjectDesc *) o;
}

static ObjectDesc *copy_shallow_array(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota)
{
    ClassDesc *c;
    ArrayDesc *o;
    ArrayDesc *ar;
    u32 gcEpoch;

    o = (ArrayDesc *) obj;
    c = (ClassDesc *) o->arrayClass;
    {
        u32 objsize = gc_objSize(obj);
        if (*quota < objsize) {
            printk(KERN_ERR "PER-CALL QUOTA REACHED: TOO MANY OBJECTS COPIED DURING PORTAL CALL\n");
            return NULL;
        }
        *quota -= objsize;
    }
    gcEpoch = dst->gc.epoch;
    ar = allocArrayInDomain(dst, c->elementClass, o->size);
    if (gcEpoch != dst->gc.epoch)
        return (ObjectDesc *) restartCopy();
    addToRefTable((ObjectDesc *) obj, (ObjectDesc *) ar);

    memcpy(ar->data, o->data, o->size * 4);
    return (ObjectDesc *) ar;
}

static char *copy_content_array(DomainDesc * src, DomainDesc * dst, ObjectDesc * obj, u32 * quota)
{
    ClassDesc *c;
    ArrayDesc *o;

    o = (ArrayDesc *) obj;
    c = (ClassDesc *) o->arrayClass;
    if (c->name[1] == 'L') {
        /* reference array */
        u32 i;
        jint *ref;
        for (i = 0; i < o->size; i++) {
            ref = (jint *) copy_shallow_reference_internal(src, dst, (ObjectDesc *) o->data[i], quota);
            if (ref == (jint *) 0xffffffff) {
                return (char *) 0xffffffff;
            }
            o->data[i] = (jint) ref;
        }
    }
    return NULL;
}

static ObjectDesc *copy_shallow_reference_internal(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota)
{
    ClassDesc *refcl;
    u32 flags;
    ObjectDesc *ret;

    if (ref == NULL)
        return NULL;

    if (src == dst)
        return ref;

    flags = getObjFlags(ref);
    if ((flags & FORWARD_MASK) == GC_FORWARD) {
        /* object already copied, this is a forward reference */
        return (ObjectDesc *) (flags & FORWARD_PTR_MASK);
    }

    if (ref != NULL) {
        flags = getObjFlags(ref) & FLAGS_MASK;
        switch (flags) {
        case OBJFLAGS_OBJECT:
            refcl = obj2ClassDesc(ref);
            return copy_shallow_object(src, dst, ref, quota);
        case OBJFLAGS_PORTAL:
            return copy_shallow_portal(src, dst, (Proxy *) ref, quota, 1);
        case OBJFLAGS_MEMORY:
            ret = copy_shallow_memory(src, dst, (struct MemoryProxy_s *) ref, quota);
            return ret;
        case OBJFLAGS_DOMAIN:
            return copy_shallow_domainproxy(src, dst, (DomainProxy *) ref, quota);
        case OBJFLAGS_ARRAY:
            return copy_shallow_array(src, dst, ref, quota);
        case OBJFLAGS_EXTERNAL_STRING:
            return ref;
        case OBJFLAGS_CPUSTATE:
            return copy_shallow_cpustate(src, dst, (ThreadDescProxy *) ref, quota);
        case OBJFLAGS_FOREIGN_CPUSTATE:
            return copy_shallow_foreign_cpustate(src, dst, (ThreadDescForeignProxy *) ref, quota);
        case OBJFLAGS_ATOMVAR:
                exceptionHandlerMsg(THROW_RuntimeException, "AtomicVariable cannot be copied between domains");
                break;
        default:
            printk(KERN_ERR "UNKNOWN OBJECT TYPE 0x%x for object %p", flags, ref);
        }
    }

    return NULL;
}


static char *copy_content_reference_internal(DomainDesc * src, DomainDesc * dst, ObjectDesc * ref, u32 * quota)
{
    ClassDesc *refcl;
    u32 flags;

    flags = getObjFlags(ref);
    if ((flags & FORWARD_MASK) == GC_FORWARD) {
        printk(KERN_ERR "ref should be on target heap");
        return NULL;
    }

    switch (flags) {
    case OBJFLAGS_OBJECT:
        refcl = obj2ClassDesc(ref);
        return copy_content_object(src, dst, ref, quota);
        break;
    case OBJFLAGS_ARRAY:
        return copy_content_array(src, dst, ref, quota);
        break;
    case OBJFLAGS_PORTAL:
        break;
    case OBJFLAGS_MEMORY:
        copy_content_memory(src, dst, (struct MemoryProxy_s *) ref, quota);
        break;
    case OBJFLAGS_DOMAIN:
        break;
    case OBJFLAGS_FOREIGN_CPUSTATE:
        break;
    default:
        printk(KERN_ERR "UNKNOWN OBJECT TYPE 0x%x for object %p", flags, ref);
    }
    return NULL;
}


static void receive_dep(void *arg)
{
    u32 depIndex = (u32) arg;
    receive_portalcall(depIndex);
}


void reinit_service_thread(void)
{
    //RT_TASK         t1;
    //RT_TASK_INFO    t1info;
    ThreadDesc      *thread = curthr();

    // Unblock the sender
    Sched_portal_handoff_to_sender(thread->mostRecentlyCalledBy);

    // Start a new thread
    //rt_task_inquire(&thread->task, &t1info);
    //memcpy(&t1, &thread->task, sizeof(RT_TASK));
    //memset(&thread->task, 0, sizeof(RT_TASK));
    //rt_task_spawn(&thread->task, t1info.name, getJVMConfig()->serviceStackSz, getJVMConfig()->defaultServicePrio,
    //              T_FPU, receive_dep, (void *) thread->processingDEP->serviceIndex);
    //rt_task_delete(&t1);
}

void service_incRefcount(DEPDesc * p)
{
    //rt_mutex_acquire(&svTableLock, TM_INFINITE);
    p->refcount++;
    //rt_mutex_release(&svTableLock);
}

void start_notify_thread(void *dummy)
{
    ObjectDesc *obj = dummy;
    ClassDesc *svcClass;
    MethodDesc *m;

    svcClass = obj2ClassDesc(obj);

    if ((m = findMethod(curdom(), svcClass->name, "serviceFinalizer", "()V")) != NULL) {
        executeSpecial(curdom(), svcClass->name, "serviceFinalizer", "()V", obj, NULL, 0);
    }
}

void service_decRefcount(DomainDesc * domain, u32 index)
{
    DEPDesc *p = domain->services[index];
    ThreadDesc *thread;
    ClassDesc *svcClass;

    p->refcount--;
    if (p->refcount == 1) {
        printk("DELETE SERVICE %s\n", obj2ClassDesc(p->obj)->name);
        /* delete service (service object will stay on the heap as garbage) */
        p->refcount = 0;
        disconnectServiceFromPool(p);
        domain->services[index] = SERVICE_ENTRY_FREE;
    }

    /* check whether there is a finalizer method */
    svcClass = obj2ClassDesc(p->obj);
    if (findMethod(curdom(), svcClass->name, "serviceFinalizer", "()V") != NULL) {
        /* use a new thread to notify the domain that a service was deleted */
        thread = createThread(domain, start_notify_thread, p->obj /*svcObj */ , STATE_RUNNABLE,
                      getJVMConfig()->defaultServicePrio, "ServiceFinalizer");
    }
}

static void connectServiceToPool(DEPDesc * svc, ServiceThreadPool * pool)
{
    //rt_mutex_acquire(&svTableLock, TM_INFINITE);
    pool->refcount++;
    //rt_mutex_release(&svTableLock);
    svc->pool = pool;
}

static void disconnectServiceFromPool(DEPDesc * svc)
{
    svc->pool->refcount--;
    if (svc->pool->refcount == 0) {
        ThreadDesc *t = svc->pool->firstReceiver;
        while (t) {
            ThreadDesc *next = t->nextInReceiveQueue;
            terminateThread(t);
            t = next;
        }
    }
}


void portals_init(void)
{
    int result;

    //result = rt_mutex_create(&svTableLock, "serviceTableLock");
    if (result < 0) {
        printk(KERN_ERR "Unable to create service table lock, rc=%d\n", result);
        return;
    }
}


