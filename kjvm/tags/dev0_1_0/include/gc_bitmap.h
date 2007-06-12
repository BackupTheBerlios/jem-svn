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
// File: gc_bitmap.h
//
//
//===========================================================================

#ifndef GC_BITMAP_H
#define GC_BITMAP_H

void gc_bitmap_init(DomainDesc * domain, u32 heap_bytes);

ObjectHandle gc_bitmap_allocDataInDomain(DomainDesc * domain,
                                         int objSize, u32 flags);
u32 gc_bitmap_freeWords(DomainDesc * domain);
u32 gc_bitmap_totalWords(struct DomainDesc_s *domain);
void gc_bitmap_printInfo(struct DomainDesc_s *domain);
void gc_bitmap_init(DomainDesc * domain, u32 heap_bytes);
void gc_bitmap_done(DomainDesc * domain);
void gc_bitmap_gc(DomainDesc * domain);
int gc_bitmap_isInHeap(DomainDesc * domain, ObjectDesc * obj);
void gc_bitmap_walkHeap(DomainDesc * domain,
                        HandleObject_t handleObject,
                        HandleObject_t handleArray,
                        HandleObject_t handlePortal,
                        HandleObject_t handleMemory,
                        HandleObject_t handleService,
                        HandleObject_t handleCAS,
                        HandleObject_t handleAtomVar,
                        HandleObject_t handleDomainProxy,
                        HandleObject_t handleCPUStateProxy);

#endif              /* GC_BITMAP_H */
