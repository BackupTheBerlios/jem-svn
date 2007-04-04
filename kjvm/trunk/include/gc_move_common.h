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
// gc_move_common.h
// 
// 
//==============================================================================

#ifndef GC_MOVE_COMMON_H
#define GC_MOVE_COMMON_H

typedef struct gc_move_common_mem_s {
	u32* (*allocHeap2) (struct DomainDesc_s * domain, u32 size);
	void (*walkHeap2) (struct DomainDesc_s * domain,
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
			  HandleObject_t handleStack);
} gc_move_common_mem_t;

u32 *gc_common_move_reference(DomainDesc * domain, ObjectDesc ** refPtr);

#define GCM_MOVE_COMMON(domain) (*(gc_move_common_mem_t*)(&domain->gc.untypedMemory))


#endif
