//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 JavaDevices Software. 
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
#include "object.h"
#include "domain.h"

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

before(DomainDesc *domain): execution(char *jemMallocCode(DomainDesc *, u32)) && args(domain)
{
    mutex_lock(&domain->domainMemLock);
}

after(DomainDesc *domain): execution(char *jemMallocCode(DomainDesc *, u32)) && args(domain)
{
    mutex_unlock(&domain->domainMemLock);
}

