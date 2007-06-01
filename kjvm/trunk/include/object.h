// Additional Copyrights:
// 	Copyright (C) 1997-2001 The JX Group.
//==============================================================================

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

