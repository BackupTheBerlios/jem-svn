//==============================================================================
// gc_impl.h
// 
// 
//==============================================================================

#ifndef GC_IMPL_H
#define GC_IMPL_H

#ifdef CONFIG_JEM_ENABLE_GC

static spinlock_t gc_lock = SPIN_LOCK_UNLOCKED;
#define GC_LOCK    spin_lock(&gc_lock);
#define GC_UNLOCK  spin_unlock(&gc_lock);

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
						AtomicVariableProxy *srcObj);
void gc_impl_walkContentAtomVar(DomainDesc *domain,
				AtomicVariableProxy * obj,
				HandleReference_t handleReference);

void gc_impl_walkContent(DomainDesc * domain, ObjectDesc * obj,
			 HandleReference_t handleReference);

void freezeThreads(DomainDesc * domain);

void walkStacks(DomainDesc * domain, HandleReference_t handler);
void walkStatics(DomainDesc * domain, HandleReference_t handler);
void walkPortals(DomainDesc * domain, HandleReference_t handler);
void walkRegistered(DomainDesc * domain, HandleReference_t handler);
void walkSpecial(DomainDesc * domain, HandleReference_t handler);
void walkInterrupHandlers(DomainDesc * domain, HandleReference_t handler);

void walkRootSet(DomainDesc * domain,
		 HandleReference_t stacksHandler,
		 HandleReference_t staticsHandler,
		 HandleReference_t portalsHandler,
		 HandleReference_t registeredHandler,
		 HandleReference_t specialHandler,
		 HandleReference_t interruptHandlersHandler);

#define FORWARD_MASK            0x00000001
#define FORWARD_PTR_MASK        0xfffffffe

/* flag word is used as forwarding pointer
   flags must be at high position 
*/

#define GC_FORWARD 0x00000001
#define GC_WHITE   0x00000000


#define FORBITMAP(map, mapsize, isset, notset) \
{ \
  u8 * addr = map; \
  u32 index; \
  u8 bits = 0; \
  for(index = 0;index < mapsize; index++) { \
    if (index % 8 == 0) bits = *addr++; \
    if (bits&1) { \
      isset; \
    } else { \
      notset; \
    } \
    bits >>= 1; \
  } \
}

#endif				/* ENABLE_GC */

#define MOVETCB(x) if (x) {tpr = thread2CPUState(x); handler(domain, (ObjectDesc **) & (tpr)); x = cpuState2thread(tpr);}

//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
// Copyright (C) 1998-2002 Michael Golm
// Copyright (C) 2001-2002 Joerg Baumann
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

#endif				/* GC_IMPL_H */
