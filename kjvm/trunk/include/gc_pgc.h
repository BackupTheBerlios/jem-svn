//==============================================================================
// gc_pgc.h
// 
// 
//==============================================================================

#ifndef GC_PGC_H
#define GC_PGC_H

#ifdef CONFIG_JEM_PROFILE
void pgcNewRun();
void pgcEndRun();
void pgcBegin(int i);
void pgcEnd(int i);

void printProfileGC();

enum pgcCtrIds_e {
	PGC_GC,
	PGC_MOVE,
	PGC_STACK,
	PGC_SPECIAL,
	PGC_REGISTERED,
	PGC_HEAP,
	PGC_INTR,
	PGC_SCAN,
	PGC_PROTECT,
	PGC_STATIC,
	PGC_STACKMAP,
	PGC_STACK1,
	PGC_STACK2,
	PGC_STACK3,
	PGC_STACK4,
	PGC_SERVICE,
	MAX_PGC_CTR_ID
};

#define PGC_GC_USE   1
#define PGC_MOVE_USE 1
#define PGC_STACK_USE 1
#define PGC_SPECIAL_USE 1
#define PGC_REGISTERED_USE 1
#define PGC_HEAP_USE 1
#define PGC_INTR_USE 1
#define PGC_SCAN_USE 1
#define PGC_PROTECT_USE 1
#define PGC_STATIC_USE 1
#define PGC_SERVICE_USE 1
#define PGC_STACKMAP_USE 1
#define PGC_STACK1_USE 1
#define PGC_STACK2_USE 1
#define PGC_STACK3_USE 1
#define PGC_STACK4_USE 1


#define PGCB(x) if (PGC_##x##_USE) pgcBegin(PGC_##x);
#define PGCE(x) if (PGC_##x##_USE) pgcEnd(PGC_##x);

#else				/* PROFILE_GC */

#define PGCB(x)
#define PGCE(x)

#endif				/* PROFILE_GC */


//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
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

#endif				/* GC_PGC_H */
