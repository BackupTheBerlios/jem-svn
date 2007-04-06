//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
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
// zero.c
// 
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

void            init_componentmanager_portal(void);
void            init_clock_portal(void);
void            init_cas_portal(void);
void            init_bootfs_portal(void);
void            init_cpumanager_portal(void);
void            init_cpustate_portal(void);
void            init_credential_portal(void);
void            init_debugchannel_portal(void);
void            init_debugsupport_portal(void);
void            init_domain_portal(void);
void            init_domainmanager_portal(void);
void            init_irq_portal(void);
void            init_memory_portal(void);
void            init_memorymanager_portal(void);
void            init_mutex_portal(void);
void            init_naming_portal(void);
void            init_ports_portal(void);
void            init_vmclass_portal(void);
void            init_vmmethod_portal(void);
void            init_vmobject_portal(void);
void            init_fbemulation_portal(void);
void            init_disk_emulation_portal(void);
void            init_net_emulation_portal(void);
void            init_timer_emulation_portal(void);
void            init_object(void);


typedef jint(*code2_f) (jint a, jint b);

ClassDesc *portalInterface = NULL;

extern ClassDesc        *vmmethodClass;
extern ClassDesc        *domainClass;
extern SharedLibDesc    *zeroLib;

Proxy *getInitialNaming(void)
{
    return curdom()->initialNamingProxy;
}

void installInitialNaming(DomainDesc * srcDomain, DomainDesc * dstDomain, Proxy * naming)
{
    u32 quota = 1000;
    thread_prepare_to_copy();
    dstDomain->initialNamingProxy = (struct Proxy_s *) copy_reference(srcDomain, dstDomain, (ObjectDesc *) naming, &quota);
}

/********** support functions *******************/

static ClassDesc *createClassDescImplementingInterface(DomainDesc * domain, ClassDesc * cl, MethodInfoDesc * methods, int numMethods,
                        char *name)
{
    ClassDesc *c;

    c                       = jemMallocClassdesc(domain, strlen(name) + 1);
    c->classType            = CLASSTYPE_CLASS;
    c->magic                = MAGIC_CLASSDESC;
    c->definingLib          = NULL;
    c->superclass           = java_lang_Object;
    c->instanceSize         = 0;
    c->numberOfInterfaces   = 1;
    c->interfaces           = jemMallocClassdesctable(domain, c->numberOfInterfaces);
    c->interfaces[0]        = cl;
    c->vtableSym            = cl->vtableSym;
    c->vtableSize           = cl->vtableSize;
    strcpy(c->name, name);
    createVTable(domain, c);
    installVtables(domain, c, methods, numMethods, cl);
    return c;
}


static ClassDesc *createDZClass(SharedLibDesc * zeroLib, char *name, MethodInfoDesc * methods, 
                                jint numberOfMethods, char *subname)
{
    ClassDesc *c;
    ClassDesc *cd;
    c = findClassDescInSharedLib(zeroLib, name);
    if (c == NULL) {
        printk(KERN_ERR "Cannot find class %s. Perhaps you are using a wrong zero lib.", name);
        return NULL;
    }
    cd = createClassDescImplementingInterface(domainZero, c, methods, numberOfMethods, subname);
    return cd;
}


ClassDesc *init_zero_class(char *ifname, MethodInfoDesc * methods, jint size, jint instanceSize, jbyte * typeMap, char *subname)
{
    ClassDesc   *cd;
    jint        mapBytes;

    cd                  = createDZClass(zeroLib, ifname, methods, size / sizeof(MethodInfoDesc), subname);
    cd->instanceSize    = instanceSize;
    mapBytes            = (instanceSize + 7) >> 3;
    cd->mapBytes        = mapBytes;
    cd->map             = typeMap;

    return cd;
}


ObjectDesc *init_zero_dep(char *ifname, char *depname, MethodInfoDesc * methods, jint size, char *subname)
{
    Proxy       *proxy;
    ClassDesc   *ifclass;
    ClassDesc   *cd;
    ObjectDesc  *obj;

    ifclass     = findClassDescInSharedLib(zeroLib, ifname);
    cd          = createDZClass(zeroLib, ifname, methods, size / sizeof(MethodInfoDesc), subname);
    obj         = allocObjectInDomain(domainZero, cd);
    proxy       = portal_auto_promo(domainZero, obj);

    if (depname != NULL)
        registerPortal(domainZero, (ObjectDesc *) proxy, depname);

    return (ObjectDesc *) proxy;
}


ObjectDesc *init_zero_dep_without_thread(char *ifname, char *depname, MethodInfoDesc * methods, jint size, char *subname)
{
    ClassDesc   *cd;
    u32         index;
    Proxy       *instance;
    u32         depIndex;

    cd                          = createDZClass(zeroLib, ifname, methods, size / sizeof(MethodInfoDesc), subname);
    cd->proxyVtable             = cd->vtable;
    instance                    = allocProxyInDomain(domainZero, cd, NULL, domainZero->id, 0);
    instance->vtable            = cd->vtable;  /* NOT a real proxy */
    instance->targetDomain      = NULL;  /* indicates direct portal */
    instance->targetDomainID    = domainZero->id;
    depIndex                    = createService(domainZero, (ObjectDesc *) instance, cd, NULL);

    if (depname != NULL)
        registerPortal(domainZero, (ObjectDesc *) instance, depname);
    index = depIndex;

    return (ObjectDesc *) instance;
}


static void addZeroVtables(void)
{
    u32 i, j;
    SharedLibDesc *lib = zeroLib;
    for (i = 0; i < lib->numberOfClasses; i++) {
        for (j = 0; j < lib->allClasses[i].numberOfMethods; j++) {
            lib->allClasses[i].methods[j].objectDesc_vtable = vmmethodClass->vtable;
        }
    }
}



/****************************************/
/* INIT */

static char *start_libs[] = {
    "zero.jll",
    "jdk0.jll",
    "zero_misc.jll",
};
extern Proxy *initialNamingProxy;


void init_zero_from_lib(DomainDesc * domain, SharedLibDesc * zeroLib)
{
    domain = domainZero;

    portalInterface = findClassDescInSharedLib(zeroLib, "jx/zero/Portal");

    init_bootfs_portal();
    init_cas_portal();
    init_clock_portal();
    init_componentmanager_portal();
    init_cpumanager_portal();
    init_cpustate_portal();
    init_credential_portal();
//    init_debugchannel_portal();
//    init_debugsupport_portal();
    init_domain_portal();
    init_domainmanager_portal();
    init_irq_portal();
    init_memory_portal();
    init_memorymanager_portal();
    init_mutex_portal();
    init_naming_portal();
    init_ports_portal();
    init_vmclass_portal();
    init_vmmethod_portal();
    init_vmobject_portal();

#ifdef FRAMEBUFFER_EMULATION
    init_fbemulation_portal();
#endif              /* FRAMEBUFFER_EMULATION */

#ifdef DISK_EMULATION
    init_disk_emulation_portal();
#endif              /* DISK_EMULATION */

#ifdef NET_EMULATION
    init_net_emulation_portal();
#endif              /* NET_EMULATION */

#ifdef TIMER_EMULATION
    init_timer_emulation_portal();
#endif              /* TIMER_EMULATION */

    /* now we can add a vtable to the DomainZero Domain object */
    //domainDesc2Obj(domainZero)->vtable = (u32)domainClass->vtable;

    /* add zero vtables */
    addZeroVtables();


    init_object();
}


void start_domain_zero(void *arg)
{
    DomainDesc      *domainInit;
    DomainProxy     *domainProxy;
    LibDesc         *lib;
    ObjectDesc      *dm;
    ArrayDesc       *arr;

    printk(KERN_INFO "Starting DomainZero.\n");

    /* load zero lib and create portals */
    lib = load(domainZero, "zero.jll");
    if (lib == NULL) {
        printk(KERN_ERR "Cannot load lib %s\n", "zero.jll");
        return;
    }

    /*
     * Create virtual method tables for interaction between 
     * Java code and C-code that implements DomainZero.
     */
    zeroLib = lib->sharedLib;

    init_zero_from_lib(domainZero, lib->sharedLib);

    /* Domainzero's naming does now exist.
     * Make it available.
     */
    domainZero->initialNamingProxy = initialNamingProxy;

    callClassConstructors(domainZero, lib);

    lib = load(domainZero, "jdk0.jll");
    if (lib == NULL) {
        printk(KERN_ERR "Cannot load lib %s\n", "jdk0.jll");
        return;
    }
    callClassConstructors(domainZero, lib);

  /*********************************
   * Create and start initial Java domain
   *********************************/
    dm = (ObjectDesc *) lookupPortal("DomainManager");
    arr = (ArrayDesc *) newStringArray(domainZero, sizeof(start_libs) / sizeof(char *), start_libs);

    thread_prepare_to_copy();

    domainProxy =
        domainManager_createDomain(dm, newString(domainZero, "Init"), NULL, NULL, newString(domainZero, "init.jll"), arr,
                       newString(domainZero, "jx/init/Main"), getJVMConfig()->heapBytesDom0, -1, -1, NULL, -1,
                       getJVMConfig()->codeBytesDom0, NULL, (ObjectDesc *) domainZero->initialNamingProxy, NULL,
                       GC_IMPLEMENTATION_DEFAULT, NULL);
    domainInit = domainProxy->domain;

    printk(KERN_INFO "DomainZero initial thread done.\n");

    rt_event_signal(&jemEvents, JEM_INIT_COMPLETE);
    rt_task_yield();

    /* initial thread of DomainZero exits here */
}

