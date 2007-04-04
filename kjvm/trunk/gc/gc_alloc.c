//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
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
//=================================================================================
// Garbage collector object allocation
//=================================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
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
#include "gc_impl.h"
#include "load.h"
#include "exception_handler.h"

Proxy *allocProxyInDomain(DomainDesc * domain, ClassDesc * c, DomainDesc * targetDomain, u32 targetDomainID, u32 depIndex)
{
    ObjectHandle handle;
    Proxy *proxy;
    jint objSize = OBJSIZE_PORTAL;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_PORTAL);
    proxy = (Proxy *) unregisterObject(domain, handle);
    if (c != NULL)
        proxy->vtable = c->proxyVtable;
    else
        proxy->vtable = NULL;
    proxy->targetDomain = targetDomain;
    proxy->targetDomainID = targetDomainID;
    proxy->index = depIndex;
    if (targetDomain && (targetDomain->id == targetDomainID)) {
        /* valid portal */
        service_incRefcount(targetDomain->services[depIndex]);
    }
    return proxy;
}


AtomicVariableProxy *allocAtomicVariableProxyInDomain(DomainDesc * domain, ClassDesc * c)
{
    ObjectHandle handle;
    AtomicVariableProxy *obj;
    jint objSize = OBJSIZE_ATOMVAR;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_ATOMVAR);
    obj = (AtomicVariableProxy *) unregisterObject(domain, handle);
    obj->vtable = c->vtable;
    obj->value = NULL;
    obj->blockedThread = NULL;
    obj->listMode = 0;
    return obj;
}

CASProxy *allocCASProxyInDomain(DomainDesc * domain, ClassDesc * c, u32 index)
{
    ObjectHandle handle;
    CASProxy *obj;
    jint objSize = OBJSIZE_CAS;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_CAS);
    obj = (CASProxy *) unregisterObject(domain, handle);
    obj->vtable = c->vtable;
    obj->index = index;
    return obj;
}

extern ClassDesc *vmobjectClass;
VMObjectProxy *allocVMObjectProxyInDomain(DomainDesc * domain)
{
    ObjectHandle handle;
    VMObjectProxy *obj;
    jint objSize = OBJSIZE_VMOBJECT;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_VMOBJECT);
    obj = (VMObjectProxy *) unregisterObject(domain, handle);
    obj->vtable = vmobjectClass->vtable;
    obj->domain = NULL;
    obj->domain_id = 0;
    obj->epoch = 0;
    obj->type = 0;
    obj->obj = NULL;
    obj->subObjectIndex = 0;
    return (VMObjectProxy *) obj;
}

CredentialProxy *allocCredentialProxyInDomain(DomainDesc * domain, ClassDesc * c, u32 signerDomainID)
{
    ObjectHandle handle;
    CredentialProxy *obj;
    jint objSize = OBJSIZE_CREDENTIAL;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_CREDENTIAL);
    obj = (CredentialProxy *) unregisterObject(domain, handle);
    obj->vtable = c->vtable;
    obj->value = NULL;
    obj->signerDomainID = signerDomainID;
    return obj;
}

extern ClassDesc *domainClass;

DomainProxy *allocDomainProxyInDomain(DomainDesc * domain, DomainDesc * domainValue, u32 domainID)
{
    ObjectHandle handle;
    DomainProxy *obj;
    ClassDesc *c = domainClass;
    jint objSize = OBJSIZE_DOMAIN;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_DOMAIN);
    obj = (DomainProxy *) unregisterObject(domain, handle);
    obj->vtable = c->vtable;
    obj->domain = domainValue;
    obj->domainID = domainID;
    return obj;
}

ThreadDescProxy *allocThreadDescProxyInDomain(DomainDesc * domain, ClassDesc * c)
{
    ObjectHandle handle;
    ObjectDesc *obj;
    jint objSize = OBJSIZE_THREADDESCPROXY;

    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_CPUSTATE);
    obj = (ObjectDesc *) unregisterObject(domain, handle);
    if (c != NULL)
        obj->vtable = c->vtable;
    else
        obj->vtable = NULL; /* bootstrap of DomainZero */

    return (ThreadDescProxy *) obj;
}


ThreadDescForeignProxy *allocThreadDescForeignProxyInDomain(DomainDesc * domain, ThreadDescProxy * src)
{
    ObjectHandle handle;
    ThreadDescForeignProxy *obj;
    ClassDesc *c = obj2ClassDesc(src);
    jint objSize = OBJSIZE_FOREIGN_THREADDESC;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_FOREIGN_CPUSTATE);
    obj = (ThreadDescForeignProxy *) unregisterObject(domain, handle);
    if (c != NULL)
        obj->vtable = c->vtable;
    else
        obj->vtable = NULL; /* bootstrap of DomainZero */

    obj->thread = &(src->desc);
    obj->threadID = src->desc.threadID;
    obj->gcEpoch = src->desc.domain->gc.epoch;
    obj->domain = allocDomainProxyInDomain(domain, src->desc.domain, src->desc.domain->id);
    // ... domain gc epoch id

    return obj;
}


extern ClassDesc *interceptInboundInfoClass;
extern ClassDesc *interceptPortalInfoClass;
InterceptInboundInfoProxy *allocInterceptInboundInfoProxyInDomain(DomainDesc * domain)
{
    ObjectHandle handle;
    InterceptInboundInfoProxy *obj;
    jint objSize = OBJSIZE_INTERCEPTINBOUNDINFO;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_INTERCEPTINBOUNDINFO);
    obj = (InterceptInboundInfoProxy *) unregisterObject(domain, handle);
    obj->vtable = interceptInboundInfoClass->vtable;
    obj->source = NULL;
    obj->target = NULL;
    obj->method = NULL;
    obj->obj = NULL;
    obj->paramlist = NULL;
    obj->index = 1;
    return obj;
}

InterceptPortalInfoProxy *allocInterceptPortalInfoProxyInDomain(DomainDesc * domain)
{
    ObjectHandle handle;
    InterceptPortalInfoProxy *obj;
    jint objSize = OBJSIZE_INTERCEPTPORTALINFO;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_INTERCEPTPORTALINFO);
    obj = (InterceptPortalInfoProxy *) unregisterObject(domain, handle);
    obj->vtable = interceptPortalInfoClass->vtable;
    obj->domain = NULL;
    obj->index = 0;
    return obj;
}

ObjectDesc *allocObjectInDomain(DomainDesc * domain, ClassDesc * c)
{
    ObjectDesc *obj;
    ObjectHandle handle;
    jint objSize;

    objSize = OBJSIZE_OBJECT(c->instanceSize);

    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_OBJECT);
    obj = unregisterObject(domain, handle);
    obj->vtable = c->vtable;
    return obj;
}

DEPDesc *allocServiceDescInDomain(DomainDesc * domain)
{
    ObjectHandle handle;
    DEPDesc *dep;
    jint objSize = OBJSIZE_SERVICEDESC;
    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_SERVICE);
    dep = (DEPDesc *) unregisterObject(domain, handle);
    memset(dep, 0, sizeof(DEPDesc));
    dep->magic = MAGIC_DEP;
    return dep;
}

ServiceThreadPool *allocServicePoolInDomain(DomainDesc * domain)
{
    ObjectHandle        handle;
    ServiceThreadPool   *dep;
    char                lockName[20];
    jint                objSize = OBJSIZE_SERVICEPOOL;
    int                 result;

    handle  = gc_allocDataInDomain(domain, objSize, OBJFLAGS_SERVICE_POOL);
    dep     = (ServiceThreadPool *) unregisterObject(domain, handle);
    memset(dep, 0, sizeof(ServiceThreadPool));
    dep->magic = MAGIC_DEP;
    sprintf(lockName, "dom%03dSvPoolLock", domain->id);
    if ((result = rt_mutex_create(&dep->poolLock, lockName)) < 0) {
        printk(KERN_ERR "Can not create service thread pool lock, rc=%d\n", result);
    }
    return dep;
}

ClassDesc *cpuStateClass = NULL;
ClassDesc *stackClass = NULL;
ClassDesc *domainClass = NULL;
ClassDesc *cpuClass = NULL;

CPUDesc *specialAllocCPUDesc(void)
{
    CPUDesc *c;
    ObjectDesc *obj;

    u32 *mem = (u32 *) jemMallocCpudesc(domainZero, OBJSIZE_CPUDESC * 4);
    c = (CPUDesc *) (mem + 2 + XMOFF);
    obj = CPUDesc2ObjectDesc(c);
    setObjFlags(obj, OBJFLAGS_EXTERNAL_CPUDESC);    /* flags */
    setObjMagic(obj, MAGIC_OBJECT);
    if (cpuClass != NULL)
        obj->vtable = cpuClass->vtable; /* vtable */
    else
        obj->vtable = NULL; /* during bootstrap of DomainZero! */
    c->magic = MAGIC_CPU;
    return c;
}


ObjectDesc *specialAllocObject(ClassDesc * c)
{
    return allocObjectInDomain(curdom(), c);
}

ArrayDesc *specialAllocArray(ClassDesc * elemClass0, jint size)
{
    if (size < 0)
        exceptionHandler(THROW_RuntimeException);
    return allocArrayInDomain(curdom(), elemClass0, size);
}

ArrayDesc *vmSpecialAllocArray(ClassDesc * elemClass0, jint size)
{
    return (ArrayDesc *) specialAllocArray(elemClass0, size);
}


/*
 *
 * AllocMultiArray Hack !!! fixme !!!
 *
 * !!! recursiv and not gc save !!!
 *
 */

ArrayDesc *doAllocMultiArray(ClassDesc * elemClass, jint dim, jint * oprs);

ArrayDesc *vmSpecialAllocMultiArray(ClassDesc * elemClass, jint dim, jint sizes)
{
    return doAllocMultiArray(elemClass, dim, (jint *) (&sizes));
}

ArrayDesc *doAllocMultiArray(ClassDesc * arrayClass, jint dim, jint * oprs)
{
    ArrayDesc *array;
    ClassDesc *elemClass;
    int i;
    u32 gcEpoch;
    DomainDesc *domain = curdom();

    elemClass = findClassOrPrimitive(domain, arrayClass->name + 1)->classDesc;

    if (dim == 1) {
        return (ArrayDesc *) specialAllocArray(elemClass, oprs[0]);
    }

      restart:
    gcEpoch = domain->gc.epoch;
    array = (ArrayDesc *) specialAllocArray(elemClass, oprs[0]);
    if (gcEpoch != domain->gc.epoch)
        goto restart;

    for (i = 0; i < oprs[0]; i++) {
        array->data[i] = (jint) doAllocMultiArray(elemClass, dim - 1, oprs + 1);
        if (gcEpoch != domain->gc.epoch)
            goto restart;
    }
    return array;
}

/*
 *
 * !!! end of AllocMultiArray Hack !!! 
 *
 */

u32 *specialAllocStaticFields(DomainDesc * domain, int numberFields)
{
    return (u32 *) jemMallocStaticfields(domain, numberFields);
}

ArrayDesc *allocByteArray(DomainDesc * domain, ClassDesc * elemClass, jint size)
{
    jint objSize;
    ObjectHandle handle;
    ArrayDesc *obj;
    ClassDesc *arrayClass;
    char name[256];

    strcpy(name, "[");
    if (elemClass->classType == CLASSTYPE_CLASS || elemClass->classType == CLASSTYPE_INTERFACE) {
        strcat(name, "L");
    }
    strcat(name, elemClass->name);
    if (elemClass->classType == CLASSTYPE_CLASS || elemClass->classType == CLASSTYPE_INTERFACE) {
        strcat(name, ";");
    }

    if (elemClass->arrayClass == NULL) {
        arrayClass = findSharedArrayClassDescByElemClass(elemClass);
    } else {
        arrayClass = elemClass->arrayClass;
    }

    objSize = OBJSIZE_ARRAY_32BIT(size);

    handle = gc_allocDataInDomain(domain, objSize, OBJFLAGS_ARRAY);
    obj = (ArrayDesc *) unregisterObject(domain, handle);
    obj->arrayClass = (ClassDesc *) arrayClass;
    obj->size = size;
    obj->vtable = arrayClass->vtable;
    return obj;
}

ArrayDesc *allocArray(ClassDesc * elemClass, jint size)
{
    return allocArrayInDomain(curdom(), elemClass, size);
}

ArrayDesc *allocArrayInDomain(DomainDesc * domain, ClassDesc * elemClass, jint size)
{
    return allocByteArray(domain, elemClass, size);
}


