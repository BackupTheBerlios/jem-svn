//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright � 2007 JemStone Software LLC. All rights reserved.
// Copyright � 1997-2001 The JX Group. All rights reserved.
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
// File: malloc.h
//
// Jem/JVM memory allocation interface.
//
//===========================================================================

#ifndef _MALLOC_H
#define _MALLOC_H

#include <linux/types.h>
#include "code.h"
#include "thread.h"

#define MEMTYPE_OTHER  ,0
#define MEMTYPE_HEAP   ,1
#define MEMTYPE_STACK  ,2
#define MEMTYPE_MEMOBJ ,3
#define MEMTYPE_CODE   ,4
#define MEMTYPE_PROFILING   ,5
#define MEMTYPE_EMULATION   ,6
#define MEMTYPE_TMP   ,7
#define MEMTYPE_DCB   ,8
#define MEMTYPE_INFO , int memtype
#define MEMTYPE_OTHER_PARAM  0
#define MEMTYPE_HEAP_PARAM   1
#define MEMTYPE_STACK_PARAM 2
#define MEMTYPE_MEMOBJ_PARAM 3
#define MEMTYPE_CODE_PARAM   4
#define MEMTYPE_PROFILING_PARAM   5
#define MEMTYPE_EMULATION_PARAM   6
#define MEMTYPE_TMP_PARAM   7
#define MEMTYPE_DCB_PARAM   8

typedef struct TempMemory_s {
	u32     size;
	char    *free;
	char    *start;
	char    *border;
} TempMemory;


int                 jemMallocInit(void);
int                 jemDestroyHeap(void);
void                *jemMalloc(u32 size MEMTYPE_INFO);
char                *jemMallocCode(struct DomainDesc_s *domain, u32 size);
void                jemFree(void *addr, u32 size MEMTYPE_INFO);
void                jemFreeThreadDesc(ThreadDesc *t);
ThreadDescProxy     *jemMallocThreadDescProxy(ClassDesc * c);
JClass              *jemMallocClasses(DomainDesc * domain, u32 number);
ClassDesc           *jemMallocPrimitiveclassdesc(DomainDesc * domain, u32 namelen);
ClassDesc           *jemMallocArrayclassdesc(DomainDesc * domain, u32 namelen);
TempMemory          *jemMallocTmp(u32 size);
void                jemFreeTmp(TempMemory * m);
LibDesc             *jemMallocLibdesc(DomainDesc * domain);
SharedLibDesc       *jemMallocSharedlibdesc(DomainDesc * domain, u32 namelen);
SharedLibDesc       **jemMallocSharedlibdesctable(DomainDesc * domain, u32 number);
struct meta_s       *jemMallocMetatable(DomainDesc * domain, u32 number);
ClassDesc           **jemMallocClassdesctable(DomainDesc * domain, u32 number);
char                **jemMallocTmpStringtable(DomainDesc * domain, TempMemory * mem, u32 number);
MethodDesc          *jemMallocMethoddesc(DomainDesc * domain);
MethodDesc          *jemMallocMethoddescs(DomainDesc * domain, u32 number);
ExceptionDesc       *jemMallocExceptiondescs(DomainDesc * domain, u32 number);
ClassDesc           *jemMallocClassdescs(DomainDesc * domain, u32 number);
char                *jemMallocString(DomainDesc * domain, u32 len);
char                *jemMallocStaticsmap(DomainDesc * domain, u32 size);
char                *jemMallocObjectmap(DomainDesc * domain, u32 size);
char                *jemMallocArgsmap(DomainDesc * domain, u32 size);
SymbolDesc          **jemMallocSymboltable(DomainDesc * domain, u32 len);
SymbolDesc          *jemMallocSymbol(DomainDesc * domain, u32 size);
char                *jemMallocStackmap(DomainDesc * domain, u32 size);
char                *jemMallocProxycode(DomainDesc * domain, u32 size);
char                *jemMallocCpudesc(DomainDesc * domain, u32 size);
ByteCodeDesc        *jemMmallocBytecodetable(DomainDesc * domain, u32 len);
SourceLineDesc      *jemMallocSourcelinetable(DomainDesc * domain, u32 len);
u32                 *jemMallocStaticfields(DomainDesc * domain, u32 number);
FieldDesc           *jemMallocFielddescs(DomainDesc * domain, u32 number);
char                **jemMallocVtableSym(DomainDesc * domain, u32 vtablelen);
MethodDesc          *jemMallocMethods(DomainDesc * domain, u32 number);
MethodDesc          **jemMallocMethodVtable(DomainDesc * domain, u32 number);
u8                  *jemMallocNativecode(DomainDesc * domain, u32 size);
code_t              *jemMallocVtable(DomainDesc * domain, u32 number);
struct nameValue_s  *jemMallocDomainzeroNamevalue(u32 namelen);

#endif

