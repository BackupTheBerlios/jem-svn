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
// File: code.h
//
// Jem/JVM java code interface.
//
//===========================================================================

#ifndef _CODE_H
#define _CODE_H

#include "object.h"


#define DEPFLAG_NONE   0
#define DEPFLAG_REDO   1
#define METHODFLAGS_STATIC 0x00000001
#define IS_STATIC(m) ((m)->flags & METHODFLAGS_STATIC != 0)
#define CLASSTYPE_CLASS          0x01
#define CLASSTYPE_INTERFACE      0x03
#define CLASSTYPE_ARRAYCLASS     0x04
#define CLASSTYPE_PRIMITIVE      0x08
#define CLASS_NOT_INIT 0
#define CLASS_READY 1
#define LIB_HASHKEY_LEN 10
#define MAGIC_SLIB 0x42240911
#define MAGIC_LIB 0x12345678
#define MAGIC_METHODDESC 0x42414039
#define MAGIC_CLASS 0x007babab
#define MAGIC_CLASSDESC 0x47114711


typedef struct SymbolDesc_s {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
} SymbolDesc;

typedef struct SymbolDescStackMap_s {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    jint    immediateNCIndexPre;
    int     n_bytes;
    int     n_bits;
    jbyte   *map;
} SymbolDescStackMap;

typedef struct FieldDesc_s {
    char    *fieldName;
    char    *fieldType;
    char    fieldOffset;
} FieldDesc;

typedef struct ByteCodeDesc_s {
    jint    bytecodePos;
    jint    start;
    jint    end;
} ByteCodeDesc;

typedef struct SourceLineDesc_s {
    jint    startBytecode;
    jint    lineNumber;
} SourceLineDesc;

typedef struct ExceptionDesc_s {
    jint                start;
    jint                end;
    struct ClassDesc_s  *type;
    u32                 addr;
} ExceptionDesc;

typedef struct MethodDesc_s {
    u32                     objectDesc_magic;
    u32                     objectDesc_flags;
    code_t                  *objectDesc_vtable;
    u32                     magic;
    char                    *name;
    char                    *signature;
    jint                    numberOfCodeBytes;
    jint                    numberOfSymbols;
    struct SymbolDesc_s     **symbols;
    jint                    numberOfByteCodes;
    struct ByteCodeDesc_s   *bytecodeTable;
    jint                    codeOffset;
    code_t                  code;
    jint                    numberOfArgs;
    jint                    numberOfArgTypeMapBytes;
    jbyte                   *argTypeMap;
    jint                    returnType;
    jint                    sizeLocalVars;
    jint                    isprofiled;
    struct ClassDesc_s      *classDesc;
    jint                    sizeOfExceptionTable;
    struct ExceptionDesc_s  *exceptionTable;
    jint                    numberOfSourceLines;
    struct SourceLineDesc_s *sourceLineTable;
    u32                     flags;
} MethodDesc;


typedef struct ClassDesc_s {
    u32                     magic;
    jint                    classType;
    char                    *name;
    struct ClassDesc_s      *superclass;
    jint                    numberOfInterfaces;
    char                    **ifname;
    struct ClassDesc_s      **interfaces;
    jint                    numberOfMethods;
    jint                    vtableSize;
    struct MethodDesc_s     *methods;
    code_t                  *vtable;
    char                    **vtableSym;
    jint                    instanceSize;
    jint                    staticFieldsSize;
    struct SharedLibDesc_s  *definingLib;
    struct ClassDesc_s      *next;
    jint                    mapBytes;
    jbyte                   *map;
    jint                    staticsMapBytes;
    jbyte                   *staticsMap;
    code_t                  *proxyVtable;
    struct MethodDesc_s     **methodVtable;
    struct ClassDesc_s      *arrayClass;
    jint                    numberFields;
    struct FieldDesc_s      *fields;
    u32                     inheritServiceThread;
    u32                     copied;
    u32                     copied_arrayelements;
    jint                    sfield_offset;
    jint                    n_instances;
    jint                    n_arrayelements;
    struct ClassDesc_s      *elementClass;
    struct ClassDesc_s      *nextInDomain;
} ClassDesc;


typedef struct JClass_s {
    u32                 objectDesc_magic;
    u32                 objectDesc_flags;
    code_t              *objectDesc_vtable;
    u32                 magic;
    struct ClassDesc_s  *classDesc;
    struct JClass_s     *superclass;
    jint                *staticFields;
    jint                state;
    jint                numberOfInstances;
} JClass;

struct meta_s {
    char *var;
    char *val;
};

typedef struct SharedLibDesc_s {
    u32                     magic;
    char                    *name;
    jint                    ndx;
    jint                    memSizeStaticFields;
    u32                     id;
    jint                    numberOfClasses;
    struct ClassDesc_s      *allClasses;
    jint                    numberOfNeededLibs;
    struct SharedLibDesc_s  **neededLibs;
    char                    *code;
    u32                     codeBytes;
    char                    key[LIB_HASHKEY_LEN];
    u32                     vtablesize;
    u32                     bytecodes;
    u32                     numberOfMeta;
    struct meta_s           *meta;
    struct SharedLibDesc_s  *next;
} SharedLibDesc;


typedef struct LibDesc_s {
    u32                     magic;
    jint                    numberOfClasses;
    char                    hasNoImplementations;
    struct JClass_s         *allClasses;
    char                    key[LIB_HASHKEY_LEN];
    struct SharedLibDesc_s  *sharedLib;
    int                     initialized;
} LibDesc;


jint callnative_special(jint * params, ObjectDesc * obj, code_t f,
                        jint params_size);
jint callnative_special_portal(jint * params, ObjectDesc * obj, code_t f,
                               jint params_size);
jint callnative_static(jint * params, code_t f, jint params_size);
void callnative_handler(u32 * ebp, u32 * sp, char *addr);


#endif
