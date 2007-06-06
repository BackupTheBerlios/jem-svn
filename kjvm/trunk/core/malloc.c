// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
// 	Copyright (C) 1998-2002 Michael Golm.
//==============================================================================
//
// Jem/JVM memory allocation implementation.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "malloc.h"
#include "domain.h"
#include "gc.h"
#include "load.h"
#include "zero.h"
// @aspect include

/*==============================================================================
 * Declarations
 *==============================================================================*/

// TEMP TEMP TEMP
DomainDesc   		*domainZero;


/*==============================================================================
 * Functions
 *==============================================================================*/
int jemMallocInit(void)
{
	// @aspect begin
    return 0;
}


void *jemMalloc(u32 size)
{
	// @aspect begin
    return kmalloc(size, GFP_KERNEL);
}


void jemFree(void *addr)
{
	// @aspect begin
    kfree(addr);
}


char *jemMallocCode(DomainDesc *domain, u32 size)
{
    char    *data;
    char    *nextObj;
    u32     c;
    u32     chunksize = domain->code_bytes;

	// @aspect Lock

    if (domain->cur_code == -1) {
    	// @aspect MallocStatistics    
        domain->code[0] = (char *) vmalloc(chunksize);
        domain->codeBorder[0] = domain->code[0] + chunksize;
        domain->codeTop[0] = domain->code[0];
        domain->cur_code = 0;
    }

    c       = domain->cur_code;
    nextObj = domain->codeTop[c] + size;

    if (nextObj > domain->codeBorder[c]) {
        c++;
        if (c == getJVMConfig()->codeFragments) {
            printk(KERN_ERR "Out of code space for domain.\n");
            return NULL;
        }
    	// @aspect MallocStatistics    
        domain->code[c]         = (char *) vmalloc(chunksize);
        domain->codeBorder[c]   = domain->code[c] + chunksize;
        domain->codeTop[c]      = domain->code[c];
        domain->cur_code        = c;
        nextObj                 = domain->codeTop[c] + size;
        if (nextObj > domain->codeBorder[c]) {
            printk(KERN_ERR "Can`t allocate %i byte for code space\n", size);
            return NULL;
        }
    }

    data                = domain->codeTop[c];
    domain->codeTop[c]  = nextObj;

    return data;
}


void jemFreeCode(void *addr)
{
	// @aspect begin
    vfree(addr);
}


ThreadDescProxy *jemMallocThreadDescProxy(ClassDesc * c)
{
    ThreadDescProxy     *proxy;

    if ((proxy = jemMalloc(sizeof(ThreadDescProxy) /* MEMTYPE_DCB */)) == NULL) return NULL;
    if (c != NULL)
        proxy->vtable = c->vtable;
    else
        proxy->vtable = NULL;   /* bootstrap of DomainZero */
    return proxy;
}


void jemFreeThreadDesc(ThreadDesc *t)
{
    ThreadDescProxy *tpxy    = (ThreadDescProxy *) ThreadDesc2ObjectDesc(t);
    jemFree(tpxy);
}


JClass *jemMallocClasses(DomainDesc * domain, u32 number)
{
    return (JClass *) jemMallocCode(domain, sizeof(JClass) * number);
}

ClassDesc *jemMallocPrimitiveclassdesc(DomainDesc * domain, u32 namelen)
{
    char *m      	= jemMallocCode(domain, sizeof(ClassDesc) + namelen);
    ClassDesc *cd  	= (ClassDesc *) m;
    memset(m, 0, sizeof(ClassDesc) + namelen);
    cd->name 		= m + sizeof(ClassDesc);
    return cd;
}

ClassDesc *jemMallocArrayclassdesc(DomainDesc * domain, u32 namelen)
{
    char *m 		= jemMallocCode(domain, sizeof(ClassDesc) + namelen + (11 + 1) * 4);    // vtable
    ClassDesc *cd 	= (ClassDesc *) m;
    memset(m, 0, sizeof(ClassDesc) + namelen);
    cd->name 		= m + sizeof(ClassDesc);
    cd->vtable 		= (code_t *) m + sizeof(ClassDesc) + namelen + 4 /* classptr at negative offset */ ;
    return cd;
}

TempMemory *jemMallocTmp(u32 size)
{
    TempMemory *t;
    char *m;
    size += sizeof(TempMemory);
    m = jemMalloc(size /* MEMTYPE_TMP */);
    t = (TempMemory *) m;
    t->size = size;
    t->start = m;
    t->free = m + (sizeof(TempMemory));
    t->border = m + size;
    return t;
}

void jemFreeTmp(TempMemory * m)
{
    jemFree(m->start);
}

LibDesc *jemMallocLibdesc(DomainDesc * domain)
{
    return (LibDesc *) jemMallocCode(domain, sizeof(LibDesc));
}

static u32 sharedLibID = 1;

SharedLibDesc *jemMallocSharedlibdesc(DomainDesc * domain, u32 namelen)
{
    char *m 			= jemMallocCode(domain, sizeof(SharedLibDesc) + namelen);
    SharedLibDesc *sl 	= (SharedLibDesc *) m;
    memset(m, 0, sizeof(SharedLibDesc) + namelen);
    sl->name 	= m + sizeof(SharedLibDesc);   /* name allocated immediately after LibDesc */
    sl->id 		= sharedLibID++;
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

ClassDesc *jemMallocClassdesc(DomainDesc * domain, u32 namelen)
{
	char *m = jemMallocCode(domain, sizeof(ClassDesc) + namelen);
	ClassDesc *cd = (ClassDesc *) m;
	memset(m, 0, sizeof(ClassDesc) + namelen);
	cd->name = m + sizeof(ClassDesc);
	return cd;
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
