//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 Christopher Stone. 
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
//
// Jem/JVM lock aspect.
//   This aspect adds all of the concurrency locking code to kjvm. The locking
//   code was broken out into an aspect because it makes it possible to
//   change to a different locking API by just changing the code in this 
//   aspect. For instance, one may want to use Xenomai's POSIX API instead
//   of the native linux mutex's and semaphores.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <asm/semaphore.h>
#include "jemtypes.h"
#include "domain.h"

static struct mutex     domainLock;
static struct mutex     svTableLock;

introduce(): intype(struct DomainDesc_s) 
{
    struct semaphore    serviceSem;
    struct mutex        domainMemLock;
    struct mutex        domainHeapLock;
}

introduce(): intype(struct GCDesc_s)
{
    struct mutex        gcLock;
}

introduce(): intype(struct ServiceThreadPool_s)
{
    struct mutex        poolLock;
}

before(DomainDesc *domain): execution(char *jemMallocCode(DomainDesc *, u32)) && args(domain)
{
    mutex_lock(&domain->domainMemLock);
}

after(DomainDesc *domain): execution(char *jemMallocCode(DomainDesc *, u32)) && args(domain)
{
    mutex_unlock(&domain->domainMemLock);
}

before(): execution(DomainDesc *specialAllocDomainDesc())
{
    mutex_lock(&domainLock);
}

after(DomainDesc *d): execution(DomainDesc *specialAllocDomainDesc()) && result(d)
{
    mutex_init(&d->domainMemLock);
    mutex_init(&d->domainHeapLock);
    mutex_init(&d->gc.gcLock);
    sema_init(&d->serviceSem, 0);
    mutex_unlock(&domainLock);
}

before(): call(DomainDesc *createDomain(...)) && infunc(initDomainSystem)
{
    mutex_init(&domainLock);
}

void around(): call(void jem_mutex_acquire()) && (infunc(terminateDomain) || infunc(createService))
{
    mutex_lock(&svTableLock);
}

void around(): call(void jem_mutex_release()) && infunc(terminateDomain)
{
    mutex_unlock(&svTableLock);
}

before(): execution(int findMethodAtAddrInDomain(...))
{
    mutex_lock(&domainLock);
}

after(): execution(int findMethodAtAddrInDomain(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(int findMethodAtAddr(...))
{
    mutex_lock(&domainLock);
}

after(): execution(int findMethodAtAddr(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(int findProxyCode(...))
{
    mutex_lock(&domainLock);
}

after(): execution(int findProxyCode(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(DomainDesc *findDomain(...))
{
    mutex_lock(&domainLock);
}

after(): execution(DomainDesc *findDomain(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(DomainDesc *findDomainByName(...))
{
    mutex_lock(&domainLock);
}

after(): execution(DomainDesc *findDomainByName(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(void foreachDomain(...))
{
    mutex_lock(&domainLock);
}

after(): execution(void foreachDomain(...))
{
    mutex_unlock(&domainLock);
}

before(): execution(void findClassForMethod(...))
{
    mutex_lock(&domainLock);
}

after(): execution(void findClassForMethod(...))
{
    mutex_unlock(&domainLock);
}

