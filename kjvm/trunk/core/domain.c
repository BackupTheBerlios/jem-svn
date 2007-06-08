// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
// 	Copyright (C) 1998-2002 Michael Golm.
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
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "exception_handler.h"
#include "sched.h"

/*==============================================================================
 * Declarations
 *==============================================================================*/

DomainDesc   		*domainZero;
static u32          numberOfDomains = 0;
static u32          currentDomainID = 0;
static char         *domainMem = NULL;
static char         *domainMemBorder = NULL;
static char         *domainMemCurrent = NULL;

#define DOMAINMEM_SIZEBYTES (OBJSIZE_DOMAINDESC*4)
#define DOMAINDESC_OFFSETBYTES (( 2 + XMOFF) *4)


/*==============================================================================
 * Functions
 *==============================================================================*/
DomainDesc *getDomainZero(void)
{
    return domainZero;
}


static DomainDesc *specialAllocDomainDesc(void)
{
    DomainDesc  *d;
    char        *domainMemStart;

	// @aspect Lock

    if (numberOfDomains == getJVMConfig(maxDomains)) {
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
            return NULL;
        }
    } while (d->state != DOMAIN_STATE_FREE);

    memset(d, 0, sizeof(DomainDesc));
    d->magic    	= MAGIC_DOMAIN;
    d->state    	= DOMAIN_STATE_CREATING;
    d->id       	= currentDomainID++;
    numberOfDomains++;

	// @aspect rtdm_mutex_init

    return d;
}


DomainDesc *createDomain(char *domainName, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, jint gcinfo4,
                         u32 code_bytes, int gcImpl)
{
    u8          *mem;
    DomainDesc  *domain;
    u32         libMemSize;

    domain = specialAllocDomainDesc();
    if (domain == NULL) {
        return NULL;
    }

    domain->maxNumberOfLibs = getJVMConfig(maxNumberLibs);
    domain->numberOfLibs    = 0;
    domain->arrayClasses    = NULL;
    domain->scratchMem      = jemMalloc(getJVMConfig(domScratchMemSz) /* MEMTYPE_OTHER */ );
    domain->scratchMemSize  = getJVMConfig(domScratchMemSz);

    domain->cur_code = -1;
    if (code_bytes != -1) {
        domain->code_bytes = code_bytes;
    } else {
        domain->code_bytes = getJVMConfig(codeBytes);
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
    domain->threads         = NULL;
    domain->services[0]     = SERVICE_ENTRY_CHANGING;
    domain->currentThreadID = 0;

    gc_init(domain, mem, gcinfo0, gcinfo1, gcinfo2, gcinfo3, gcinfo4, gcImpl);

    domain->state = DOMAIN_STATE_ACTIVE;

    return domain;
}


int initDomainSystem(void)
{
    DomainDesc          *d;
    char                *domainMemStart;
    int                 result;

    domainMem           = (char *) jemMalloc((DOMAINMEM_SIZEBYTES * getJVMConfig(maxDomains)) /* MEMTYPE_DCB */);
    domainMemStart      = domainMem;
    numberOfDomains     = 0;
    domainMemCurrent    = domainMem;
    domainMemBorder     = domainMem + (DOMAINMEM_SIZEBYTES * getJVMConfig(maxDomains));

    while (domainMemStart < domainMemBorder) {
        char *mem       = domainMemStart;
        d               = (DomainDesc *) (((char *) mem) + DOMAINDESC_OFFSETBYTES);
        d->state        = DOMAIN_STATE_FREE;
	    d->codeBorder	= jemMalloc((sizeof(char *) * getJVMConfig(codeFragments)) /* MEMTYPE_DCB */); 
	    d->code			= jemMalloc((sizeof(char *) * getJVMConfig(codeFragments)) /* MEMTYPE_DCB */); 
	    d->codeTop		= jemMalloc((sizeof(char *) * getJVMConfig(codeFragments)) /* MEMTYPE_DCB */); 
	    d->services		= jemMalloc((sizeof(char *) * getJVMConfig(maxServices)) /* MEMTYPE_DCB */); 
	    d->pools		= jemMalloc((sizeof(char *) * getJVMConfig(maxServices)) /* MEMTYPE_DCB */); 
        domainMemStart  += DOMAINMEM_SIZEBYTES;
    }

	// @aspect rtdm_mutex_init

    domainZero = createDomain("DomainZero", getJVMConfig(heapBytesDom0), -1, -1, NULL, -1,
                              getJVMConfig(codeBytesDom0), GC_IMPLEMENTATION_DEFAULT);
    if (domainZero == NULL)
    {
        printk(KERN_ERR "Cannot create domainzero.\n");
        return -EFAULT;
    }

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
        if (d->state != DOMAIN_STATE_FREE) {
            rt_mutex_delete(&d->domainMemLock);
            rt_mutex_delete(&d->domainHeapLock);
            rt_mutex_delete(&d->gc.gcLock);
        }
	    jemFree(domain->codeBorder);
	    jemFree(domain->code);
	    jemFree(domain->codeTop);
	    jemFree(domain->services);
	    jemFree(domain->pools);
        domainMemStart  += DOMAINMEM_SIZEBYTES;
    }
    
    jemFree(domainMem);

    rt_mutex_delete(&domainLock);
}


int findClassForMethod(MethodDesc * method, JClass **jclass)
{
	int d, l, c, m;
	DomainDesc *domain;
	char *mem;
	int ret = -1;

	if (method == NULL)
		return -1;

	LOCK_DOMAINS;

	for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
		domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
		for (l = 0; l < domain->numberOfLibs; l++) {
			LibDesc *lib = domain->libs[l];
			JClass *allClasses = lib->allClasses;
			for (c = 0; c < lib->numberOfClasses; c++) {
				ClassDesc *classDesc = allClasses[c].classDesc;
				for (m = 0; m < classDesc->numberOfMethods; m++) {
					if (&classDesc->methods[m] == method) {
						*class = &allClasses[c];
						//*class = classDesc;
						ret = 0;
						goto finish;
					}
				}
			}
		}
	}

      finish:
	UNLOCK_DOMAINS;

	return ret;
}


static int findByteCodePosition(MethodDesc * method, u8 * addr)
{
    ByteCodeDesc *table;
    int i, offset, bPos;

    table = method->bytecodeTable;
    offset = addr - (u8 *) (method->code);

    bPos = -1;
    for (i = 0; i < method->numberOfByteCodes; i++) {
        if (table[i].start <= offset && table[i].end >= offset) {
            bPos = table[i].bytecodePos;
            break;
        }
    }

    return bPos;
}


int findMethodAtAddrInDomain(DomainDesc * domain, u8 * addr, MethodDesc ** method, ClassDesc ** classInfo,
                             jint * bytecodePos, jint * lineNumber)
{
    int g, h, i, j, k, l, m;
    int ret = -1;
    SourceLineDesc *stable;
    if (addr == NULL)
        return -1;

    rt_mutex_acquire(&domainLock, TM_INFINITE);
    for (h = 0; h < domain->numberOfLibs; h++) {
        LibDesc *lib = domain->libs[h];
        JClass *allClasses = lib->allClasses;
        ClassDesc *classDesc;
        u8 *lower, *upper=NULL;
        if (lib->numberOfClasses <= 0) {
            printk(KERN_ERR "Problem in domain, %d, empty lib\n", domain->id);
            return -1;
        }
        if (lib->hasNoImplementations)
            continue;
        for (i = 0; i < lib->numberOfClasses; i++) {
            classDesc = lib->allClasses[i].classDesc;
            if ((classDesc->classType & CLASSTYPE_INTERFACE) == CLASSTYPE_INTERFACE)
                continue;
            for (j = 0; j < classDesc->numberOfMethods; j++) {
                lower = (u8 *) (classDesc->methods[j].code);
                if (lower)
                    goto low_class;
            }
        }
        if (i == lib->numberOfClasses) {
            lib->hasNoImplementations = JNI_TRUE;
            continue;
        }
        low_class:
        for (g = lib->numberOfClasses - 1; g >= i; g--) {
            classDesc = lib->allClasses[g].classDesc;
            if ((classDesc->classType & CLASSTYPE_INTERFACE) == CLASSTYPE_INTERFACE)
                continue;
            for (j = classDesc->numberOfMethods - 1; j >= 0; j--) {
                upper = ((u8 *) classDesc->methods[j].code) + classDesc->methods[j].numberOfCodeBytes;
                if (upper)
                    goto upp_class;
            }
        }
        upp_class:
        if ((addr < lower) || (addr >= upper))
            continue;
        for (; i <= g; i++) {
            classDesc = allClasses[i].classDesc;
            if ((classDesc->classType & CLASSTYPE_INTERFACE) == CLASSTYPE_INTERFACE)
                continue;
            for (l = 0; l < classDesc->numberOfMethods; l++) {
                lower = (u8 *) (classDesc->methods[l].code);
                if (lower)
                    break;
            }
            for (m = classDesc->numberOfMethods - 1; m >= l; m--) {
                upper = ((u8 *) classDesc->methods[m].code) + classDesc->methods[m].numberOfCodeBytes;
                if (upper)
                    break;
            }
            if ((addr < lower) || (addr >= upper))
                continue;
            l = 0;
            m = classDesc->numberOfMethods - 1;
            for (j = l; j <= m; j++) {
                if (classDesc->methods[j].code <= (code_t) addr
                    && classDesc->methods[j].code + classDesc->methods[j].numberOfCodeBytes > (code_t) addr) {
                    *method = &classDesc->methods[j];
                    *classInfo = classDesc;

                    *bytecodePos = findByteCodePosition(&(classDesc->methods[j]), addr);

                    *lineNumber = -1;
                    if (*bytecodePos != -1) {
                        /* find source code line */
                        stable = classDesc->methods[j].sourceLineTable;
                        for (k = 0; k < classDesc->methods[j].numberOfSourceLines - 1; k++) {
                            if ((stable[k].startBytecode <= *bytecodePos)
                                && (stable[k + 1].startBytecode > *bytecodePos)) {
                                *lineNumber = stable[k].lineNumber;
                                break;
                            }
                        }
                    }
                    ret = 0;
                    goto finish;
                }
            }
        }
    }

    finish:
    rt_mutex_release(&domainLock);

    return ret;
}


/* -1 failure,
   0 success */
int findMethodAtAddr(u8 * addr, MethodDesc ** method, ClassDesc ** classInfo, jint * bytecodePos, jint * lineNumber)
{
    DomainDesc *domain;
    char *mem;
    int ret = -1;

    if (addr == NULL)
        return -1;

    rt_mutex_acquire(&domainLock, TM_INFINITE);

    for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
        domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
        if (findMethodAtAddrInDomain(domain, addr, method, classInfo, bytecodePos, lineNumber) == 0) {
            ret = 0;
            goto finish;
        }
    }

    finish:
    rt_mutex_release(&domainLock);
    return ret;
}


int findProxyCode(DomainDesc * domain, char *addr, char **method, char **sig, ClassDesc ** classInfo)
{
    char *mem;
    int ret = -1;

    rt_mutex_acquire(&domainLock, TM_INFINITE);

    for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
        domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
        if (findProxyCodeInDomain(domain, addr, method, sig, classInfo) == 0) {
            ret = 0;
            break;
        }
    }

    rt_mutex_release(&domainLock);
    return ret;
}


DomainDesc *findDomain(u32 id)
{
    DomainDesc *domain;
    char *mem;
    DomainDesc *ret = NULL;

    rt_mutex_acquire(&domainLock, TM_INFINITE);

    for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
        domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
        if (domain->state == DOMAIN_STATE_FREE)
            continue;
        if (domain->id == id) {
            ret = domain;
            break;
        }
    }

    rt_mutex_release(&domainLock);
    return ret;
}


DomainDesc *findDomainByName(char *name)
{
    DomainDesc *domain;
    char *mem;
    DomainDesc *ret = NULL;

    rt_mutex_acquire(&domainLock, TM_INFINITE);

    for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
        domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
        if (domain->state == DOMAIN_STATE_FREE)
            continue;
        if (domain->domainName && strcmp(domain->domainName, name) == 0) {
            ret = domain;
            break;
        }
    }

    rt_mutex_release(&domainLock);
    return ret;
}


void foreachDomain(domain_f func)
{
    DomainDesc *domain;
    char *mem;

    rt_mutex_acquire(&domainLock, TM_INFINITE);

    for (mem = domainMem; mem < domainMemBorder; mem += DOMAINMEM_SIZEBYTES) {
        domain = (DomainDesc *) (mem + DOMAINDESC_OFFSETBYTES);
        if (domain->state != DOMAIN_STATE_ACTIVE)
            continue;
        func(domain);
    }

    rt_mutex_release(&domainLock);
}


jint getNumberOfDomains()
{
    return numberOfDomains;
}


void domain_panic(DomainDesc * domain, char *msg)
{
    printk(KERN_ERR "DOMAIN PANIC: \n");
    if (domain != NULL)
        printk(KERN_ERR "   in domain %d\n", domain->id);
    printk(KERN_ERR "%s\n", msg);

    terminateDomain(domain);
}

/* only a thread outside the domain can terminate the domain */
void terminateDomain(DomainDesc * domain)
{
    u32 index, j, size;
    ThreadDesc *t;

    domain->state = DOMAIN_STATE_TERMINATING;

    /* deactivate services */
    rt_mutex_acquire(&svTableLock, TM_INFINITE);
    for (index = 0; index < CONFIG_JEM_MAX_SERVICES; index++) {
        if (domain->services[index] != SERVICE_ENTRY_FREE) {
            domain->services[index] = SERVICE_ENTRY_CHANGING;
            break;
        }
    }
    rt_mutex_release(&svTableLock);

    /* terminate all pending portal calls */
    /* and return from all running service executions with an exception */

    for (t = domain->threads; t != NULL; t = t->nextInDomain) {
        if (t->blockedInDomain != NULL) {
            DEPDesc *svc = t->blockedInDomain->services[t->blockedInServiceIndex];
            if (t->state == STATE_PORTAL_WAIT_FOR_RCV) {
                portal_remove_sender(svc, t);
            } else if (t->state == STATE_PORTAL_WAIT_FOR_RET) {
                portal_abort_current_call(svc, t);
            } else {
                printk(KERN_WARNING "Domain terminate with pending call not yet supported\n");
            }
        }
        if (t->mostRecentlyCalledBy != NULL) {
            /* called by domain */
            /* throw exception */
            ThreadDesc *source = t->mostRecentlyCalledBy;
            source->portalReturn = (struct ObjectDesc_s *) THROW_DomainTerminatedException;
            source->portalReturnType = PORTAL_RETURN_TYPE_EXCEPTION;
            Sched_portal_handoff_to_sender(source);
        }
    }

    /* free all TCBs and stacks */
    for (t = domain->threads; t != NULL;) {
        ThreadDesc *tnext = t->nextInDomain;
        terminateThread(t);
        t = tnext;
    }

    /* free unshared code segments */
    for (j = 0; j < domain->cur_code + 1; j++) {
        size = (char *) (domain->codeBorder[j]) - (char *) (domain->code[j]);
        jemFreeCode(domain->code[j]);
        domain->code[j] = domain->codeBorder[j] = domain->codeTop[j] = NULL;
    }

    gc_done(domain);

    jemFree(domain->scratchMem);

    rt_mutex_delete(&domain->domainMemLock);
    rt_mutex_delete(&domain->domainHeapLock);
    rt_mutex_delete(&domain->gc.gcLock);
    rt_sem_delete(&domain->serviceSem);

    domain->state = DOMAIN_STATE_FREE;  /* domain control block can be reused */
    numberOfDomains--;
}


