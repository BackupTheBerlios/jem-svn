#ifndef GC_STACK_H
#define GC_STACK_H
#ifdef CONFIG_JEM_ENABLE_GC

jboolean find_stackmap(MethodDesc * method, u32  * eip, u32  * ebp,
		       jbyte * stackmap, u32  maxslots, u32  * nslots);
void list_stackmaps(MethodDesc * method);
void walkStack(DomainDesc * domain, ThreadDesc * thread,
	       HandleReference_t handleReference);

#endif				/* ENABLE_GC */



//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
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
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================

#endif				/* GC_STACK_H */
