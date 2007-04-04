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
#include "portal.h"

void        initPrimitiveClasses(void);
void        stringToChar(ObjectDesc * str, char *buf, jint buflen);
LibDesc     *load(DomainDesc * domain, char *filename);
void        linkdomain(DomainDesc * domain, LibDesc * lib,
                       jint allocObjectFunction, jint allocArrayFunction);
void        callClassConstructorsInLib(LibDesc * lib);
u32         executeStatic(DomainDesc * domain, char *ClassName, char *methodname,
                          char *signature, jint * params, jint params_size);
jint        getArraySize(ArrayDesc * array);
ObjectDesc  *getReferenceArrayElement(ArrayDesc * array, jint pos);
char        *methodName2str(ClassDesc * jclass, MethodDesc * method, char *buffer,
                            int size);
MethodDesc  *findMethod(DomainDesc * domain, char *classname, char *methodname, char *signature);
JClass      *findClass(DomainDesc * domain, char *name);
jint        findDEPMethodIndex(DomainDesc * domain, char *className, char *methodName, char *signature);
u32         executeVirtual(DomainDesc * domain, char *className, char *methodname, char *signature, ObjectDesc * obj, jint * params,
                           int params_size);
void        callClassConstructor(JClass * cl);
ClassDesc   *findClassDesc(char *name);
ClassDesc   *findClassDescInSharedLib(SharedLibDesc * lib, char *name);
int         findMethodAtAddr(u8 * addr, MethodDesc ** method,
                             ClassDesc ** ClassInfo, jint * bytecodePos,
                             jint * lineNumber);
ClassDesc   *findSharedArrayClassDescByElemClass(ClassDesc * elemClass);
u32         findFieldOffset(ClassDesc * c, char *fieldname);
LibDesc     *sharedLib2Lib(DomainDesc * domain, SharedLibDesc * slib);
void        copyIntoByteArray(ArrayDesc * array, char *str, jint size);
ObjectDesc  *string_replace_char(ObjectDesc * str, jint c1, jint c2);
ObjectDesc  *newStringFromClassname(DomainDesc * domain, char *value);
ObjectDesc  *newString(DomainDesc * domain, char *value);
ObjectDesc  *newStringArray(DomainDesc * domain, int size, char *arr[]);
ClassDesc   *obj2ClassDesc(ObjectDesc * obj);
JClass      *ClassDesc2Class(DomainDesc * domain, ClassDesc * classDesc);
ObjectDesc  *allocObject(ClassDesc * c);
void        callClassConstructors(DomainDesc * domain, LibDesc * lib);
void        executeInterface(DomainDesc * domain, char *className,
                             char *methodname, char *signature, ObjectDesc * obj,
                             jint * params, int params_size);
void        executeSpecial(DomainDesc * domain, char *className, char *methodname,
                           char *signature, ObjectDesc * obj, jint * params,
                           int params_size);
ArrayDesc   *allocArrayInDomain(DomainDesc * domain, ClassDesc * type, jint size);
ArrayDesc   *specialAllocArray(ClassDesc * elemClass, jint size);
void        findClassDescAndMethodInObject(SharedLibDesc * lib, char *classname, char *methodname, char *signature,
                                           ClassDesc ** classFound, MethodDesc ** methodFound);
JClass      *findClassOrPrimitive(DomainDesc * domain, char *name);
ObjectDesc  *specialAllocObject(ClassDesc * c);

extern SharedLibDesc    *sharedLibs;
extern DEPDesc          **allDEPInstances;
extern int              numDEPInstances;
extern DomainDesc       *domainZero;
extern ClassDesc        *java_lang_Object;
extern JClass           *java_lang_Object_Class;
extern code_t           *array_vtable;

#define obj2ClassDesc(obj) ( *(ClassDesc**)(((ObjectDesc*)(obj))->vtable-1) )

#define obj2ClassDescFAST(obj) ( *(ClassDesc**)(((ObjectDesc*)(obj))->vtable-1) )


#endif              /* LOAD_H */
