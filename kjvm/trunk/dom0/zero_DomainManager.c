//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
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
// zero_DomainManager.c
//
// DomainZero DomainManager
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "vmsupport.h"
#include "load.h"
#include "exception_handler.h"
#include "zero.h"
#include "malloc.h"
#include "gc_alloc.h"

#define CALLERDOMAIN (curthr()->mostRecentlyCalledBy?curthr()->mostRecentlyCalledBy->domain:domainZero)

extern ClassDesc *domainClass;


void start_initial_thread(void *dummy)
{
    LibDesc *lib;
    char value[80];
    jint i, n_libs;
    ObjectDesc *o;
    u32 dz[3];
    DomainDesc *domain;

    domain = curdom();
    if (domain->libNames != NULL) {
        n_libs = getArraySize(domain->libNames);
        for (i = 0; i < n_libs; i++) {
            o = getReferenceArrayElement(domain->libNames, i);
            stringToChar(o, value, sizeof(value));
            lib = load(curdom(), value);
            if (lib == NULL) {
                printk(KERN_ERR "Cannot load lib %s\n", value);
                return;
            }
        }
        domain->libNames = NULL;    /* no longer needed */
    }

    stringToChar(domain->dcodeName, value, sizeof(value));
    lib = load(curdom(), value);
    if (lib == NULL) {
        printk(KERN_ERR "Cannot load domain file %s\n", value);
        return;
    }

    domain->dcodeName = NULL;   /* no longer needed */

    stringToChar(domain->startClassName, value, sizeof(value));
    domain->startClassName = NULL;  /* don't need this object any longer */

    dz[0] = (jint) getInitialNaming();

    /* execute class constructors */
    for (i = 0; i < curdom()->numberOfLibs; i++) {
        lib = curdom()->libs[i];
        callClassConstructors(curdom(), lib);
    }

    if (!curdom()->argv) {
        curdom()->argv = (ArrayDesc *) newStringArray(curdom(), 0, NULL);
    }
    /* try to find new init method */
    if (findMethod(curdom(), value, "init", "(Ljx/zero/Naming;[Ljava/lang/String;[Ljava/lang/Object;)V")
        != NULL) {
        dz[1] = (u32) curdom()->argv;
        dz[2] = (u32) curdom()->initialPortals;
        executeStatic(curdom(), value, "init", "(Ljx/zero/Naming;[Ljava/lang/String;[Ljava/lang/Object;)V", (jint *) dz,
                  3);
    } else if (findMethod(curdom(), value, "init", "(Ljx/zero/Naming;[Ljava/lang/String;)V") != NULL) {
        dz[1] = (u32) curdom()->argv;
        executeStatic(curdom(), value, "init", "(Ljx/zero/Naming;[Ljava/lang/String;)V", (jint *) dz, 2);
    } else if (findMethod(curdom(), value, "init", "(Ljx/zero/Naming;)V")
           != NULL) {
        /* try to find old fashioned init method */
        executeStatic(curdom(), value, "init", "(Ljx/zero/Naming;)V", (jint *) dz, 1);
    } else {
        printk(KERN_ERR " %s.init(Ljx/zero/Naming;[Ljava/lang/String;)V\n", value);
        printk(KERN_ERR "Init method not found for domain %d\n", domain->id);
        return;
    }
}

/* create Domain without HLScheduler */
static DomainDesc *__domainManager_createDomain(ObjectDesc * self, ObjectDesc * dname, ArrayDesc * cpuObjs,
                        ObjectDesc * dcodeName, ArrayDesc * libsName, ObjectDesc * startClassName,
                        jint gcinfo0, jint gcinfo1, jint gcinfo2, ObjectDesc * gcinfo3, jint gcinfo4,
                        jint codeSize, ArrayDesc * argv, ObjectDesc * naming, ArrayDesc * moreArgs,
                        jint gcImpl, ArrayDesc * schedInfo)
{
    char value[80];
    char value1[80];
    DomainDesc *domain;
    DomainDesc *sourceDomain;
    DomainDesc *dataSrcDomain;
    u32 quota = getJVMConfig()->createDomainPortalQuota;

    if (dname == 0)
        return NULL;

    sourceDomain = CALLERDOMAIN;

    /* name */
    stringToChar(dname, value, sizeof(value));

    /* hack: get principle for memory accounting */
    if (gcinfo3) {
        stringToChar(gcinfo3, value1, sizeof(value1));
    } else {
        value1[0] = '\0';
    }
    domain = createDomain(value, gcinfo0, gcinfo1, gcinfo2, value1, gcinfo4, codeSize, gcImpl);
    if (domain == NULL)
        exceptionHandlerMsg(THROW_RuntimeException, "Cannot create domain.");

/* create GC thread before copying data to the domain */
#ifdef CONFIG_JEM_ENABLE_GC
    {
        domain->gc.gcThread = createThread(domain, NULL, NULL, STATE_AVAILABLE, SCHED_CREATETHREAD_NORUNQ);
        setThreadName(domain->gc.gcThread, "GC", NULL);
        domain->gc.gcThread->isGCThread = 1;
        domain->gc.gcCode = gc_in;
    }
#endif

    if (naming == NULL) {
        // inherit naming from parent
        installInitialNaming(sourceDomain, domain, sourceDomain->initialNamingProxy);
    } else {
        // install user supplied naming
        installInitialNaming(sourceDomain, domain, (Proxy *) naming);
    }

    dataSrcDomain           = sourceDomain;
    domain->startClassName  = copy_reference(dataSrcDomain, domain, (ObjectDesc *) startClassName, &quota);
    domain->dcodeName       = copy_reference(dataSrcDomain, domain, (ObjectDesc *) dcodeName, &quota);
    domain->libNames        = (ArrayDesc *) copy_reference(dataSrcDomain, domain, (ObjectDesc *) libsName, &quota);
    domain->argv            = (ArrayDesc *) copy_reference(dataSrcDomain, domain, (ObjectDesc *) argv, &quota);
    domain->initialPortals  = (ArrayDesc *) copy_reference(dataSrcDomain, domain, (ObjectDesc *) moreArgs, &quota);
    return domain;
}


DomainProxy *domainManager_createDomain(ObjectDesc * self, ObjectDesc * dname, ArrayDesc * cpuObjs, ArrayDesc * HLSNames,
                    ObjectDesc * dcodeName, ArrayDesc * libsName, ObjectDesc * startClassName, jint gcinfo0,
                    jint gcinfo1, jint gcinfo2, ObjectDesc * gcinfo3 /*hack */ , jint gcinfo4, jint codeSize,
                    ArrayDesc * argv, ObjectDesc * naming, ArrayDesc * portals, jint gcImpl,
                    ArrayDesc * schedInfo)
{
    ThreadDesc *thread;
    DomainDesc *domain;
    DomainProxy *domainProxy;
    DomainDesc *callerDomain;

    callerDomain = CALLERDOMAIN;

    /* create Domain without HLS */
    domain =
        __domainManager_createDomain(self, dname, cpuObjs, dcodeName, libsName, startClassName, gcinfo0, gcinfo1, gcinfo2,
                     gcinfo3, gcinfo4, codeSize, argv, naming, portals, gcImpl, schedInfo);

    thread = createInitialDomainThread(domain, STATE_RUNNABLE, getJVMConfig()->defaultDomainPrio);

    domainProxy = allocDomainProxyInDomain(curdom(), domain, domain->id);

    return domainProxy;
}


static ObjectDesc *domainManager_getDomainZero(ObjectDesc * self)
{
    DomainProxy *domainProxy;
    domainProxy = allocDomainProxyInDomain(curdom(), domainZero, domainZero->id);
    return (ObjectDesc *) domainProxy;
}

static ObjectDesc *domainManager_getCurrentDomain(ObjectDesc * self)
{
    DomainDesc *sourceDomain = CALLERDOMAIN;
    DomainProxy *domainProxy;
    domainProxy = allocDomainProxyInDomain(curdom(), sourceDomain, sourceDomain->id);
    return (ObjectDesc *) domainProxy;
}

extern SharedLibDesc *zeroLib;

static void domainManager_installInterceptor(ObjectDesc * self, DomainProxy * domainObj, ObjectDesc * interceptor,
                      ObjectDesc * interceptorThread)
{
    exceptionHandler(THROW_RuntimeException);   /* no support for interception */
}


static void domainManager_terminate(ObjectDesc * self, DomainProxy * domainObj)
{
    if (domainObj->domain->id == domainObj->domainID) {
        terminateDomain(domainObj->domain);
    }
}

static void domainManager_terminateCaller(ObjectDesc * self)
{
    DomainDesc *domain = CALLERDOMAIN;
    terminateDomain(domain);
}

static void domainManager_freeze(ObjectDesc * self, ObjectDesc * domainObj)
{
    return;
}

static void domainManager_thaw(ObjectDesc * self, ObjectDesc * domainObj)
{
    return;
}

static void domainManager_gc(ObjectDesc * self, DomainProxy * domainObj)
{
    DomainDesc *domain;

    if (domainObj->domain->id != domainObj->domainID)
        return;
    domain = domainObj->domain;
#ifdef CONFIG_JEM_ENABLE_GC
    if (domain->gc.gcThread == NULL) {
        printk(KERN_ERR "GC but no GC thread available\n");
        return;
    }
    start_thread_using_code1(domain->gc.gcObject, domain->gc.gcThread, domain->gc.gcCode, (u32) domain);
#endif
}

MethodInfoDesc domainManagerMethods[] = {
    {"createDomain", "", (code_t) domainManager_createDomain}
    ,
    {"getDomainZero", "", (code_t) domainManager_getDomainZero}
    ,
    {"getCurrentDomain", "", (code_t) domainManager_getCurrentDomain}
    ,
    {"installInterceptor", "",
     (code_t) domainManager_installInterceptor}
    ,

    {"freeze", "(Ljx/zero/Domain;)V", (code_t) domainManager_freeze}
    ,
    {"thaw", "(Ljx/zero/Domain;)V", (code_t) domainManager_thaw}
    ,
    {"terminate", "(Ljx/zero/Domain;)V",
     (code_t) domainManager_terminate}
    ,
    {"terminateCaller", "()V", (code_t) domainManager_terminateCaller}
    ,

    {"gc", "(Ljx/zero/Domain;)V", (code_t) domainManager_gc}
    ,
};

void init_domainmanager_portal(void)
{
    init_zero_dep("jx/zero/DomainManager", "DomainManager", domainManagerMethods, sizeof(domainManagerMethods),
              "<jx/zero/DomainManager>");
}
