//==============================================================================
// domain.c
// 
// Jem/JVM domain implementation
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include "jemtypes.h"
#include "malloc.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "execjava.h"
#include "javascheduler.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"

static u32          numberOfDomains = 0;
static u32          currentDomainID = 0;
static DomainDesc   *domainZero;
static char         *domainMem = NULL;
static char         *domainMemBorder = NULL;
static char         *domainMemCurrent = NULL;
static RT_MUTEX     domainLock;

#define DOMAINMEM_SIZEBYTES (OBJSIZE_DOMAINDESC*4)
#define DOMAINDESC_OFFSETBYTES (( 2 + XMOFF) *4)


DomainDesc *getDomainZero(void)
{
    return domainZero;
}


// TEMPTEMP

void gc_init(DomainDesc * domain, u8 * mem, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4, int gcImpl)
{
}

void sched_local_init(DomainDesc * domain, int schedImpl)
{
}


u32 gc_mem(void)
{
    return 100;
}
// TEMPTEMP

static DomainDesc *specialAllocDomainDesc(void)
{
	DomainDesc  *d;
	char        *domainMemStart, lockName[15];

    int result = rt_mutex_acquire(&domainLock, TM_INFINITE);
    if (result < 0) {
        printk(KERN_ERR "Error acquiring domain lock, code=%d\n", result);
        return NULL;
    }

	if (numberOfDomains == getJVMConfig()->maxDomains) {
        rt_mutex_release(&domainLock);
		printk(KERN_ERR "Maximum domains have been created.\n");
        return NULL;
    }

	domainMemStart = domainMemCurrent;
	do {
		char *mem = domainMemCurrent;
		d = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
		domainMemCurrent += DOMAINMEM_SIZEBYTES;
		if (domainMemCurrent >= domainMemBorder)
			domainMemCurrent = domainMem;
		if (domainMemCurrent == domainMemStart) {
            printk(KERN_ERR "Maximum domains have been created.\n");
            rt_mutex_release(&domainLock);
			return NULL;
		}
	} while (d->state != DOMAIN_STATE_FREE);

	memset(d, 0, sizeof(DomainDesc));
	d->magic    = MAGIC_DOMAIN;
	d->state    = DOMAIN_STATE_CREATING;
	d->id       = currentDomainID++;
	numberOfDomains++;

    sprintf(lockName, "dom%03dMemLock", d->id);
    result          = rt_mutex_create(&d->domainMemLock, lockName);
    if (result < 0) {
        printk(KERN_CRIT "Unable to create domain memory lock %s, rc=%d\n", lockName, result);
        rt_mutex_release(&domainLock);
        return NULL;
    }

    rt_mutex_release(&domainLock);
	return d;
}


DomainDesc *createDomain(char *domainName, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4, 
                         u32 code_bytes, int gcImpl, ArrayDesc * schedinfo)
{
	u8          *mem;
	DomainDesc  *domain;
	u32         libMemSize;

	domain = specialAllocDomainDesc();
	if (domain == NULL) {
		return NULL;
	}

	domain->maxNumberOfLibs = getJVMConfig()->maxNumberLibs;
	domain->numberOfLibs    = 0;
	domain->arrayClasses    = NULL;
	domain->scratchMem      = jemMalloc(getJVMConfig()->domScratchMemSz MEMTYPE_OTHER);
	domain->scratchMemSize  = getJVMConfig()->domScratchMemSz;

	domain->cur_code = -1;
	if (code_bytes != -1) {
		domain->code_bytes = code_bytes;
	} else {
		domain->code_bytes = CONFIG_CODE_BYTES;
	}

	libMemSize = sizeof(LibDesc *) * domain->maxNumberOfLibs + strlen(domainName) + 1
	    + sizeof(LibDesc *) * domain->maxNumberOfLibs + sizeof(jint *) * domain->maxNumberOfLibs
	    + gc_mem();
	mem = jemMallocCode(domain, libMemSize);

	domain->libMemSize  = libMemSize;
	domain->libs        = (LibDesc **) mem;
	mem                 += sizeof(LibDesc *) * domain->maxNumberOfLibs;
	domain->domainName  = mem;
	mem                 += strlen(domainName) + 1;

	domain->ndx_libs    = (LibDesc **) mem;
	mem                 += sizeof(LibDesc *) * domain->maxNumberOfLibs;
	memset(domain->ndx_libs, 0, sizeof(LibDesc *) * domain->maxNumberOfLibs);
	domain->sfields     = (jint **) mem;
	mem                 += sizeof(jint *) * domain->maxNumberOfLibs;
	memset(domain->sfields, 0, sizeof(jint *) * domain->maxNumberOfLibs);

	strcpy(domain->domainName, domainName);
	domain->threads     = NULL;
	domain->services[0] = SERVICE_ENTRY_CHANGING;

	gc_init(domain, mem, gcinfo0, gcinfo1, gcinfo2, gcinfo3, gcinfo4, gcImpl);

#ifdef NEW_SCHED
	sched_local_init(domain, 0);
#endif

	domain->state = DOMAIN_STATE_ACTIVE;

	return domain;
}


void initDomainSystem(void)
{
	DomainDesc          *d;
	char                *domainMemStart;
    int                 result;
    struct jvmConfig    *jvmConfigData = getJVMConfig();

	domainMem           = (char *) jemMalloc((DOMAINMEM_SIZEBYTES * jvmConfigData->maxDomains) MEMTYPE_DCB);
	domainMemStart      = domainMem;
	numberOfDomains     = 0;
	domainMemCurrent    = domainMem;
	domainMemBorder     = domainMem + (DOMAINMEM_SIZEBYTES * jvmConfigData->maxDomains);

	while (domainMemStart < domainMemBorder) {
		char *mem       = domainMemStart;
		d               = (DomainDesc *) (((char *) mem) + DOMAINDESC_OFFSETBYTES);
		d->state        = DOMAIN_STATE_FREE;
		domainMemStart  += DOMAINMEM_SIZEBYTES;
	}

    result = rt_mutex_create(&domainLock, "globalDomainLock");
    if (result < 0) {
        printk(KERN_ERR "Unable to create global domain lock, rc=%d\n", result);
        return;
    }

	domainZero = createDomain("DomainZero", jvmConfigData->heapBytesDom0, -1, -1, NULL, -1, 
                              jvmConfigData->codeBytesDom0, GC_IMPLEMENTATION_DEFAULT, NULL);
	if (domainZero == NULL)
		printk(KERN_ERR "Cannot create domainzero.\n");

	domainZero->memberOfTCB = JNI_TRUE;
}


void deleteDomainSystem(void)
{
	char                *domainMemStart;
	DomainDesc          *d;

	domainMemStart      = domainMem;
    while (domainMemStart < domainMemBorder) {
        char *mem       = domainMemStart;
        d               = (DomainDesc *) (((char *) mem) + DOMAINDESC_OFFSETBYTES);
        rt_mutex_delete(&d->domainMemLock);
        domainMemStart  += DOMAINMEM_SIZEBYTES;
    }

    rt_mutex_delete(&domainLock);
}




//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
// Copyright (C) 1998-2002 Michael Golm
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
// As a special exception, if other files instantiate templates or use macros or 
// inline functions from this file, or you compile this file and link it with other 
// works to produce a work based on this file, this file does not by itself cause 
// the resulting work to be covered by the GNU General Public License. However the 
// source code for this file must still be made available in accordance with 
// section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this
// file might be covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================

