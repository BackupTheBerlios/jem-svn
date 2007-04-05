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


#endif

