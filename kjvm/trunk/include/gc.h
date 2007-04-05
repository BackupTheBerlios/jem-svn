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
// File: gc.h
//
// Jem/JVM garbage collector interface.
//
//===========================================================================

#ifndef _GC_H
#define _GC_H

#include <linux/types.h>
#include "jemConfig.h"
#include "domain.h"

#define GC_IMPLEMENTATION_NEW        0
#define GC_IMPLEMENTATION_COMPACTING 1
#define GC_IMPLEMENTATION_BITMAP     2
#define GC_IMPLEMENTATION_CHUNKED    3
#define GC_IMPLEMENTATION_DEFAULT    GC_IMPLEMENTATION_NEW 
#define OBJFLAGS_OBJECT               0x00000002
#define OBJFLAGS_PORTAL               0x00000004
#define OBJFLAGS_MEMORY               0x00000006
#define OBJFLAGS_XXXXXXXXX            0x00000008
#define OBJFLAGS_ATOMVAR              0x0000000a
#define OBJFLAGS_SERVICE              0x0000000c
#define OBJFLAGS_CPUSTATE             0x0000000e
#define OBJFLAGS_FOREIGN_CPUSTATE     0x00000010
#define OBJFLAGS_XXXXXXXXXX           0x00000012
#define OBJFLAGS_EXTERNAL_CPUDESC     0x00000014
#define OBJFLAGS_ARRAY                0x00000016
#define OBJFLAGS_EXTERNAL_STRING      0x00000018
#define OBJFLAGS_CAS                  0x0000001a
#define OBJFLAGS_EXTERNAL_METHOD      0x0000001c
#define OBJFLAGS_EXTERNAL_CLASS       0x0000001e
#define OBJFLAGS_INTERCEPTINBOUNDINFO 0x00000020
#define OBJFLAGS_INTERCEPTPORTALINFO  0x00000022
#define OBJFLAGS_VMOBJECT             0x00000024
#define OBJFLAGS_CREDENTIAL           0x00000026
#define OBJFLAGS_DOMAIN               0x00000028
#define OBJFLAGS_SERVICE_POOL         0x0000002a
#define OBJFLAGS_MAPPED_MEMORY        0x0000002c
#define OBJFLAGS_STACK                0x0000002e
#define FLAGS_MASK        0x000000fe
#define XMONE XMOFF
#define XMZERO 0

void gc_init(struct DomainDesc_s *domain, u8 *memu, jint gcinfo0, jint gcinfo1, jint gcinfo2, char *gcinfo3, 
             jint gcinfo4, int gcImpl);
void gc_done(struct DomainDesc_s *domain);
u32 gc_mem(void);

static inline u32 *ObjectDesc2ptr(struct ObjectDesc_s * ref)
{
    return((u32 *) ref) - 1 - XMOFF;
}

static inline struct ObjectDesc_s *ptr2ObjectDesc(u32 * ptr) {
    return(struct ObjectDesc_s *) (ptr + 1 + XMOFF);
}

static inline struct ObjectDesc_s *DomainDesc2ObjectDesc(struct DomainDesc_s * domain) {
    return(struct ObjectDesc_s *) (((u32 *) domain) - 1);
}

static inline struct ObjectDesc_s *CPUDesc2ObjectDesc(struct CPUDesc_s * cpu) {
    return(struct ObjectDesc_s *) (((u32 *) cpu) - 1);
}

static inline struct ObjectDesc_s *ThreadDesc2ObjectDesc(struct ThreadDesc_s * thread) {
    return(struct ObjectDesc_s *) (((u32 *) thread) - 1);
}

static inline u32 getObjFlags(struct ObjectDesc_s * ref)
{
    return *(((u32 *) ref) - 1 - XMONE);
}

static inline void setObjFlags(struct ObjectDesc_s * ref, u32 flags)
{
    *(((u32 *) ref) - 1 - XMONE) = flags;
}

#define getObjMagic(ref) (*(((u32*)ref) - 1 - XMZERO))

static inline void setObjMagic(struct ObjectDesc_s * ref, u32 magic)
{
    *(((u32 *) ref) - 1 - XMZERO) = magic;
}

static inline u32 gc_freeWords(struct DomainDesc_s * domain)
{
    return domain->gc.freeWords(domain);
    //return 0;
}

static inline u32 gc_totalWords(struct DomainDesc_s * domain)
{
    return domain->gc.totalWords(domain);
    //return 0;
}

void gc_printInfo(struct DomainDesc_s * domain);

jboolean gc_checkHeap(struct DomainDesc_s * domain, jboolean invalidate);
void gc_findOnHeap(struct DomainDesc_s * domain, char *classname);


#define OBJSIZE_ARRAY \
      XMOFF /* magic */\
      + 1 /* flags at neg index */\
      + 1 /* vtable (arrays are objects!) */\
      + 1 /* size */\
      + 1 /* elemClass pointer */

#define OBJSIZE_ARRAY_8BIT(size) \
      ((((size)+3)/4) \
       + OBJSIZE_ARRAY)

#define OBJSIZE_ARRAY_16BIT(size) \
      ((((size)+1)/2) \
       + OBJSIZE_ARRAY)

#define OBJSIZE_ARRAY_32BIT(size) \
      ((size) \
       + OBJSIZE_ARRAY)

#define OBJSIZE_ARRAY_64BIT(size) \
      (((size)*2) \
       + OBJSIZE_ARRAY)

#define OBJSIZE_OBJECT(size) \
      (size \
      + XMOFF /* magic */\
      + 1 /* flags at neg index */\
      + 1 /* vtable  */\
      + 1 /* size */)

#define OBJSIZE_STACK(size) \
      (size \
      + XMOFF /* magic */\
      + 1 /* flags at neg index */\
      + 2 /* size field, tcb field */\
      + 1 /* vtable  */)

#define OBJSIZE_PORTAL \
      ( XMOFF /* magic */\
          + 1 /* flags at neg */\
      + 1 /* domain */\
      + 1 /* domainID */\
      + 1 /* index */\
      + 1 /* vtable pointer */)

#define OBJSIZE_MEMORY \
     ( XMOFF /* magic */\
         + 1 /* flags at negative index */\
     + (memory_sizeof_proxy()>>2) /* vtable pointer + data */)

#define OBJSIZE_SERVICEDESC \
 (((sizeof(DEPDesc) + 4) >> 2) \
    + 1  /* OBJFLAGS */)

#define OBJSIZE_SERVICEPOOL \
 (((sizeof(ServiceThreadPool) + 4) >> 2) \
    + 1  /* OBJFLAGS */)


#define OBJSIZE_DOMAINDESC \
      ( XMOFF /* magic */\
          + ((sizeof(struct DomainDesc_s)+4)>>2) \
          + 1 /* flags at index -1 */\
          + 1 /* vtable */)

#define OBJSIZE_ATOMVAR \
    ( XMOFF /* magic */\
    + 1 /* flags at negative index */\
    + 1 /* vtable pointer */\
    + 3 /* data */)

#define OBJSIZE_CAS \
    ( XMOFF /* magic */\
    + 1 /* flags at negative index */\
    + 1 /* vtable pointer */\
    + 1 /* data */)

#define OBJSIZE_VMOBJECT \
      ( XMOFF /* magic */\
      + 1 /* flags at negative index */\
      + 1 /* vtable pointer */\
      + 6 /* data */)

#define OBJSIZE_CREDENTIAL \
    ( XMOFF /* magic */\
    + 1 /* flags at negative index */\
    + 1 /* vtable pointer */\
    + 2 /* data */)

#define OBJSIZE_DOMAIN \
    ( XMOFF /* magic */\
    + 1 /* flags at negative index */\
    + 1 /* vtable pointer */\
    + 2 /* data */)

#define OBJSIZE_THREADDESCPROXY \
      ( XMOFF /* magic */\
      + ((sizeof(ThreadDescProxy)+4)>>2) \
      + 1 /* flags at index -1 */\
      + 1 /* vtable */)

#define OBJSIZE_FOREIGN_THREADDESC \
      ( XMOFF /* magic */\
      + ((sizeof(ThreadDescForeignProxy)+4)>>2) \
      + 1 /* flags at index -1 */\
      + 1 /* vtable */)

#define OBJSIZE_MAPPED_MEMORY \
      ( XMOFF /* magic */\
      + ((sizeof(MappedMemoryProxy)+4)>>2) \
      + 1 /* flags at index -1 */\
      + 1 /* vtable */)


#define OBJSIZE_INTERCEPTINBOUNDINFO \
        ( XMOFF /* magic */\
        + 1 /* flags at negative index */\
        + 1 /* vtable pointer */\
        + 6 /* data */)

#define OBJSIZE_INTERCEPTPORTALINFO \
        ( XMOFF /* magic */\
        + 1 /* flags at negative index */\
        + 1 /* vtable pointer */\
        + 2 /* data */)


static inline ObjectHandle gc_allocDataInDomain(struct DomainDesc_s * domain,
                                                int objsize, u32 flags)
{
    return domain->gc.allocDataInDomain(domain, objsize, flags);
}

void gc_in(struct ObjectDesc_s * o, struct DomainDesc_s * domain);
jboolean isRef(jbyte * map, int total, int num);

#define REF2HANDLE(ref) (&(ref))
// JUMP to atomic code
#define RETURN_FROMHANDLE(handle)  ASSERTOBJECT(*handle); return *handle;
#define RETURN_UNREGHANDLE(handle) return unregisterObject(curdom(), handle);
#define UNREGHANDLE(handle) unregisterObject(curdom(), handle);

#define gc_objSize(_o_) gc_objSize2(_o_, getObjFlags(_o_))
u32 gc_objSize2(ObjectDesc* obj, jint flags); 

void gc_walkContinuesBlock(DomainDesc * domain, u32 * start, u32 ** top,
                           HandleObject_t handleObject,
                           HandleObject_t handleArray,
                           HandleObject_t handlePortal,
                           HandleObject_t handleMemory,
                           HandleObject_t handleService,
                           HandleObject_t handleCAS,
                           HandleObject_t handleAtomVar,
                           HandleObject_t handleDomainProxy,
                           HandleObject_t handleCPUStateProxy,
                           HandleObject_t handleServicePool,
                           HandleObject_t handleStackProxy);

void gc_walkContinuesBlock_Alt(DomainDesc * domain, u32 * start,
                               u32 * top, HandleObject_t handleObject,
                               HandleObject_t handleArray,
                               HandleObject_t handlePortal,
                               HandleObject_t handleMemory,
                               HandleObject_t handleService,
                               HandleObject_t handleCAS,
                               HandleObject_t handleAtomVar,
                               HandleObject_t handleDomainProxy);


ObjectHandle registerObject(DomainDesc * domain, ObjectDesc * obj);
ObjectDesc *unregisterObject(DomainDesc * domain, ObjectHandle handle);

#endif

