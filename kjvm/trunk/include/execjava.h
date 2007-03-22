//===========================================================================
// File: gc.h
//
// Jem/JVM garbage collector interface.
//
//===========================================================================

#ifndef _EXECJAVA_H
#define _EXECJAVA_H

#include "thread.h"

typedef jint(*java_method0_t) (struct ObjectDesc_s *);
typedef jint(*java_method1_t) (struct ObjectDesc_s *, long arg1);
typedef jint(*java_method2_t) (struct ObjectDesc_s *, long arg1, long arg2);

int call_JAVA_method0(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
		      java_method0_t function);
int call_JAVA_method1(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
		      java_method1_t function, long param);
int call_JAVA_method2(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
		      java_method2_t function, long param1, long param2);

/* activates a Java mthod but does not save the current context
   eflags specifies the EFLAGS for the worker-Thread
   see CALL_WITH_... defines below */
void destroy_call_JAVA_function(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
				java_method0_t function, long eflags);
void destroy_call_JAVA_method1(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
			       java_method1_t function, long param,
			       long eflags);
void destroy_call_JAVA_method2(struct ObjectDesc_s * Object, struct ThreadDesc_s * worker,
			       java_method2_t function, long param1,
			       long param2, long eflags);
#define CALL_WITH_ENABLED_IRQS 0x00000212
#define CALL_WITH_DISABLED_IRQS 0x00000012


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

#endif /* _EXECJAVA_H*/

