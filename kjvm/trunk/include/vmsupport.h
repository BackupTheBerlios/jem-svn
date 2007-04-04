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
// vmsupport.h
// 
// 
//==============================================================================

#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#define VMSUPPORT(_ndx_) vmsupport[vmsupport[_ndx_].index]

typedef struct {
	char *name;
	int index;
	code_t fkt;
} vm_fkt_table_t;

extern jint numberVMOperations;
extern vm_fkt_table_t vmsupport[];

void vm_unsupported(void);

ClassDesc *get_element_class(ClassDesc * c);
jboolean is_interface(ClassDesc * c);
jboolean implements_interface(ClassDesc * c, ClassDesc * ifa);
jboolean check_assign(ClassDesc * variable_class, ClassDesc * reference_class);

#endif
