//===========================================================================
// File: object.h
//
// Jem/JVM object interface.
//
//===========================================================================

#ifndef _OBJECT_H
#define _OBJECT_H


#define MAGIC_OBJECT 0xbebeceee
#define MAGIC_INVALID 0xabcdef98
#define MAGIC_CPU 0xcb0cb0ff



typedef struct ObjectDesc_s {
	code_t      *vtable;
	jint        data[1];
} ObjectDesc;

typedef struct ObjectDesc_s **ObjectHandle;

typedef struct ArrayDesc_s {
	code_t              *vtable;
	struct ClassDesc_s  *arrayClass;
	jint                size;
	jint                data[1];
} ArrayDesc;

typedef struct CPUDesc_s {
	u32         magic;
	int         cpu_id;
} CPUDesc;




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

