//===========================================================================
// File: malloc.h
//
// Jem/JVM memory allocation interface.
//
//===========================================================================

#ifndef _MALLOC_H
#define _MALLOC_H

#include <linux/types.h>

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


int jemMallocInit(void);
int jemDestroyHeap(void);
void *jemMalloc(u32 size MEMTYPE_INFO);
char *jemMallocCode(struct DomainDesc_s *domain, u32 size);



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

#endif

