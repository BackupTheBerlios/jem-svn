//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 2007 JemStone Software LLC. All rights reserved.
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
//=================================================================================
// File: malloc.c
//
// Jem/JVM memory allocation implementation.
//
//===========================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/queue.h>
#include <native/heap.h>
#include "jemtypes.h"
#include "malloc.h"
#include "messages.h"
#include "jemUPipe.h"
#include "domain.h"
#include "gc.h"
#include "load.h"
#include "zero.h"

#define MEMTYPE_PARAM , memtype


static RT_HEAP      jemHeap;
unsigned int        jemHeapSz=0;
static unsigned int allocRam = 0;

struct alloc_stat_s {
    u32 heap;
    u32 code;
    u32 stack;
    u32 memobj;
    u32 profiling;
    u32 emulation;
    u32 tmp;
    u32 dcb;
    u32 other;
};
static struct alloc_stat_s alloc_stat;


int jemMallocInit(void)
{
    int     result;

    if ((result = rt_heap_create(&jemHeap, "jemHeap", jemHeapSz, H_PRIO|H_MAPPABLE)) < 0) {
        printk(KERN_ERR "Can not create Jem/JVM heap of size %d, code=%d.\n", jemHeapSz, result);
        return result;
    }

    return 0;
}


int jemDestroyHeap(void)
{
    return rt_heap_delete(&jemHeap);
}


static void *jemMallocInternal(u32 size, u32 ** start MEMTYPE_INFO)
{
    void    *addr = NULL;

    if (rt_heap_alloc(&jemHeap, size, TM_NONBLOCK, &addr)) {
        return NULL;
    }

    allocRam += size;

    switch (memtype) {
    case MEMTYPE_HEAP_PARAM:
        alloc_stat.heap += size;
        break;
    case MEMTYPE_CODE_PARAM:
        alloc_stat.code += size;
        break;
    case MEMTYPE_MEMOBJ_PARAM:
        alloc_stat.memobj += size;
        break;
    case MEMTYPE_PROFILING_PARAM:
        alloc_stat.profiling += size;
        break;
    case MEMTYPE_STACK_PARAM:
        alloc_stat.stack += size;
        break;
    case MEMTYPE_EMULATION_PARAM:
        alloc_stat.emulation += size;
        break;
    case MEMTYPE_TMP_PARAM:
        alloc_stat.tmp += size;
        break;
    case MEMTYPE_DCB_PARAM:
        alloc_stat.dcb += size;
        break;
    case MEMTYPE_OTHER_PARAM:
        alloc_stat.other += size;
        break;
    default:
        printk(KERN_WARNING "Allocation MEMTYPE unknown.\n");
        alloc_stat.other += size;
    }

    memset(addr, 0, size);

    *start = addr;
    return addr;
}


void *jemMalloc(u32 size MEMTYPE_INFO)
{
    u32 *start;
    return jemMallocInternal(size, &start MEMTYPE_PARAM);
}


void jemFree(void *addr, u32 size MEMTYPE_INFO)
{

    switch (memtype) {
    case MEMTYPE_HEAP_PARAM:
        alloc_stat.heap -= size;
        break;
    case MEMTYPE_CODE_PARAM:
        alloc_stat.code -= size;
        break;
    case MEMTYPE_MEMOBJ_PARAM:
        alloc_stat.memobj -= size;
        break;
    case MEMTYPE_PROFILING_PARAM:
        alloc_stat.profiling -= size;
        break;
    case MEMTYPE_STACK_PARAM:
        alloc_stat.stack -= size;
        break;
    case MEMTYPE_EMULATION_PARAM:
        alloc_stat.emulation -= size;
        break;
    case MEMTYPE_TMP_PARAM:
        alloc_stat.tmp -= size;
        break;
    case MEMTYPE_DCB_PARAM:
        alloc_stat.dcb -= size;
        break;
    case MEMTYPE_OTHER_PARAM:
        alloc_stat.other -= size;
        break;
    default:
        printk(KERN_WARNING "Free MEMTYPE unknown\n");
    }

    allocRam -= size;
    rt_heap_free(&jemHeap, addr);
}


char *jemMallocCode(DomainDesc *domain, u32 size)
{
    char    *data;
    char    *nextObj;
    u32     c;
    u32     chunksize = domain->code_bytes;

    int result = rt_mutex_acquire(&domain->domainMemLock, TM_INFINITE);
    if (result < 0) {
        printk(KERN_ERR "Error acquiring domain memory lock, code=%d\n", result);
        return NULL;
    }

    if (domain->cur_code == -1) {
        domain->code[0] = (char *) jemMalloc(chunksize MEMTYPE_CODE);
        domain->codeBorder[0] = domain->code[0] + chunksize;
        domain->codeTop[0] = domain->code[0];
        domain->cur_code = 0;
    }

    c       = domain->cur_code;
    nextObj = domain->codeTop[c] + size;

    if (nextObj > domain->codeBorder[c]) {
        c++;
        if (c == CONFIG_JEM_CODE_FRAGMENTS) {
            printk(KERN_ERR "Out of code space for domain.\n");
            rt_mutex_release(&domain->domainMemLock);
            return NULL;
        }
        domain->code[c]         = (char *) jemMalloc(chunksize MEMTYPE_CODE);
        domain->codeBorder[c]   = domain->code[c] + chunksize;
        domain->codeTop[c]      = domain->code[c];
        domain->cur_code        = c;
        nextObj                 = domain->codeTop[c] + size;
        if (nextObj > domain->codeBorder[c]) {
            printk(KERN_ERR "Can`t allocate %i byte for code space\n", size);
            rt_mutex_release(&domain->domainMemLock);
            return NULL;
        }
    }

    data                = domain->codeTop[c];
    domain->codeTop[c]  = nextObj;

    rt_mutex_release(&domain->domainMemLock);

    return data;
}


ThreadDescProxy *jemMallocThreadDescProxy(ClassDesc * c)
{
    ThreadDescProxy     *proxy;

    if ((proxy = jemMalloc(sizeof(ThreadDescProxy) MEMTYPE_DCB)) == NULL) return NULL;
    if (c != NULL)
        proxy->vtable = c->vtable;
    else
        proxy->vtable = NULL;   /* bootstrap of DomainZero */
    return proxy;
}


void jemFreeThreadDesc(ThreadDesc *t)
{
    ThreadDescProxy *tpxy    = (ThreadDescProxy *) ThreadDesc2ObjectDesc(t);
    jemFree(tpxy, sizeof(ThreadDescProxy) MEMTYPE_DCB);
}


JClass *jemMallocClasses(DomainDesc * domain, u32 number)
{
    return (JClass *) jemMallocCode(domain, sizeof(JClass) * number);
}

ClassDesc *jemMallocPrimitiveclassdesc(DomainDesc * domain, u32 namelen)
{
    char *m                 = jemMallocCode(domain, sizeof(ClassDesc) + namelen);
    ClassDesc *cd  = (ClassDesc *) m;
    memset(m, 0, sizeof(ClassDesc) + namelen);
    cd->name = m + sizeof(ClassDesc);
    return cd;
}

ClassDesc *jemMallocArrayclassdesc(DomainDesc * domain, u32 namelen)
{
    char *m = jemMallocCode(domain, sizeof(ClassDesc) + namelen + (11 + 1) * 4);    // vtable
    ClassDesc *cd = (ClassDesc *) m;
    memset(m, 0, sizeof(ClassDesc) + namelen);
    cd->name = m + sizeof(ClassDesc);
    cd->vtable = (code_t *) m + sizeof(ClassDesc) + namelen + 4 /* classptr at negative offset */ ;
    return cd;
}

TempMemory *jemMallocTmp(u32 size)
{
    TempMemory *t;
    char *m;
    size += sizeof(TempMemory);
    m = jemMalloc(size MEMTYPE_TMP);
    t = (TempMemory *) m;
    t->size = size;
    t->start = m;
    t->free = m + (sizeof(TempMemory));
    t->border = m + size;
    return t;
}

void jemFreeTmp(TempMemory * m)
{
    jemFree(m->start, m->size MEMTYPE_TMP);
}

LibDesc *jemMallocLibdesc(DomainDesc * domain)
{
    return (LibDesc *) jemMallocCode(domain, sizeof(LibDesc));
}

static u32 sharedLibID = 1;

SharedLibDesc *jemMallocSharedlibdesc(DomainDesc * domain, u32 namelen)
{
    char *m = jemMallocCode(domain, sizeof(SharedLibDesc) + namelen);
    SharedLibDesc *sl = (SharedLibDesc *) m;
    memset(m, 0, sizeof(SharedLibDesc) + namelen);
    sl->name = m + sizeof(SharedLibDesc);   /* name allocated immediately after LibDesc */
    sl->id = sharedLibID++;
    return sl;
}

SharedLibDesc **jemMallocSharedlibdesctable(DomainDesc * domain, u32 number)
{
    return (SharedLibDesc **) jemMallocCode(domain, number * sizeof(SharedLibDesc *));
}

struct meta_s *jemMallocMetatable(DomainDesc * domain, u32 number)
{
    return (struct meta_s *) jemMallocCode(domain, number * sizeof(struct meta_s));
}


ClassDesc *jemMallocClassdescs(DomainDesc * domain, u32 number)
{
    return (ClassDesc *) jemMallocCode(domain, sizeof(ClassDesc) * number);
}

ClassDesc **jemMallocClassdesctable(DomainDesc * domain, u32 number)
{
    return (ClassDesc **) jemMallocCode(domain, number * sizeof(ClassDesc *));
}

char **jemMallocTmpStringtable(DomainDesc * domain, TempMemory * mem, u32 number)
{
    char *m;
    char *n;
    number *= 4;
    n = mem->free + number;
    if (n > mem->border) {
        printk(KERN_ERR "Temp space not sufficient\n");
        return NULL;
    }
    m = mem->free;
    mem->free = n;
    return (char **) m;
}

MethodDesc *jemMallocMethoddesc(DomainDesc * domain)
{
    return (MethodDesc *) jemMallocCode(domain, sizeof(MethodDesc));
}

MethodDesc *jemMallocMethoddescs(DomainDesc * domain, u32 number)
{
    return (MethodDesc *) jemMallocCode(domain, sizeof(MethodDesc) * number);
}

ExceptionDesc *jemMallocExceptiondescs(DomainDesc * domain, u32 number)
{
    char *m = jemMallocCode(domain, sizeof(ExceptionDesc) * number);
    return (ExceptionDesc *) m;
}

char *jemMallocString(DomainDesc * domain, u32 len)
{
    return (char *) jemMallocCode(domain, (len + 1) * sizeof(char));
}

char *jemMallocStaticsmap(DomainDesc * domain, u32 size)
{
    return (char *) jemMallocCode(domain, size);
}

char *jemMallocObjectmap(DomainDesc * domain, u32 size)
{
    return (char *) jemMallocCode(domain, size);
}

char *jemMallocArgsmap(DomainDesc * domain, u32 size)
{
    return (char *) jemMallocCode(domain, size);
}


SymbolDesc **jemMallocSymboltable(DomainDesc * domain, u32 len)
{
    return (SymbolDesc **) jemMallocCode(domain, sizeof(SymbolDesc *) * len);
}

SymbolDesc *jemMallocSymbol(DomainDesc * domain, u32 size)
{
    return (SymbolDesc *) jemMallocCode(domain, size);
}

char *jemMallocStackmap(DomainDesc * domain, u32 size)
{
    return (char *) jemMallocCode(domain, size);
}

char *jemMallocProxycode(DomainDesc * domain, u32 size)
{
    return (char *) jemMallocCode(domain, size);
}

char *jemMallocCpudesc(DomainDesc * domain, u32 size)
{       
    return (char *) jemMallocCode(domain, size);
}

ByteCodeDesc *jemMmallocBytecodetable(DomainDesc * domain, u32 len)
{
    return (ByteCodeDesc *) jemMallocCode(domain, sizeof(ByteCodeDesc) * len);
}

SourceLineDesc *jemMallocSourcelinetable(DomainDesc * domain, u32 len)
{
    return (SourceLineDesc *) jemMallocCode(domain, sizeof(SourceLineDesc) * len);
}

u32 *jemMallocStaticfields(DomainDesc * domain, u32 number)
{
    return (u32 *) jemMallocCode(domain, number * 4);
}

FieldDesc *jemMallocFielddescs(DomainDesc * domain, u32 number)
{
    return (FieldDesc *) jemMallocCode(domain, sizeof(FieldDesc) * number);
}

char **jemMallocVtableSym(DomainDesc * domain, u32 vtablelen)
{
    return (char **) jemMallocCode(domain, vtablelen * 3 * sizeof(char *));
}

MethodDesc *jemMallocMethods(DomainDesc * domain, u32 number)
{
    return (MethodDesc *) jemMallocCode(domain, number * sizeof(MethodDesc));
}

MethodDesc **jemMallocMethodVtable(DomainDesc * domain, u32 number)
{
    return (MethodDesc **) jemMallocCode(domain, number * sizeof(MethodDesc *));
}

u8 *jemMallocNativecode(DomainDesc * domain, u32 size)
{
    /* align code to 32 bit or 4 bytes */
    return (u8 *) (((u32) jemMallocCode(domain, size + 4) + 3) & (u32) ~ 0x3);
}


struct nameValue_s *jemMallocDomainzeroNamevalue(u32 namelen)
{
    char *m = jemMallocCode(domainZero, sizeof(struct nameValue_s) + namelen);
    struct nameValue_s *s = (struct nameValue_s *) m;
    s->name = m + sizeof(struct nameValue_s);
    return s;
}

code_t *jemMallocVtable(DomainDesc * domain, u32 number)
{
    return (code_t *) jemMallocCode(domain, number * sizeof(code_t));
}

