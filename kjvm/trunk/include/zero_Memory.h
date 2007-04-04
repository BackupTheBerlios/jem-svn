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

#ifndef ZERO_MEMORY_H
#define ZERO_MEMORY_H


struct MemoryProxy_s;
typedef struct MemoryProxy_s **MemoryProxyHandle;

void dzmemory_decRefcount(struct MemoryProxy_s *m);
void dzmemory_alive(struct MemoryProxy_s *dzm);
void dzmemory_redirect_invalid_dz(MemoryProxyHandle mem);


ObjectDesc *copy_memory(struct DomainDesc_s *src, struct DomainDesc_s *dst,
			struct MemoryProxy_s *obj, u32 * quota);


MemoryProxyHandle allocMemoryProxyInDomain(DomainDesc * domain,
					   ClassDesc * c, jint start,
					   jint size);

struct MemoryProxy_s;
struct MemoryProxy_s *gc_impl_shallowCopyMemory(u32 * dst,
						struct MemoryProxy_s
						*srcObj);

u32             memory_sizeof_proxy(void);
void            memory_deleted(struct MemoryProxy_s *obj);
jint            memory_getStartAddress(ObjectDesc * self);
jint            memory_size(ObjectDesc * self);
ObjectDesc      *memoryManager_alloc(ObjectDesc * self, jint size);
ObjectDesc      *memoryManager_allocAligned(ObjectDesc * self, jint size, jint bytes);
ObjectDesc      *copy_shallow_memory(DomainDesc * src, DomainDesc * dst, struct MemoryProxy_s * obj, u32 * quota);
void            copy_content_memory(DomainDesc * src, DomainDesc * dst, struct MemoryProxy_s * obj, u32 * quota);

#endif
