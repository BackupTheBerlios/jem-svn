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
// exception_handler.h
// 
// Java exception handler interface
// 
//==============================================================================

#ifndef _EXCEPTION_HANDLER_H_
#define _EXCEPTION_HANDLER_H_

/* #define THROW_VirtualMachineError */
#define THROW_OutOfMemoryError       ((jint*)-3)
#define THROW_StackOverflowError     ((jint*)-5)

#define THROW_RuntimeException       ((jint*)-1)
#define THROW_NullPointerException   ((jint*)-2)
#define THROW_MemoryIndexOutOfBounds ((jint*)-4)
#define THROW_ArithmeticException    ((jint*)-6)

#define THROW_MagicNumber            ((jint*)-7)
#define THROW_ParanoidCheck          ((jint*)-8)
#define THROW_StackJam               ((jint*)-9)

#define THROW_ArrayIndexOutOfBounds  ((jint*)-10)
#define THROW_UnsupportedByteCode    ((jint*)-11)
#define THROW_InvalidMemory          ((jint*)-12)

#define THROW_MemoryExhaustedException ((jint*)-13)
#define THROW_DomainTerminatedException ((jint*)-14)

#define CHECK_NULL_PTR(_ptr_) {if (_ptr_==NULL) exceptionHandler(THROW_NullPointerException);}
#define CHECK_NULL_POINTER(_exp_) {if (_exp_) exceptionHandler(THROW_NullPointerException);}

ObjectDesc *createExceptionInDomain(DomainDesc * domain,
                                    const char *exception,
                                    const char *details);

void throw_exception(ObjectDesc * exception, u32 * sp);
void throw_ArithmeticException(jint dummy);
void throw_StackOverflowError(void);
void throw_ArrayIndexOutOfBounds(jint dummy);
void throw_NullPointerException(jint dummy);

void exceptionHandlerMsg(jint * p, char *msg);
void exceptionHandler(jint * p);
void exceptionHandlerInternal(char *msg);


#endif
