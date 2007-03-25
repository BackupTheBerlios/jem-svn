//==============================================================================
// load.h
// 
// 
//==============================================================================

#ifndef LOAD_H
#define LOAD_H

#define CURRENT_COMPILER_VERSION 9

#include "domain.h"
#include "object.h"
#include "code.h"

#ifdef CONFIG_JEM_PROFILE
struct heapsample_s {
	char *eip[10];
	ClassDesc *cl;
	jint size;
};
#endif

void initPrimitiveClasses(void);
void stringToChar(ObjectDesc * str, char *buf, jint buflen);
LibDesc *load(DomainDesc * domain, char *filename);
void linkdomain(DomainDesc * domain, LibDesc * lib,
		jint allocObjectFunction, jint allocArrayFunction);
void callClassConstructorsInLib(LibDesc * lib);
u32 executeStatic(DomainDesc * domain, char *ClassName, char *methodname,
		   char *signature, jint * params, jint params_size);

jint getArraySize(ArrayDesc * array);
ObjectDesc *getReferenceArrayElement(ArrayDesc * array, jint pos);


char *methodName2str(ClassDesc * jclass, MethodDesc * method, char *buffer,
		     int size);
JClass *findClass(DomainDesc * domain, char *name);
JClass *findClassInLib(LibDesc * lib, char *name);
JClass *findClassOrPrimitive(DomainDesc * domain, char *name);
int findClassForMethod(MethodDesc * method, JClass ** jclass);
code_t findAddrOfMethodBytecode(char *ClassName, char *method,
				char *signature, jint bytecodePos);
int findMethodAtAddr(u8 * addr, MethodDesc ** method,
		     ClassDesc ** ClassInfo, jint * bytecodePos,
		     jint * lineNumber);

MethodDesc *cloneMethodInDomain(DomainDesc * domain, MethodDesc * method);

ClassDesc *findSharedArrayClassDescByElemClass(ClassDesc * elemClass);
ObjectDesc *newString(DomainDesc * domain, char *value);
ObjectDesc *newStringArray(DomainDesc * domain, int size, char *arr[]);
ClassDesc *findClassDescInSharedLib(SharedLibDesc * lib, char *name);
ClassDesc *obj2ClassDesc(ObjectDesc * obj);
JClass *ClassDesc2Class(DomainDesc * domain, ClassDesc * classDesc);
ObjectDesc *allocObject(ClassDesc * c);
void callClassConstructors(DomainDesc * domain, LibDesc * lib);
void linksharedlib(DomainDesc * domain, SharedLibDesc * lib,
		   jint allocObjectFunction, jint allocArrayFunction,
		   TempMemory * tmp_mem);
void executeInterface(DomainDesc * domain, char *className,
		      char *methodname, char *signature, ObjectDesc * obj,
		      jint * params, int params_size);
void executeSpecial(DomainDesc * domain, char *className, char *methodname,
		    char *signature, ObjectDesc * obj, jint * params,
		    int params_size);
void createVTable(DomainDesc * domain, ClassDesc * c);

JClass *specialAllocClass(DomainDesc * domain, int number);

ClassDesc *findClassDesc(char *name);

extern SharedLibDesc *sharedLibs;

extern DEPDesc **allDEPInstances;
extern int numDEPInstances;

extern DomainDesc *domainZero;

extern ClassDesc *java_lang_Object;
extern JClass *java_lang_Object_Class;
extern code_t *array_vtable;

#define obj2ClassDesc(obj) ( *(ClassDesc**)(((ObjectDesc*)(obj))->vtable-1) )

#define obj2ClassDescFAST(obj) ( *(ClassDesc**)(((ObjectDesc*)(obj))->vtable-1) )

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

#endif				/* LOAD_H */
