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
#ifndef GC_MOVE_H
#define GC_MOVE_H

ObjectDesc *gc_impl_shallowCopyObject(u32 * dst, ObjectDesc * srcObj);
void gc_impl_walkContentObject(DomainDesc * domain, ObjectDesc * obj,
			       HandleReference_t handleReference);

DEPDesc *gc_impl_shallowCopyService(u32 * dst, DEPDesc * srcObj);
void gc_impl_walkContentService(DomainDesc * domain, DEPDesc * obj,
				HandleReference_t handleReference);

ArrayDesc *gc_impl_shallowCopyArray(u32 * dst, ArrayDesc * srcObj);
void gc_impl_walkContentArray(DomainDesc * domain, ArrayDesc * obj,
			      HandleReference_t handleReference);

Proxy *gc_impl_shallowCopyPortal(u32 * dst, Proxy * srcObj);

CASProxy *gc_impl_shallowCopyCAS(u32 * dst, CASProxy * srcObj);

AtomicVariableProxy *gc_impl_shallowCopyAtomVar(u32 * dst,
						AtomicVariableProxy *
						srcObj);
void gc_impl_walkContentAtomVar(DomainDesc * domain,
				AtomicVariableProxy * obj,
				HandleReference_t handleReference);

void gc_impl_walkContent(DomainDesc * domain, ObjectDesc * obj,
			 HandleReference_t handleReference);

void gc_impl_walkContentServicePool(DomainDesc * domain, ServiceThreadPool * obj, HandleReference_t handleReference);
void gc_impl_walkContentForeignCPUState(DomainDesc * domain, ThreadDescForeignProxy * obj, HandleReference_t handleReference);
void gc_impl_walkContentCPUState(DomainDesc * domain, ThreadDescProxy * obj, HandleReference_t handleReference);


#endif				/* GC_MOVE_H */
