//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
// Copyright © 2001-2002 Christian Wawersich
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
// Loader and linker
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "load.h"
#include "gc_alloc.h"
#include "jar.h"
#include "libcache.h"
#include "vmsupport.h"
#include "exception_handler.h"

SharedLibDesc   *sharedLibs = NULL;
static jint     sharedLibsIndexNumber = 0;
ClassDesc       *java_lang_Object;
JClass          *java_lang_Object_class;
code_t          *array_vtable_prototype;


#if defined(_BIG_ENDIAN)
NOT IMPL jint readInt()
{
    unsigned char *b;
    jint i;
    b = (unsigned char *) codefilepos;
    codefilepos += 4;
    i = (((((b[3] << 8) | b[2]) << 8) | b[1]) << 8) | b[0];
    return i;
}
#else
#define readInt(i) {i = *(jint*)codefilepos; codefilepos += 4;}
#endif

#define readShort(i) {i = *(jshort*)codefilepos; codefilepos += 2;}

#define readByte(i) {i = *(jbyte*)codefilepos; codefilepos += 1;}

#define readStringData(buf, nBytes) { memcpy(buf, codefilepos, nBytes); codefilepos += nBytes; buf[nBytes] = '\0';}

#define readString(buf,nbuf) { jint nBytes; readInt(nBytes); if (nBytes >= nbuf) printk(KERN_ERR "Buf too small\n"); readStringData(buf, nBytes);}

#define readStringID(buf) { jint id ; readInt(id); buf = string_table[id]; }

#define readAllocString(buf) {jint nBytes; readInt(nBytes);  if (nBytes >= 10000) printk(KERN_ERR "nBytes too large\n"); buf = jemMallocString(domain, nBytes+1); readStringData(buf, nBytes);}

#define readCode(buf, nbytes) {  memcpy(buf, codefilepos, nbytes); codefilepos += nbytes;}

#define STACK_CHUNK      13

ArrayDesc *vmSpecialAllocMultiArray(ClassDesc * elemClass, jint dim, jint sizes);

/**
 * SYMBOLS
 */


typedef SymbolDesc SymbolDescDomainZero;

typedef SymbolDesc SymbolDescExceptionHandler;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    char *className;
    char *methodName;
    char *methodSignature;
} SymbolDescDEPFunction;

typedef SymbolDesc SymbolDescAllocObject;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    char *className;
    jint kind;
    jint fieldOffset;
} SymbolDescStaticField;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    jint kind;
} SymbolDescProfile;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    char *className;
} SymbolDescClass;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    char *className;
    char *methodName;
    char *methodSignature;
} SymbolDescDirectMethodCall;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    char *value;
} SymbolDescString;

typedef SymbolDesc SymbolDescAllocArray;

typedef SymbolDesc SymbolDescAllocMultiArray;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    jint operation;
} SymbolDescLongArithmetic;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    jint operation;
} SymbolDescVMSupport;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    int primitiveType;
} SymbolDescPrimitiveClass;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    int targetNCIndex;
} SymbolDescUnresolvedJump;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    int targetNCIndex;  /* extends UnresolvedJump ! */
    int rangeStart;
    int rangeEnd;
    char *className;
} SymbolDescExceptionTable;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
} SymbolDescThreadPointer;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
} SymbolDescStackChunkSize;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
} SymbolDescMethodeDesc;

typedef struct {
    jint    type;
    jint    immediateNCIndex;
    jint    numBytes;
    jint    nextInstrNCIndex;
    jint kind;
} SymbolDescTCBOffset;

/*****/

/*
 * Prototypes
 */
static void        findClassDescAndMethod(char *classname, char *methodname, char *signature, ClassDesc ** classFound,
                                   MethodDesc ** methodFound);
static void        findClassAndMethodInLib(LibDesc * lib, char *classname, char *methodname, char *signature, JClass ** classFound,
                                    MethodDesc ** methodFound);
static MethodDesc  *findMethodInSharedLibs(char *classname, char *methodname, char *signature);
static ClassDesc   *createSharedArrayClassDesc(char *name);
static JClass      *findPrimitiveClass(char name);
static SharedLibDesc *loadSharedLibrary(DomainDesc * domain, char *filename, TempMemory * tmp_mem);
static LibDesc *loadLib(DomainDesc * domain, SharedLibDesc * sharedLib);
static void linksharedlib(DomainDesc * domain, SharedLibDesc * lib, jint allocObjectFunction, jint allocArrayFunction,
           TempMemory * tmp_mem);
static ClassDesc   *createSharedArrayClassDescUsingElemClass(ClassDesc * elemClass);
static MethodDesc  *findMethodInLib(LibDesc * lib, char *classname, char *methodname, char *signature);


/*
 * Class management
 */

JClass *class_I;
JClass *class_J;
JClass *class_F;
JClass *class_D;
JClass *class_B;
JClass *class_C;
JClass *class_S;
JClass *class_Z;

ClassDesc  *sharedArrayClasses = NULL;
ClassDesc  *vmclassClass;
ClassDesc  *vmmethodClass;

static JClass *specialAllocClass(DomainDesc * domain, int number)
{
    int i;
    JClass *ret;
    JClass *c = jemMallocClasses(domain, number);
    memset(c, 0, sizeof(JClass) * number);
    ret = c;
    for (i = 0; i < number; i++) {
        c->magic = MAGIC_CLASS;
        c->objectDesc_magic = MAGIC_OBJECT;
        c->objectDesc_flags = OBJFLAGS_EXTERNAL_CLASS;

        if (vmclassClass != NULL)
            c->objectDesc_vtable = vmclassClass->vtable;
        c++;
    }
    return ret;
}

static JClass *createPrimitiveClass(char *name)
{
    ClassDesc *cd = jemMallocPrimitiveclassdesc(domainZero, strlen(name) + 1);
    JClass *c = specialAllocClass(domainZero, 1);
    cd->magic = MAGIC_CLASSDESC;
    c->classDesc = (ClassDesc *) cd;
    cd->classType = CLASSTYPE_PRIMITIVE;
    strcpy(cd->name, name);
    return c;
}

void initPrimitiveClasses(void)
{
    class_I = createPrimitiveClass("I");
    class_J = createPrimitiveClass("J");
    class_F = createPrimitiveClass("F");
    class_D = createPrimitiveClass("D");
    class_B = createPrimitiveClass("B");
    class_C = createPrimitiveClass("C");
    class_S = createPrimitiveClass("S");
    class_Z = createPrimitiveClass("Z");
}


static ClassDesc *findSharedArrayClassDesc(char *name)
{
    ClassDesc *c;
    for (c = (ClassDesc *) sharedArrayClasses; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            return c;
        }
    }
    /* not found, create one */
    c = (ClassDesc *) createSharedArrayClassDesc(name);
    c->next = (ClassDesc *) sharedArrayClasses;
    sharedArrayClasses = (ClassDesc *) c;
    return c;
}

ClassDesc *findSharedArrayClassDescByElemClass(ClassDesc * elemClass)
{
    ClassDesc *c;
    if (elemClass->arrayClass != NULL) {
        c = elemClass->arrayClass;
        return c;
    }
    /* not found, create one */
    c = createSharedArrayClassDescUsingElemClass(elemClass);
    c->next = (ClassDesc *) sharedArrayClasses;
    elemClass->arrayClass = c;
    sharedArrayClasses = c;
    return c;
}

/* creates a new array class 
*/
static JClass *createArrayClass(DomainDesc * domain, char *name)
{
    JClass *cl;
    char *n = name + 1;
    JClass *c = findClassOrPrimitive(domain, n);
    ClassDesc *arrayClass = (ClassDesc *) jemMalloc(sizeof(ClassDesc) MEMTYPE_OTHER);
    memset(arrayClass, 0, sizeof(ClassDesc));

    arrayClass->magic = MAGIC_CLASSDESC;
    arrayClass->classType = CLASSTYPE_ARRAYCLASS;
    arrayClass->name = (char *) jemMalloc(strlen(name) + 1 MEMTYPE_OTHER);
    strcpy(arrayClass->name, name);
    arrayClass->elementClass = c->classDesc;

    /* create class */
    cl = specialAllocClass(domain, 1);
    cl->classDesc = (ClassDesc *) arrayClass;

    /* add to domain */
    arrayClass->nextInDomain = domain->arrayClasses;
    domain->arrayClasses = (ClassDesc *) cl;

    return cl;
}


static ClassDesc *createSharedArrayClassDesc(char *name)
{
    ClassDesc *c;
    JClass *cl;
    ClassDesc *arrayClass;
    char value[80];
    u32 namelen;
    char *n = name + 1;

    if (*n == 'L') {
        strncpy(value, name + 2, strlen(name) - 3);
        value[strlen(name) - 3] = '\0';
        c = findClassDesc(value);
    } else if (*n == '[') {
        c = findSharedArrayClassDesc(n);
    } else {
        cl = findPrimitiveClass(*n);
        if (cl == NULL)
            return NULL;
        c = cl->classDesc;
    }
    if (c == NULL) return c;
    namelen = strlen(name) + 1;
    arrayClass = jemMallocArrayclassdesc(domainZero, namelen);
    arrayClass->magic = MAGIC_CLASSDESC;
    arrayClass->classType = CLASSTYPE_ARRAYCLASS;
    strcpy(arrayClass->name, name);
    arrayClass->elementClass = c;

    memcpy(arrayClass->vtable, array_vtable, 11 * 4);
    *((u32 *) (arrayClass->vtable) - 1) = (u32) arrayClass;

    return arrayClass;
}


static ClassDesc *createSharedArrayClassDescUsingElemClass(ClassDesc * elemClass)
{
    ClassDesc *arrayClass;
    ClassDesc *c = elemClass;
    jboolean primitiveElems;
    u32 namelen;

    if (c == NULL) return NULL;

    primitiveElems = *elemClass->name == '[' || elemClass->classType == CLASSTYPE_PRIMITIVE;
    if (primitiveElems) {
        namelen = strlen(elemClass->name) + 1 + 1;  /* [ ...  */
    } else {
        namelen = strlen(elemClass->name) + 1 + 3;  /* [L  ... ; */
    }

    arrayClass = jemMallocArrayclassdesc(domainZero, namelen + 1);
    arrayClass->magic = MAGIC_CLASSDESC;
    arrayClass->classType = CLASSTYPE_ARRAYCLASS;
    if (primitiveElems) {
        strcpy(arrayClass->name, "[");
        strcat(arrayClass->name, elemClass->name);
    } else {
        strcpy(arrayClass->name, "[L");
        strcat(arrayClass->name, elemClass->name);
        strcat(arrayClass->name, ";");
    }

    arrayClass->elementClass = elemClass;

    memcpy(arrayClass->vtable, array_vtable, 11 * 4);
    *((u32 *) (arrayClass->vtable) - 1) = (u32) arrayClass;

    return arrayClass;
}

static JClass *findPrimitiveClass(char name)
{
    switch (name) {
    case 'I':
        return class_I;
    case 'J':
        return class_J;
    case 'F':
        return class_F;
    case 'D':
        return class_D;
    case 'B':
        return class_B;
    case 'C':
        return class_C;
    case 'S':
        return class_S;
    case 'Z':
        return class_Z;
    }
    printk(KERN_ERR "Unknown primitive type %c at %s:%d", name, __FILE__, __LINE__);
    return NULL;
}

ClassDesc *findClassDescInSharedLib(SharedLibDesc * lib, char *name)
{
    int i;
    if (strcmp(name, "java/lang/Object") == 0)
        return java_lang_Object;
    for (i = 0; i < lib->numberOfClasses; i++) {
        if (strcmp(lib->allClasses[i].name, name) == 0)
            return &lib->allClasses[i];
    }
    return NULL;
}


static JClass *findClassInLib(LibDesc * lib, char *name)
{
    int i;

    if (strcmp(name, "java/lang/Object") == 0)
        return java_lang_Object_class;
    for (i = 0; i < lib->numberOfClasses; i++) {
        if (strcmp(lib->allClasses[i].classDesc->name, name) == 0)
            return &lib->allClasses[i];
    }
    return NULL;
}

/* classes are given as, for example,
 * Ljava/lang/Object; for Object class
 * or
 * I for primitive int type
 */
JClass *findClassOrPrimitive(DomainDesc * domain, char *name)
{
    JClass *cl;
    if (*name == '[') {
        return findClass(domain, name);
    } else if (*name != 'L') {
        if ((cl = findPrimitiveClass(*name)) != NULL) {
            return cl;
        }
    } else {
        char tmp[80];
        strncpy(tmp, name + 1, strlen(name) - 2);
        tmp[strlen(name) - 2] = '\0';
        return findClass(domain, tmp);
    }
    return NULL;
}

static void addHashKey(char *name, char *key, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (name[i] == 0)
            return;
        key[i] = key[i] | name[i];
    }
}

static jint testHashKey(char *name, char *key, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (name[i] == 0)
            return JNI_TRUE;
        if ((name[i] & key[i]) != name[i])
            return JNI_FALSE;
    }
    return JNI_TRUE;
}

u32 findFieldOffset(ClassDesc * c, char *fieldname)
{
    u32 i;
    for (i = 0; i < c->numberFields; i++) {
        if (strcmp(c->fields[i].fieldName, fieldname) == 0)
            return c->fields[i].fieldOffset;
    }
    return -1;
}

/* find class looking through all shared libs */
/* TODO: the same classname could be used in different
   shared libs. These libs cannot be used together but
   can both be available as global shared lib! */
ClassDesc *findClassDesc(char *name)
{
    SharedLibDesc *sharedLib;
    ClassDesc *cl;

    if (strcmp(name, "java/lang/Object") == 0)
        return java_lang_Object;

    sharedLib = sharedLibs;
    while (sharedLib != NULL) {
        if (testHashKey(name, sharedLib->key, LIB_HASHKEY_LEN)) {
            cl = findClassDescInSharedLib(sharedLib, name);
            if (cl != NULL)
                return cl;
        }
        sharedLib = (SharedLibDesc *) sharedLib->next;
    }
    return NULL;
}

JClass *findClass(DomainDesc * domain, char *name)
{
    jint i;
    JClass *cl;

    if (strcmp(name, "java/lang/Object") == 0)
        return java_lang_Object_class;

    for (i = 0; i < domain->numberOfLibs; i++) {
        if (testHashKey(name, domain->libs[i]->key, LIB_HASHKEY_LEN)) {
            cl = findClassInLib(domain->libs[i], name);
            if (cl != NULL)
                return cl;
        }
    }

    if (name[0] == '[') {
        cl = (JClass *) domain->arrayClasses;
        for (; cl != NULL; cl = (JClass *) cl->classDesc->nextInDomain) {
            if (strcmp(cl->classDesc->name, name) == 0) {
                return cl;
            }
        }
        cl = createArrayClass(domain, name);
        return cl;
    }
    return NULL;
}

LibDesc *sharedLib2Lib(DomainDesc * domain, SharedLibDesc * slib)
{
    if (slib == 0)
        return (LibDesc *) 0;
    return domain->ndx_libs[slib->ndx];
}

static JClass *classDesc2Class(DomainDesc * domain, ClassDesc * classDesc)
{
    int ndx;
    char *name;
    LibDesc *lib;
    SharedLibDesc *slib;

    name = classDesc->name;
    if (strcmp(name, "java/lang/Object") == 0)
        return java_lang_Object_class;

    if (name[0] == '<') {
        return NULL;    /* domainzero class */
    }

    if (name[0] != '[') {

        slib = classDesc->definingLib;
        if ((lib = sharedLib2Lib(domain, slib))) {
            ndx = (int) (classDesc - (slib->allClasses));
            return &(lib->allClasses[ndx]);
        }
        
        printk(KERN_ERR "Could not find class %s in domain %s!\n", name, domain->domainName);
        return NULL;
    } else {

        JClass *acl = (JClass *) domain->arrayClasses;
        for (; acl != NULL; acl = (JClass *) acl->classDesc->nextInDomain) {
            if (strcmp(acl->classDesc->name, name) == 0) {
                return acl;
            }
        }
        return createArrayClass(domain, name);

    }

    return NULL;
}


/*
 * support system
 */

char *methodName2str(ClassDesc * class, MethodDesc * method, char *buffer, int size)
{
    int i, p;
    char *src;

    if (class == NULL) {
        buffer[0] = 0;
        return buffer;
    }

    src = class->name;
    p = 0;
    for (i = 0; p < (size - 2); i++) {
        if (src[i] == 0)
            break;
        if ((src[i] == '/') || (src[i] == '\\')) {
            buffer[p++] = '.';
        } else {
            buffer[p++] = src[i];
        }
    }
    buffer[p++] = '.';

    if (method == NULL) {
        buffer[p] = 0;
        return buffer;
    }

    src = method->name;
    for (i = 0; p < (size - 1); i++) {
        if (src[i] == 0)
            break;
        buffer[p++] = src[i];
    }
    src = method->signature;
    for (i = 0; p < (size - 1); i++) {
        if (src[i] == 0)
            break;
        buffer[p++] = src[i];
    }
    buffer[p++] = 0;

    return buffer;
}


ObjectDesc *allocObject(ClassDesc * c)
{
    return allocObjectInDomain(curdom(), c);
}

jint getArraySize(ArrayDesc * array)
{
    if (array == NULL) return 0;
    return array->size;
}

ObjectDesc *getReferenceArrayElement(ArrayDesc * array, jint pos)
{
    return (ObjectDesc *) array->data[pos];
}

static void copyIntoCharArray(ArrayDesc * array, char *str, jint size)
{
    jint i;
    u32 *field = (u32 *) (array->data);

    for (i = 0; i < size; i++) {
        field[i] = str[i];
    }
}

static void copyFromCharArray(char *buf, jint buflen, ArrayDesc * array)
{
    jint i, size;
    u32 *field = (u32 *) (array->data);

    if (buflen == 0)
        return;

    buflen--;
    size = (array->size < buflen) ? array->size : buflen;

    for (i = 0; i < size; i++) {
        buf[i] = field[i];
    }

    buf[i] = 0;
}

void copyIntoByteArray(ArrayDesc * array, char *str, jint size)
{
    jint i;
    u32 *field = (u32 *) (array->data);

    for (i = 0; i < size; i++) {
        field[i] = str[i];
    }
}

ObjectDesc *string_replace_char(ObjectDesc * str, jint c1, jint c2)
{
    jint i;
    ArrayDesc *array;
    array = (ArrayDesc *) str->data[0];
    for (i = 0; i < array->size; i++) {
        if (array->data[i] == c1)
            array->data[i] = c2;
    }
    return str;
}

void stringToChar(ObjectDesc * str, char *c, jint buflen)
{
    ArrayDesc *arrObj;
    arrObj = (ArrayDesc *) str->data[0];
    copyFromCharArray(c, buflen, arrObj);
}

ObjectDesc *newString(DomainDesc * domain, char *value)
{
    ObjectDesc *s;
    ArrayDesc *arrObj;
    jint size;

    s = (ObjectDesc *) allocObjectInDomain(domain, findClassDesc("java/lang/String"));
    size = strlen(value);
    arrObj = allocArrayInDomain(domain, class_C->classDesc, size);
    s->data[0] = (jint) arrObj;
    copyIntoCharArray(arrObj, value, size);
    return s;
}

ObjectDesc *newStringArray(DomainDesc * domain, int size, char *arr[])
{
    ObjectDesc *sObj;
    ArrayDesc *arrObj;
    ClassDesc *acl;
    char *s;
    int i;

    acl = findClass(domain, "[Ljava/lang/String;")->classDesc;
    arrObj = allocArrayInDomain(domain, acl, size);

    for (i = 0; i < size; i++) {
        s = arr[i];
        sObj = newString(domain, s);
        arrObj->data[i] = (jint) sObj;
    }
    return (ObjectDesc *) arrObj;
}

ObjectDesc *newStringFromClassname(DomainDesc * domain, char *value)
{
    ObjectDesc *s;
    ArrayDesc *arrObj;
    jint size, i;

    s = (ObjectDesc *) allocObjectInDomain(domain, findClassDesc("java/lang/String"));
    size = strlen(value);
    arrObj = allocArrayInDomain(domain, class_C->classDesc, size);
    s->data[0] = (jint) arrObj;

    for (i = 0; i < size; i++) {
        if (value[i] == '/') {
            arrObj->data[i] = '.';
        } else {
            arrObj->data[i] = value[i];
        }
    }

    return s;
}

/* TODO: alloc shared strings in non-movable area (code area); this would allow to GC the heap of DomainZero */
static ObjectDesc *newDomainZeroString(char *value)
{
    ObjectDesc *o = newString(domainZero, value);
    setObjFlags(o, OBJFLAGS_EXTERNAL_STRING);
    return o;
}


/*
 * load/link/execute
 */
static char *read_codefile(char *filename, jint * size)
{
    jarentry entry;
    char *codefile;

    if ((codefile = libcache_lookup_jll(filename, size)) != NULL) {
        return codefile;
    }

    jarReset();
    for (;;) {
        if (jarNextEntry(&entry) == -1)
            return NULL;
        if (strcmp(entry.filename, filename) == 0) {
            codefile = entry.data;
            *size = entry.uncompressed_size;
            return codefile;
        }
    }
    return NULL;
}


static SharedLibDesc *findSharedLib(char *libname)
{
    SharedLibDesc *sharedLib;

    sharedLib = sharedLibs;
    while (sharedLib != NULL) {
        if (strcmp(sharedLib->name, libname) == 0) {
            /* found lib */
            break;
        }
        sharedLib = (SharedLibDesc *) sharedLib->next;
    }

    return sharedLib;
}

LibDesc *load(DomainDesc * domain, char *filename)
{
    SharedLibDesc *sharedLib;

    /* try to find an already loaded library */
    sharedLib = findSharedLib(filename);

    if (sharedLib == NULL) {
        /* could not find a loaded lib, now try to load it */
        TempMemory *tmp_mem = jemMallocTmp(6000);

        sharedLib = loadSharedLibrary(domainZero, filename, tmp_mem);

        if (sharedLib == NULL) {
            printk(KERN_ERR "Could not load shared library \"%s\"\n", filename);
            return NULL;
        }

        linksharedlib(domainZero, sharedLib, (jint) specialAllocObject, (jint) vmSpecialAllocArray, tmp_mem);
        jemFreeTmp(tmp_mem);

    }

    return loadLib(domain, sharedLib);
}


/* is called by no more than one thread per domain at a time */
static LibDesc *loadLib(DomainDesc * domain, SharedLibDesc * sharedLib)
{
    int i;
    LibDesc *lib;
    jint static_offset;
    jint *sfield = NULL;

    /*
       load neede Libs
     */

    for (i = 0; i < sharedLib->numberOfNeededLibs; i++) {
        if (sharedLib2Lib(domain, sharedLib->neededLibs[i]) == NULL) {
            loadLib(domain, sharedLib->neededLibs[i]);
        }
    }

    if (domain->numberOfLibs == domain->maxNumberOfLibs) {
        printk(KERN_WARNING "Max number of libs in domain %s reached!\n", domain->domainName);
        return NULL;
    }

    lib = jemMallocLibdesc(domain);
    memset(lib, 0, sizeof(LibDesc));
    lib->magic = MAGIC_LIB;
    lib->sharedLib = sharedLib;

    /* insert the lib in the domain */
    domain->libs[domain->numberOfLibs++] = lib;
    if (sharedLib->ndx < domain->maxNumberOfLibs) {
        domain->ndx_libs[sharedLib->ndx] = lib;
    } else {
        printk(KERN_ERR "Max number of libs reached! %d\n", (int) domain->maxNumberOfLibs);
        return NULL;
    }

    /* create the non-shared part of all classes */
    lib->numberOfClasses = sharedLib->numberOfClasses;
    lib->allClasses = specialAllocClass(domain, lib->numberOfClasses);

    static_offset = 0;
    if (sharedLib->memSizeStaticFields != 0) {
        sfield = (jint *) specialAllocStaticFields(domain, sharedLib->memSizeStaticFields);
        memset(sfield, 0, sharedLib->memSizeStaticFields * sizeof(jint));
        domain->sfields[sharedLib->ndx] = sfield;
    }

    for (i = 0; i < sharedLib->numberOfClasses; i++) {
        int ssize;
        ClassDesc *cd;
        JClass *cl;
        lib->allClasses[i].magic = MAGIC_CLASS;
        lib->allClasses[i].classDesc = &sharedLib->allClasses[i];
        cd = &sharedLib->allClasses[i];
        cl = &lib->allClasses[i];
        ssize = cd->staticFieldsSize;
        cl->objectDesc_flags = OBJFLAGS_EXTERNAL_CLASS;
        cl->magic = MAGIC_CLASS;
        cl->objectDesc_magic = MAGIC_OBJECT;
        addHashKey(cd->name, lib->key, LIB_HASHKEY_LEN);
        /* superclass */
        if (cd->superclass == NULL) {
            cl->superclass = NULL;
        } else {
            cl->superclass = classDesc2Class(domain, sharedLib->allClasses[i].superclass);
        }

        if (ssize != 0) {
            cl->staticFields = &sfield[static_offset];
            static_offset += ssize;
        } else {
            cl->staticFields = 0;
            cd->sfield_offset = 0;
        }
    }

    return lib;
}

static char *testCheckSumAndVersion(char *filename, char *codefile, int size)
{
    jint i, checksum, version;
    char *codefilepos;
    char processor[20];

    codefilepos = codefile;

    checksum = 0;
    for (i = 0; i < size - 4; i++) {
        checksum = (checksum ^ (*(jbyte *) (codefile + i))) & 0xff;
    }

    if (checksum != *(jint *) (codefile + size - 4)) {
        printk(KERN_ERR "Wrong checksum in library\n");
        return NULL;
    }

    readInt(version);
    if (version != CURRENT_COMPILER_VERSION) {
        printk(KERN_ERR "Mismatch between library version and version supported by Jem/JVM\n");
        return NULL;
    }
    readString(processor, sizeof(processor));

    return codefilepos;
}

static SharedLibDesc *loadSharedLibrary(DomainDesc * domain, char *filename, TempMemory * tmp_mem)
{
    jint i, j, k, m;
    jint completeCodeBytes;
    jint completeVtableSize = 0;
    jint completeBytecodeSize = 0;
    char *supername;
    char libname[32];
    SharedLibDesc *lib;
    jint totalNumberOfClasses;
    SharedLibDesc *neededLib;
    char *codefilepos;
    char **string_table;
    jint dummy;
    jint size;
    jint isinterface;
    jint sfields_offset;

    if ((codefilepos = read_codefile(filename, &size)) == NULL) {
        return NULL;
    }

    if ((codefilepos = testCheckSumAndVersion(filename, codefilepos, size)) == NULL) return NULL;

    lib = jemMallocSharedlibdesc(domain, strlen(filename) + 1);
    lib->magic = MAGIC_SLIB;
    strcpy(lib->name, filename);
    lib->ndx = -1;

    /*
       reserved for option fields
     */

    readInt(i);

    /*
       load needed libs
     */

    readInt(lib->numberOfNeededLibs);

    if (lib->numberOfNeededLibs == 0) {
        lib->neededLibs = NULL;
    } else {
        lib->neededLibs = jemMallocSharedlibdesctable(domain, lib->numberOfNeededLibs);
    }

    for (i = 0; i < lib->numberOfNeededLibs; i++) {
        readString(libname, sizeof(libname));
        neededLib = findSharedLib(libname);

        if (neededLib == NULL) {
            /*  could not find a loaded lib, now try to load it  */
            TempMemory *tmp_mem = jemMallocTmp(5000);

            /* FIXME:  shared libraries should not always be loaded into domainzero  */
            //printf("slib %s load %s\n",lib->name,libname);
            neededLib = loadSharedLibrary(domain, libname, tmp_mem);
            if (neededLib != NULL) {
                linksharedlib(domain, neededLib, (jint) specialAllocObject, (jint) vmSpecialAllocArray, tmp_mem);
            }

            jemFreeTmp(tmp_mem);
        }

        lib->neededLibs[i] = neededLib;
    }

    /*
       load meta
     */
    readInt(lib->numberOfMeta);
    lib->meta = jemMallocMetatable(domain, lib->numberOfMeta);
    for (i = 0; i < lib->numberOfMeta; i++) {
        readAllocString(lib->meta[i].var);
        readAllocString(lib->meta[i].val);
    }

    /*
       read string table
     */

    readInt(i);
    if (i == 0) {
        string_table = NULL;
    } else {
        string_table = (char **) jemMallocCode(domain, i * sizeof(char *));
        for (j = 0; j < i; j++)
            readAllocString(string_table[j]);
    }

    /*
       vmsymbol-table
     */

    readInt(i);
    if (i > 0) {
        char symbol[30];
        for (j = 0; j < i; j++) {
            readString(symbol, sizeof(symbol));
            if (j == numberVMOperations) {
                printk(KERN_ERR "Too many symbols to load/n");
                break;
            }
            vmsupport[j].index = 0;
            for (k = 0; k < numberVMOperations; k++) {
                if (strcmp(symbol, vmsupport[k].name) == 0) {
                    vmsupport[j].index = k;
                    break;
                }
            }
        }
    }

    /*
       load classes
     */

    readInt(totalNumberOfClasses);
    lib->numberOfClasses = 0;
    lib->allClasses = jemMallocClassdescs(domain, totalNumberOfClasses);
    memset(lib->allClasses, 0, sizeof(ClassDesc) * totalNumberOfClasses);
    completeCodeBytes = 0;
    sfields_offset = 0;
    lib->memSizeStaticFields = 0;
    for (i = 0; i < totalNumberOfClasses; i++) {
        lib->allClasses[i].classType = CLASSTYPE_CLASS;
        lib->allClasses[i].magic = MAGIC_CLASSDESC;
        lib->allClasses[i].definingLib = lib;

        readStringID(lib->allClasses[i].name);
        addHashKey(lib->allClasses[i].name, lib->key, LIB_HASHKEY_LEN);

        readStringID(supername);
        if (supername[0] == '\0') {
            lib->allClasses[i].superclass = NULL;
        } else {
            ClassDesc *scl = NULL;
            if (strcmp(supername, "java/lang/Object") == 0) {
                scl = java_lang_Object;
            }
            if (scl == NULL)
                scl = findClassDescInSharedLib(lib, supername);
            if (scl == NULL)
                scl = findClassDesc(supername);
            if (scl == NULL)
                printk(KERN_ERR "find superclass\n");
            lib->allClasses[i].superclass = scl;
        }

        readInt(isinterface);
        if (isinterface) {
            lib->allClasses[i].classType = CLASSTYPE_INTERFACE;
        }
        readInt(lib->allClasses[i].numberOfInterfaces);
        if (lib->allClasses[i].numberOfInterfaces > 0) {
            lib->allClasses[i].interfaces = jemMallocClassdesctable(domain, lib->allClasses[i].numberOfInterfaces);
            lib->allClasses[i].ifname =
                jemMallocTmpStringtable(domain, tmp_mem, lib->allClasses[i].numberOfInterfaces);
        } else {
            lib->allClasses[i].interfaces = (ClassDesc **) NULL;
            lib->allClasses[i].ifname = NULL;
        }
        for (j = 0; j < lib->allClasses[i].numberOfInterfaces; j++) {
            readStringID(lib->allClasses[i].ifname[j]);
        }
        readInt(lib->allClasses[i].numberOfMethods);
        if (lib->allClasses[i].numberOfMethods > 0) {
            lib->allClasses[i].methods = jemMallocMethoddescs(domain, lib->allClasses[i].numberOfMethods);
        } else {
            lib->allClasses[i].methods = NULL;
        }
        readInt(lib->allClasses[i].instanceSize);

        /* fieldmap */
        readInt(lib->allClasses[i].mapBytes);
        lib->allClasses[i].map = NULL;
        if (lib->allClasses[i].mapBytes > 0) {
            lib->allClasses[i].map = jemMallocObjectmap(domain, lib->allClasses[i].mapBytes);
            for (j = 0; j < lib->allClasses[i].mapBytes; j++) {
                readByte(lib->allClasses[i].map[j]);
            }
        }

        /* fieldlist */
        readInt(lib->allClasses[i].numberFields);
        lib->allClasses[i].fields = NULL;
        if (lib->allClasses[i].numberFields > 0) {
            lib->allClasses[i].fields = jemMallocFielddescs(domain, lib->allClasses[i].numberFields);
            for (j = 0; j < lib->allClasses[i].numberFields; j++) {
                readStringID(lib->allClasses[i].fields[j].fieldName);
                readStringID(lib->allClasses[i].fields[j].fieldType);
                readInt(lib->allClasses[i].fields[j].fieldOffset);
            }
        }


        /* static fields */
        readInt(lib->allClasses[i].staticFieldsSize);
        lib->allClasses[i].sfield_offset = sfields_offset;
        sfields_offset += lib->allClasses[i].staticFieldsSize;
        lib->memSizeStaticFields += lib->allClasses[i].staticFieldsSize;

        /* static maps */
        readInt(lib->allClasses[i].staticsMapBytes);
        lib->allClasses[i].staticsMap = NULL;
        if (lib->allClasses[i].staticsMapBytes > 0) {
            lib->allClasses[i].staticsMap = jemMallocStaticsmap(domain, lib->allClasses[i].staticsMapBytes);
            for (j = 0; j < lib->allClasses[i].staticsMapBytes; j++) {
                readByte(lib->allClasses[i].staticsMap[j]);
            }
        }
        readInt(dummy);


        completeBytecodeSize += dummy;
        readInt(dummy);

        /* read vtable */
        readInt(lib->allClasses[i].vtableSize);
        completeVtableSize += lib->allClasses[i].vtableSize;
        if (lib->allClasses[i].vtableSize != 0) {
            lib->allClasses[i].vtableSym = jemMallocVtableSym(domain, lib->allClasses[i].vtableSize);
            for (j = 0; j < lib->allClasses[i].vtableSize * 3; j += 3) {
                readStringID(lib->allClasses[i].vtableSym[j]);  /* class */
                readStringID(lib->allClasses[i].vtableSym[j + 1]);  /* name */
                readStringID(lib->allClasses[i].vtableSym[j + 2]);  /* type */
                readInt(dummy);
            }
        } 

        for (j = 0; j < lib->allClasses[i].numberOfMethods; j++) {
            lib->allClasses[i].methods[j].objectDesc_flags = OBJFLAGS_EXTERNAL_METHOD;
            lib->allClasses[i].methods[j].magic = MAGIC_METHODDESC;
            lib->allClasses[i].methods[j].objectDesc_magic = MAGIC_OBJECT;
            if (vmmethodClass)
                lib->allClasses[i].methods[j].objectDesc_vtable = vmmethodClass->vtable;

            readStringID(lib->allClasses[i].methods[j].name);
            readStringID(lib->allClasses[i].methods[j].signature);
            readInt(lib->allClasses[i].methods[j].sizeLocalVars);
            lib->allClasses[i].methods[j].classDesc = &(lib->allClasses[i]);
            readInt(lib->allClasses[i].methods[j].numberOfCodeBytes);
            lib->allClasses[i].methods[j].sizeOfExceptionTable = 0;

            readInt(lib->allClasses[i].methods[j].numberOfArgTypeMapBytes);
            readInt(lib->allClasses[i].methods[j].numberOfArgs);
            lib->allClasses[i].methods[j].argTypeMap = NULL;
            if (lib->allClasses[i].methods[j].numberOfArgTypeMapBytes > 0) {
                lib->allClasses[i].methods[j].argTypeMap =
                    jemMallocArgsmap(domain, lib->allClasses[i].methods[j].numberOfArgTypeMapBytes);
                for (m = 0; m < lib->allClasses[i].methods[j].numberOfArgTypeMapBytes; m++) {
                    readByte(lib->allClasses[i].methods[j].argTypeMap[m]);
                }
            }
            readInt(lib->allClasses[i].methods[j].returnType);
            readInt(lib->allClasses[i].methods[j].flags);

            lib->allClasses[i].methods[j].codeOffset = completeCodeBytes;
            completeCodeBytes += lib->allClasses[i].methods[j].numberOfCodeBytes;
            readInt(lib->allClasses[i].methods[j].numberOfSymbols);
            if (lib->allClasses[i].methods[j].numberOfSymbols > 0) {
                lib->allClasses[i].methods[j].symbols = jemMallocSymboltable(domain, lib->allClasses[i].methods[j].numberOfSymbols);    /* FIXME: alloc them in temp memory ? */
            } else {
                lib->allClasses[i].methods[j].symbols = NULL;
            }
            for (k = 0; k < lib->allClasses[i].methods[j].numberOfSymbols; k++) {
                jint type;
                jint immediateNCIndex;
                jint numBytes;
                jint nextInstrNCIndex;

                readInt(type);
                readInt(immediateNCIndex);
                readInt(numBytes);
                readInt(nextInstrNCIndex);
                if ((immediateNCIndex + numBytes) > lib->allClasses[i].methods[j].numberOfCodeBytes) {
                    printk(KERN_ERR "Wrong patch index: %d in method %s.%s\nMethod has %d codebytes.\nNumber of bytes to patch is %d.\n",
                         (int) immediateNCIndex, lib->allClasses[i].name, lib->allClasses[i].methods[j].name,
                         (int) lib->allClasses[i].methods[j].numberOfCodeBytes, (int) numBytes);
                }
                switch (type) {
                case 0:
                    printk(KERN_ERR "Error: Symboltype 0\n");
                    break;
                case 1:{    /* DomainZeroSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescDomainZero));
                        break;
                    }
                case 2:{    /* ExceptionHandlerSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescExceptionHandler));
                        break;
                    }
                case 3:{    /* DEPFunctionSTEntry */
                        SymbolDescDEPFunction *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescDEPFunction));
                        s = (SymbolDescDEPFunction *) lib->allClasses[i].methods[j].symbols[k];
                        readAllocString(s->className);
                        readAllocString(s->methodName);
                        readAllocString(s->methodSignature);
                        break;
                    }
                case 4:{    /* StaticFieldSTEntry */
                        SymbolDescStaticField *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescStaticField));
                        s = (SymbolDescStaticField *) lib->allClasses[i].methods[j].symbols[k];
                        readStringID(s->className);
                        readInt(s->kind);
                        readInt(s->fieldOffset);
                        break;
                    }
                case 5:{    /* AllocObjectSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescAllocObject));
                        break;
                    }
                case 6:{    /* ClassSTEntry */
                        SymbolDescClass *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescClass));
                        s = (SymbolDescClass *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readStringID(s->className);
                        break;
                    }
                case 7:{    /* DirectMethodCallSTEntry */
                        SymbolDescDirectMethodCall *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescDirectMethodCall));
                        s = (SymbolDescDirectMethodCall *) lib->allClasses[i].methods[j].symbols[k];
                        readStringID(s->className);
                        readStringID(s->methodName);
                        readStringID(s->methodSignature);
                        break;
                    }
                case 8:{    /* StringSTEntry */
                        SymbolDescString *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescString));
                        s = (SymbolDescString *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readStringID(s->value);
                        break;
                    }
                case 9:{    /* AllocArraySTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescAllocArray));
                        break;
                    }
                case 10:{   /* AllocMultiArraySTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescAllocMultiArray));
                        break;
                    }
                case 11:{   /* LongArithmeticSTEntry */
                        SymbolDescLongArithmetic *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescLongArithmetic));
                        s = (SymbolDescLongArithmetic *) lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->operation);
                        break;
                    }
                case 12:{   /* VMSupportSTEntry */
                        SymbolDescVMSupport *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescVMSupport));
                        s = (SymbolDescVMSupport *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->operation);
                        break;
                    }
                case 13:{   /* PrimitiveClassSTEntry */
                        SymbolDescPrimitiveClass *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescPrimitiveClass));
                        s = (SymbolDescPrimitiveClass *) lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->primitiveType);
                        break;
                    }
                case 14:{   /* UnresolvedJump */
                        SymbolDescUnresolvedJump *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescUnresolvedJump));
                        s = (SymbolDescUnresolvedJump *) lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->targetNCIndex);
                        break;
                    }
                case 15:{   /* VMAbsoluteSTEntry */
                        SymbolDescVMSupport *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescVMSupport));
                        s = (SymbolDescVMSupport *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->operation);
                        break;
                    }
                case 16:    // old version
                    {   /* StackMap */
                        SymbolDescStackMap *s;
                        int mapPos;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescStackMap));
                        s = (SymbolDescStackMap *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->n_bytes);
                        readInt(s->n_bits);
                        if (s->n_bytes > 0)
                            s->map = jemMallocStackmap(domain, s->n_bytes);
                        else
                            s->map = NULL;
                        for (mapPos = 0; mapPos < s->n_bytes; mapPos++) {
                            readByte(s->map[mapPos]);
                        }
                        break;
                    }
                case 17:    // new version 
                    {   /* StackMap */
                        SymbolDescStackMap *s;
                        int mapPos;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescStackMap));
                        s = (SymbolDescStackMap *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->immediateNCIndexPre);
                        readInt(s->n_bytes);
                        readInt(s->n_bits);
                        if (s->n_bytes > 0)
                            s->map = jemMallocStackmap(domain, s->n_bytes);
                        else
                            s->map = NULL;
                        for (mapPos = 0; mapPos < s->n_bytes; mapPos++) {
                            readByte(s->map[mapPos]);
                        }
                        break;
                    }
                case 18:{   /* jx.compiler.symbols.ExceptionTableSTEntry %d %d %s %p */
                        SymbolDescExceptionTable *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescExceptionTable));
                        lib->allClasses[i].methods[j].sizeOfExceptionTable++;
                        s = (SymbolDescExceptionTable *) lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->targetNCIndex);  /* UnresolvedJump */
                        readInt(s->rangeStart);
                        readInt(s->rangeEnd);
                        readStringID(s->className);
                        break;
                    }
                case 19:{   /* CurrentThreadPointerSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescThreadPointer));
                        break;
                    }
                case 20:{   /* StackChunkSizeSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescStackChunkSize));
                        break;
                    }
                case 21:{   /* ProfileSTEntry */
                        SymbolDescProfile *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescProfile));
                        s = (SymbolDescProfile *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->kind);
                        break;
                    }
                case 22:{   /* MethodeDescSTEntry */
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescMethodeDesc));
                        break;
                    }
                case 23:{   /* TCBOffsetSTEntry */
                        SymbolDescTCBOffset *s;
                        lib->allClasses[i].methods[j].symbols[k] =
                            jemMallocSymbol(domain, sizeof(SymbolDescTCBOffset));
                        s = (SymbolDescTCBOffset *)
                            lib->allClasses[i].methods[j].symbols[k];
                        readInt(s->kind);
                        break;
                    }
                default:
                    printk(KERN_ERR "Unknown symbol %d\n", (int) type);
                }
                if (lib->allClasses[i].methods[j].symbols[k]) {
                    lib->allClasses[i].methods[j].symbols[k]->type = type;
                    lib->allClasses[i].methods[j].symbols[k]->immediateNCIndex = immediateNCIndex;
                    lib->allClasses[i].methods[j].symbols[k]->numBytes = numBytes;
                    lib->allClasses[i].methods[j].symbols[k]->nextInstrNCIndex = nextInstrNCIndex;
                }
            }

            if (lib->allClasses[i].methods[j].sizeOfExceptionTable > 0) {
                lib->allClasses[i].methods[j].exceptionTable =
                    jemMallocExceptiondescs(domain, lib->allClasses[i].methods[j].sizeOfExceptionTable);
            } else {
                lib->allClasses[i].methods[j].exceptionTable = NULL;
            }

            readInt(lib->allClasses[i].methods[j].numberOfByteCodes);
            if (lib->allClasses[i].methods[j].numberOfByteCodes > 0) {
                lib->allClasses[i].methods[j].bytecodeTable =
                    jemMmallocBytecodetable(domain, lib->allClasses[i].methods[j].numberOfByteCodes);
                for (k = 0; k < lib->allClasses[i].methods[j].numberOfByteCodes; k++) {
                    readInt(lib->allClasses[i].methods[j].bytecodeTable[k].bytecodePos);
                    readInt(lib->allClasses[i].methods[j].bytecodeTable[k].start);
                    readInt(lib->allClasses[i].methods[j].bytecodeTable[k].end);
                }
            } else {
                lib->allClasses[i].methods[j].bytecodeTable = NULL;
            }


            /* read BC -> SOURCELINE mapping */

            readInt(lib->allClasses[i].methods[j].numberOfSourceLines);
            if (lib->allClasses[i].methods[j].numberOfSourceLines > 0) {
                lib->allClasses[i].methods[j].sourceLineTable =
                    jemMallocSourcelinetable(domain, lib->allClasses[i].methods[j].numberOfSourceLines);
                for (k = 0; k < lib->allClasses[i].methods[j].numberOfSourceLines; k++) {
                    readInt(lib->allClasses[i].methods[j].sourceLineTable[k].startBytecode);
                    readInt(lib->allClasses[i].methods[j].sourceLineTable[k].lineNumber);
                }
            } else {
                lib->allClasses[i].methods[j].sourceLineTable = NULL;
            }


        }
        lib->numberOfClasses++;
    }

    /* read code */
    lib->code = jemMallocNativecode(domainZero, completeCodeBytes);
    lib->codeBytes = completeCodeBytes;
    lib->vtablesize = completeVtableSize;
    lib->bytecodes = completeBytecodeSize;
    readCode(lib->code, completeCodeBytes);

    lib->ndx = sharedLibsIndexNumber;
    sharedLibsIndexNumber++;

    lib->next = sharedLibs;
    sharedLibs = lib;

    return lib;
}

static void patchByte(code_t code, SymbolDesc * symbol, jint value)
{
    char *addr;
    addr = (char *) ((char *) code + symbol->immediateNCIndex);
    addr[0] = value & 0xff;
}

static void patchConstant(code_t code, SymbolDesc * symbol, jint value)
{
    char *addr;
    addr = (char *) ((char *) code + symbol->immediateNCIndex);
    addr[0] = value & 0xff;
    addr[1] = (value >> 8) & 0xff;
    addr[2] = (value >> 16) & 0xff;
    addr[3] = (value >> 24) & 0xff;
}

static void patchRelativeAddress(code_t code, SymbolDesc * symbol, jint function)
{
    jint relAddr = -((jint) code + (jint) symbol->nextInstrNCIndex - (jint) function);
    patchConstant(code, symbol, relAddr);
}

static void patchStaticFieldAddress(code_t code, SymbolDesc * symbol)
{
    ClassDesc *c;
    SymbolDescStaticField *s;
    int libindex;

    s = (SymbolDescStaticField *) symbol;
    c = findClassDesc(s->className);
    if (c == NULL) {
        printk(KERN_ERR "Could not find class at %s:%d\n", __FILE__, __LINE__);
        return;
    }
    libindex = c->definingLib->ndx;

    switch (s->kind) {
    case 0:
        patchConstant(code, symbol, c->sfield_offset + s->fieldOffset);
        break;
    case 1:
        if (libindex < 0 || libindex > sharedLibsIndexNumber) {
            printk(KERN_ERR "Invalid lib index at %s:%d\n", __FILE__, __LINE__);
            return;
        }
        patchConstant(code, symbol, c->definingLib->ndx * sizeof(jint *));
        break;
    case 2:
        patchConstant(code, symbol, (c->sfield_offset + s->fieldOffset) * sizeof(jint *));
        break;
    case 3:
        patchConstant(code, symbol, (0xffffffff << STACK_CHUNK));
        break;
    default:
        printk(KERN_ERR "Unknown static field symbol %d at %s:%d\n", (int) s->kind, __FILE__, __LINE__);
    }
}

static void patchClassPointer(code_t code, SymbolDesc * symbol)
{
    ClassDesc *c;
    SymbolDescClass *s = (SymbolDescClass *) symbol;


    if (*s->className == '[') {
        c = findSharedArrayClassDesc(s->className);
    } else {
        c = findClassDesc(s->className);
    }

    if (c == NULL) {
        printk(KERN_ERR "Link error: Required class not found: %s at %s:%d", s->className, __FILE__, __LINE__);
        return;
    }

    patchConstant(code, symbol, (jint) c);
}

static void should_not_be_called(void)
{
    printk(KERN_ERR "Unknown method called.\n");
}

static void patchDirectMethodAddress(code_t code, SymbolDesc * symbol)
{
    ClassDesc *c;
    MethodDesc *m;
    SymbolDescDirectMethodCall *s = (SymbolDescDirectMethodCall *) symbol;

    findClassDescAndMethod(s->className, s->methodName, s->methodSignature, &c, &m);

    if (m == NULL) {
        printk(KERN_WARNING "!!! no direct method found: %s.%s%s\n", s->className, s->methodName, s->methodSignature);
        patchRelativeAddress(code, symbol, (jint) should_not_be_called);
        return;
    }

    if (m->code == NULL) {
        printk(KERN_WARNING "!!! bad direct method address found: %s.%s%s\n", s->className, s->methodName, s->methodSignature);
        patchRelativeAddress(code, symbol, (jint) should_not_be_called);
        return;
    }

    patchRelativeAddress(code, symbol, (jint) m->code);
}

static void patchStringAddress(code_t code, SymbolDesc * symbol)
{
    ObjectDesc *obj;
    SymbolDescString *s = (SymbolDescString *) symbol;

    obj = newDomainZeroString(s->value);    /* shared libs are part of domainzero and so are these constant strings */

    if (obj == NULL) {
        printk(KERN_ERR "Could not create string object at %s:%d\n", __FILE__, __LINE__);
        return;
    }
    patchConstant(code, symbol, (jint) obj);
}

static jlong longDiv(jlong a, jlong b)
{
    return a / b;
}

static jlong longRem(jlong a, jlong b)
{
    return a % b;
}

static jlong longShr(jlong a, jint b)
{
    return a >> (b & 63);
}

static jlong longShl(jlong a, jint b)
{
    return a << (b & 63);
}

static jlong longUShr(jlong a, jint b)
{
    b &= 63;
    return a >= 0 ? a >> b : (a >> b) + (2 << ~b);
}

static jint longCmp(jlong a, jlong b)
{
    if (a > b)
        return 1;
    if (a < b)
        return -1;
    return 0;
}

static jlong longMul(jlong a, jlong b)
{
    return a * b;
}

code_t longops[] = {
    0,
    (code_t) longDiv,
    (code_t) longRem,
    (code_t) longShr,
    (code_t) longShl,
    (code_t) longUShr,
    (code_t) longCmp,
    (code_t) longMul,
};

static void patchRelativeLongAddress(code_t code, SymbolDesc * symbol)
{
    SymbolDescLongArithmetic *s = (SymbolDescLongArithmetic *) symbol;
    patchRelativeAddress(code, symbol, (jint) (longops[s->operation]));
}

static void patchPrimitiveClassPointer(code_t code, SymbolDesc * symbol)
{
    SymbolDescPrimitiveClass *s = (SymbolDescPrimitiveClass *) symbol;
    jint t = -1;
    switch ((jint) (s->primitiveType)) {
    case 0:
        t = (jint) class_I->classDesc;
        break;
    case 1:
        t = (jint) class_J->classDesc;
        break;
    case 2:
        t = (jint) class_F->classDesc;
        break;
    case 3:
        t = (jint) class_D->classDesc;
        break;
    case 5:
        t = (jint) class_B->classDesc;
        break;
    case 6:
        t = (jint) class_C->classDesc;
        break;
    case 7:
        t = (jint) class_S->classDesc;
        break;
    case 8:
        t = (jint) class_Z->classDesc;
        break;
    default:
        printk(KERN_ERR "Not a primitive at %s:%d\n", __FILE__, __LINE__);
    }
    patchConstant(code, symbol, t);
}

static void patchUnresolvedJump(code_t code, SymbolDesc * symbol)
{
    u32 targetAddr;
    SymbolDescUnresolvedJump *s = (SymbolDescUnresolvedJump *) symbol;

    targetAddr = (u32) s->targetNCIndex;
    targetAddr += (u32) code;
    patchConstant(code, symbol, (jint) targetAddr);
}



static void setCodeStart(SharedLibDesc * lib)
{
    jint i, j;
    for (i = 0; i < lib->numberOfClasses; i++) {
        for (j = 0; j < lib->allClasses[i].numberOfMethods; j++) {
            if (lib->allClasses[i].methods[j].numberOfCodeBytes == 0) {
                /* abstract method */
                lib->allClasses[i].methods[j].code = 0;
            } else {
                /* resolve absolute code address */
                lib->allClasses[i].methods[j].code =
                    (code_t) (lib->code + lib->allClasses[i].methods[j].codeOffset);
            }
        }
    }
}


static void createVTable(DomainDesc * domain, ClassDesc * c)
{
    char **vtable;
    vtable = (char **) jemMallocVtable(domain, c->vtableSize + 1);
    memset(vtable, 0, 4 * c->vtableSize + 4);

    c->vtable = (code_t *) vtable + 1;  /* classpointer is at negative offset */
    *vtable = (char *) c;

    if (c != java_lang_Object) {
        c->methodVtable = jemMallocMethodVtable(domain, c->vtableSize);
    }
}

static void patchMethodSymbols(MethodDesc * method, code_t code, jint allocObjectFunction, jint allocArrayFunction)
{
    int exCount, k;
    exCount = 0;
    for (k = 0; k < method->numberOfSymbols; k++) {
        if (method->symbols[k] == NULL)
            continue;
        switch (method->symbols[k]->type) {
        case 0:
            break;
        case 1:{    /* DomainZeroSTEntry */
                break;
            }
        case 2:{    /* ExceptionHandlerSTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             exceptionHandler);
                break;
            }
        case 3:{    /* DEPFunctionSTEntry */
                break;
            }
        case 4:{    /* StaticFieldSTEntry */
                patchStaticFieldAddress(code, method->symbols[k]);
                break;
            }
        case 5:{    /* AllocObjectSTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             allocObjectFunction);
                break;
            }
        case 6:{    /* ClassSTEntry */
                patchClassPointer(code, method->symbols[k]);
                break;
            }
        case 7:{    /* DirectMethodCallSTEntry */
                patchDirectMethodAddress(code, method->symbols[k]);
                break;
            }
        case 8:{    /* StringSTEntry */
                patchStringAddress(code, method->symbols[k]);
                break;
            }
        case 9:{    /* AllocArraySTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             allocArrayFunction);
                break;
            }
        case 10:{   /* AllocMultiArraySTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             vmSpecialAllocMultiArray);
                break;
            }
        case 11:{   /* LongArithmeticSTEntry */
                patchRelativeLongAddress(code, method->symbols[k]);
                break;
            }
        case 12:{   /* VMSupportSTEntry */
                SymbolDescVMSupport *s = (SymbolDescVMSupport *) method->symbols[k];
                if (s->operation <= 0 || s->operation > numberVMOperations) {
                    printk(KERN_ERR "Wrong vmsupport index at %s:%d\n", __FILE__, __LINE__);
                    break;
                }
                patchRelativeAddress(code, method->symbols[k], (jint) VMSUPPORT(s->operation).fkt);
                break;
            }
        case 13:{   /* PrimitveClassSTEntry */
                patchPrimitiveClassPointer(code, method->symbols[k]);
                break;
            }
        case 14:{   /* UnresolvedJump */
                patchUnresolvedJump(code, method->symbols[k]);
                break;
            }
        case 15:{   /* VMAbsoluteSTEntry */
                SymbolDescVMSupport *s = (SymbolDescVMSupport *) method->symbols[k];
                if (s->operation <= 0 || s->operation > numberVMOperations) {
                    printk(KERN_ERR "Wrong vmsupport index at %s:%d\n", __FILE__, __LINE__);
                    break;
                }
                patchConstant(code, method->symbols[k], (jint) VMSUPPORT(s->operation).fkt);
                break;
            }
        case 16:
        case 17:{   /* StackMap */
                break;
            }
        case 18:{   /* jx.compiler.symbols.ExceptionTableSTEntry  */
                SymbolDescExceptionTable *s;
                ExceptionDesc *e;

                s = (SymbolDescExceptionTable *) method->symbols[k];
                e = method->exceptionTable;
                e[exCount].addr = s->targetNCIndex;
                e[exCount].start = s->rangeStart;
                e[exCount].end = s->rangeEnd;

                if (strcmp(s->className, "any") == 0) {
                    e[exCount].type = NULL;
                } else {
                    e[exCount].type = findClassDesc(s->className);
                }
                exCount++;

                break;
            }
        case 19:{   /* CurrentThreadPointerSTEntry */
                patchConstant(code, method->symbols[k], (jint) curthrP());
                break;
            }
        case 20:{   /* StackChunkSizeSTEntry */
                patchByte(code, method->symbols[k], (jint) STACK_CHUNK);
                break;
            }
        case 21:{   /* ProfileSTEntry */
                SymbolDescProfile *s;
                s = (SymbolDescProfile *) method->symbols[k];
                switch (s->kind) {
#ifdef EVENT_LOG
                case 0:{
                        patchConstant(code, s, &events);
                        break;
                    }
                case 1:{
                        patchConstant(code, s, &n_events);
                        break;
                    }
                case 2:{
                        patchConstant(code, s, MAX_EVENTS);
                        break;
                    }
                case 3:{
                        jint event_id = cpuManager_createNewEventID(method->name);
                        if (event_id < 0) {
                            printk(KERN_WARNING "warn: out of event types MAX_EVENT_TYPES=%d", MAX_EVENT_TYPES);
                            event_id = 0;
                        }
                        patchConstant(code, s, event_id);
                        break;
                    }
#endif
#ifdef PROFILE
                case 4:{    /* ProfileCallSTEntry */
                        method->isprofiled = JNI_TRUE;
                        patchRelativeAddress(code, method->symbols[k], (jint)
                                     profile_call);
                        break;
                    }
                case 5:{    /* ProfilePtrOffsetSTEntry */
                        patchConstant(code, method->symbols[k], (char *)
                                  &(curthr()->profile) - (char *)
                                  curthr());
                        break;
                    }
                case 6:{    /* TraceCallSTEntry */
                        patchRelativeAddress(code, method->symbols[k], (jint)
                                     profile_trace);
                        break;
                    }
#endif
                default:
                    printk(KERN_ERR "Linker unknown profile symbol %d\n", (int) s->kind);
                }
                break;
            }
        case 22:{   /* MethodeDescSTEntry */
                patchConstant(code, method->symbols[k], (jint) method);
                break;
            }
        case 23:{   /* TCBOffsetSTEntry */
                SymbolDescTCBOffset *s;
                s = (SymbolDescTCBOffset *) method->symbols[k];
                switch (s->kind) {
                case 1:{
                        patchConstant(code, (SymbolDesc *) s, (char *) &(curthr()->stack) - (char *)
                                  curthr());
                        break;
                    }
                default:
                    printk(KERN_ERR "Linker unknown TCBOffset symbol %d\n", (int) s->kind);
                }
                break;
            }
        default:
            printk(KERN_ERR "Linker unknown symbol %d\n", (int) method->symbols[k]->type);
        }
    }
}

void repatchMethodSymbols(MethodDesc * method, code_t code, jint allocObjectFunction, jint allocArrayFunction)
{
    int exCount, k;
    exCount = 0;
    for (k = 0; k < method->numberOfSymbols; k++) {
        if (method->symbols[k] == NULL)
            continue;
        switch (method->symbols[k]->type) {
        case 0:
            break;
        case 1:{    /* DomainZeroSTEntry */
                break;
            }
        case 2:{    /* ExceptionHandlerSTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             exceptionHandler);
                break;
            }
        case 3:{    /* DEPFunctionSTEntry */
                break;
            }
        case 4:{    /* StaticFieldSTEntry */
                patchStaticFieldAddress(code, method->symbols[k]);
                break;
            }
        case 5:{    /* AllocObjectSTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint) allocObjectFunction); /* FIXME */
                break;
            }
        case 6:{    /* ClassSTEntry */
                patchClassPointer(code, method->symbols[k]);
                break;
            }
        case 7:{    /* DirectMethodCallSTEntry */
                patchDirectMethodAddress(code, method->symbols[k]);
                break;
            }
        case 8:{    /* StringSTEntry */
                patchStringAddress(code, method->symbols[k]);
                break;
            }
        case 9:{    /* AllocArraySTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             allocArrayFunction);
                break;
            }
        case 10:{   /* AllocMultiArraySTEntry */
                patchRelativeAddress(code, method->symbols[k], (jint)
                             vmSpecialAllocMultiArray);
                break;
            }
        case 11:{   /* LongArithmeticSTEntry */
                patchRelativeLongAddress(code, method->symbols[k]);
                break;
            }
        case 12:{   /* VMSupportSTEntry */
                SymbolDescVMSupport *s = (SymbolDescVMSupport *) method->symbols[k];
                if (s->operation <= 0 || s->operation > numberVMOperations) {
                    printk(KERN_ERR "Wrong vmsupport index at %s:%d\n", __FILE__, __LINE__);
                    break;
                }
                patchRelativeAddress(code, method->symbols[k], (jint) VMSUPPORT(s->operation).fkt);
                break;
            }
        case 13:{   /* PrimitveClassSTEntry */
                patchPrimitiveClassPointer(code, method->symbols[k]);
                break;
            }
        case 14:{   /* UnresolvedJump */
                patchUnresolvedJump(code, method->symbols[k]);
                break;
            }
        case 15:{   /* VMAbsoluteSTEntry */
                break;
            }
        case 16:
        case 17:{   /* StackMap */
                break;
            }
        case 18:{   /* jx.compiler.symbols.ExceptionTableSTEntry  */
                break;
            }
        case 19:{   /* CurrentThreadPointerSTEntry */
                break;
            }
        case 20:{   /* StackChunkSizeSTEntry */
                break;
            }
        case 21:{   /* ProfileSTEntry */
                SymbolDescProfile *s;
                s = (SymbolDescProfile *) method->symbols[k];
                switch (s->kind) {
#ifdef EVENT_LOG
                case 0:{
                        /*patchConstant(code,s,&events); */
                        break;
                    }
                case 1:{
                        /*patchConstant(code,s,&n_events); */
                        break;
                    }
                case 2:{
                        /*patchConstant(code,s,MAX_EVENTS); */
                        break;
                    }
                case 3:{
                        /*
                           jint event_id = cpuManager_createNewEventID(method->name);
                           if (event_id<0) {
                           printf("warn: out of event types MAX_EVENT_TYPES=%d",MAX_EVENT_TYPES);
                           event_id=0;
                           }
                           patchConstant(code,s,event_id);
                         */
                        break;
                    }
#endif
#ifdef PROFILE
                case 4:{    /* ProfileCallSTEntry */
                        method->isprofiled = JNI_TRUE;
                        patchRelativeAddress(code, method->symbols[k], (jint)
                                     profile_call);
                        break;
                    }
                case 5:{    /* ProfilePtrOffsetSTEntry */
                        patchConstant(code, method->symbols[k], (char *)
                                  &(curthr()->profile) - (char *)
                                  curthr());
                        break;
                    }
                case 6:{    /* TraceCallSTEntry */
                        patchRelativeAddress(code, method->symbols[k], (jint)
                                     profile_trace);
                        break;
                    }
#endif
                default:
                    printk(KERN_ERR "Linker unknown profile symbol %d\n", (int) s->kind);
                }
                break;
            }
        case 22:{   /* MethodeDescSTEntry */
                patchConstant(code, method->symbols[k], (jint) method);
                break;
            }
        default:
            printk(KERN_ERR "Linker unknown symbol %d\n", (int) method->symbols[k]->type);
        }
    }
}


static void linksharedlib(DomainDesc * domain, SharedLibDesc * lib, jint allocObjectFunction, jint allocArrayFunction,
           TempMemory * tmp_mem)
{
    jint i, j;
    MethodDesc *method;

    setCodeStart(lib);  /* must resolve code start addresses first because of direct method calls */
    for (i = 0; i < lib->numberOfClasses; i++) {
        ClassDesc *superclass;
        /* create vtable */
        superclass = lib->allClasses[i].superclass;
        createVTable(domain, &(lib->allClasses[i]));

    }

    for (i = 0; i < lib->numberOfClasses; i++) {

        /* find and link interfaces to this class */
        for (j = 0; j < lib->allClasses[i].numberOfInterfaces; j++) {
            ClassDesc *scl = NULL;
            char *ifname;
            ifname = lib->allClasses[i].ifname[j];
            scl = findClassDescInSharedLib(lib, ifname);
            if (scl == NULL)
                scl = findClassDesc(ifname);
            if (scl == NULL) {
                printk(KERN_ERR "Cannot find interface %s while linking class %s of library %s\n", ifname,
                      lib->allClasses[i].name, lib->name);
            }
            lib->allClasses[i].interfaces[j] = scl;
        }

        /* patch addresses in method code */
        for (j = 0; j < lib->allClasses[i].numberOfMethods; j++) {
            patchMethodSymbols(&(lib->allClasses[i].methods[j]), lib->allClasses[i].methods[j].code,
                       allocObjectFunction, allocArrayFunction);
        }

        /* build vtable */
        if (lib->allClasses[i].vtableSize < 11)
            printk(KERN_ERR "vtableSize<11\n");

        for (j = 0; j < lib->allClasses[i].vtableSize; j++) {
            if (lib->allClasses[i].vtableSym[j * 3][0] == '\0')
                continue;   /* hole */
            method =
                findMethodInSharedLibs(lib->allClasses[i].vtableSym[j * 3], lib->allClasses[i].vtableSym[j * 3 + 1],
                           lib->allClasses[i].vtableSym[j * 3 + 2]);

            lib->allClasses[i].vtable[j] = method->code;
            lib->allClasses[i].methodVtable[j] = method;
        }
    }
}

static void findClassAndMethodInLib(LibDesc * lib, char *classname, char *methodname, char *signature, JClass ** classFound,
                 MethodDesc ** methodFound)
{
    jint i, j;
    ClassDesc *cl;

    for (i = 0; i < lib->numberOfClasses; i++) {
        cl = lib->allClasses[i].classDesc;
        if (strcmp(classname, cl->name) == 0) {
            for (j = 0; j < cl->numberOfMethods; j++) {
                if (strcmp(methodname, cl->methods[j].name)
                    == 0) {
                    if (strcmp(signature, cl->methods[j].signature) == 0) {
                        *classFound = &(lib->allClasses[i]);
                        *methodFound = &(cl->methods[j]);
                        return;
                    }
                }
            }
        }
    }
    *classFound = NULL;
    *methodFound = NULL;
}


void findClassDescAndMethodInLib(SharedLibDesc * lib, char *classname, char *methodname, char *signature, ClassDesc ** classFound,
                 MethodDesc ** methodFound)
{
    jint i, j;

    if (strcmp(classname, "java/lang/Object") == 0) {   /* Object is part of every lib */
        findClassDescAndMethodInObject(lib, classname, methodname, signature, classFound, methodFound);
        return;
    }

    for (i = 0; i < lib->numberOfClasses; i++) {
        if (strcmp(classname, lib->allClasses[i].name) == 0) {
            for (j = 0; j < lib->allClasses[i].numberOfMethods; j++) {
                if (strcmp(methodname, lib->allClasses[i].methods[j].name) == 0) {
                    if (strcmp(signature, lib->allClasses[i].methods[j].signature) == 0) {
                        *classFound = &(lib->allClasses[i]);
                        *methodFound = &(lib->allClasses[i].methods[j]);
                        return;
                    }
                }
            }
        }
    }
    *classFound = NULL;
    *methodFound = NULL;
}

static void findClassDescAndMethod(char *classname, char *methodname, char *signature, ClassDesc ** classFound,
                MethodDesc ** methodFound)
{
    SharedLibDesc *sharedLib;
    *classFound = NULL;
    *methodFound = NULL;
    sharedLib = sharedLibs;

    while (sharedLib != NULL) {
        if (testHashKey(classname, sharedLib->key, LIB_HASHKEY_LEN)) {
            findClassDescAndMethodInLib(sharedLib, classname, methodname, signature, classFound, methodFound);
            if (*classFound != NULL)
                return;
        }
        sharedLib = sharedLib->next;
    }

    sharedLib = sharedLibs;
    while (sharedLib != NULL) {
        if (!testHashKey(classname, sharedLib->key, LIB_HASHKEY_LEN)) {
            findClassDescAndMethodInLib(sharedLib, classname, methodname, signature, classFound, methodFound);
            if (*classFound != NULL)
                return;
        }
        sharedLib = sharedLib->next;
    }

}

static MethodDesc *findMethodInSharedLibs(char *classname, char *methodname, char *signature)
{
    ClassDesc *classFound;
    MethodDesc *methodFound;

    findClassDescAndMethod(classname, methodname, signature, &classFound, &methodFound);
    return methodFound;
}


static MethodDesc *findMethodInLib(LibDesc * lib, char *classname, char *methodname, char *signature)
{
    JClass *classFound;
    MethodDesc *methodFound;
    //printf("findMethodInLib: %s %s %s %s\n", lib->sharedLib->name, classname, methodname, signature);
    findClassAndMethodInLib(lib, classname, methodname, signature, &classFound, &methodFound);
    return methodFound;
}

MethodDesc *findMethod(DomainDesc * domain, char *classname, char *methodname, char *signature)
{
    jint i;
    MethodDesc *m;

    //printf("findMethod: %s %s %s\n", classname, methodname, signature);
    for (i = 0; i < domain->numberOfLibs; i++) {
        if (testHashKey(classname, domain->libs[i]->key, LIB_HASHKEY_LEN)) {
            m = findMethodInLib(domain->libs[i], classname, methodname, signature);
            if (m != NULL) {
                return m;
            }
        }
    }

    /* java/lang/Object */
    if (domain->numberOfLibs > 0)
        m = findMethodInLib(domain->libs[0], classname, methodname, signature);

    return NULL;
}

static code_t findVirtualMethodCode(DomainDesc * domain, char *classname, char *methodname, char *signature, ObjectDesc * obj)
{
    jint j;
    JClass *c = findClass(domain, classname);
    ClassDesc *cl = c->classDesc;
    if (cl == NULL) {
        printk(KERN_ERR "Cannot find class %s\n", classname);
    }
    for (j = 0; j < cl->vtableSize; j++) {
        if (cl->vtableSym[j * 3][0] == '\0')
            continue;   /* hole */
        if ((strcmp(cl->vtableSym[j * 3 + 1], methodname) == 0)
            && (strcmp(cl->vtableSym[j * 3 + 2], signature) == 0)) {
            return obj->vtable[j];
        }
    }
    printk(KERN_ERR "Cannot find method %s::%s%s\n", classname, methodname, signature);
    return NULL;
}


jint findDEPMethodIndex(DomainDesc * domain, char *className, char *methodName, char *signature)
{
    jint j;
    ClassDesc *cl;
    JClass *c = findClass(domain, className);
    if (c == NULL) {
        printk(KERN_ERR "Cannot find DEP %s\n", className);
        return 0;
    }
    cl = c->classDesc;
    for (j = 0; j < cl->vtableSize; j++) {
        if (cl->vtableSym[j * 3][0] == '\0')
            continue;   /* hole */
        if ((strcmp(cl->vtableSym[j * 3 + 1], methodName) == 0)
            && (strcmp(cl->vtableSym[j * 3 + 2], signature) == 0)) {
            return j;
        }
    }
    printk(KERN_ERR "Cannot find DEP method %s:: %s%s\n", className, methodName, signature);
    return 0;
}

void callClassConstructor(JClass * cl)
{
    jint i;
    code_t c;

    if (cl->state == CLASS_READY)
        return;
    cl->state = CLASS_READY;
    for (i = 0; i < cl->classDesc->numberOfMethods; i++) {
        if (strcmp("<clinit>", cl->classDesc->methods[i].name) == 0) {
            c = (code_t) cl->classDesc->methods[i].code;
            c();
            break;
        }
    }
}

/* this conforms not exactly to the JVM spec */
void callClassConstructors(DomainDesc * domain, LibDesc * lib)
{
    jint i;

    if (domain != curdom())
        printk(KERN_ERR "WRONG DOMAIN called in class constructor\n");

    if (lib->initialized == 1)
        return;     /* already done */

    for (i = 0; i < lib->numberOfClasses; i++)
        callClassConstructor(&lib->allClasses[i]);

    lib->initialized = 1;
}

u32 executeStatic(DomainDesc * domain, char *className, char *methodname, char *signature, jint * params, jint params_size)
{
    MethodDesc *method;
    code_t c;

    method = findMethod(domain, className, methodname, signature);
    if (method == 0) {
        printk(KERN_ERR "StaticMethod not found: %s.%s%s\n", className, methodname, signature);
        return 0;
    }

    c = (code_t) method->code;

    return callnative_static(params, c, params_size);
}

void executeSpecial(DomainDesc * domain, char *className, char *methodname, char *signature, ObjectDesc * obj, jint * params,
            int params_size)
{
    MethodDesc *method;
    code_t c;
    jint ret;

    method = findMethod(domain, className, methodname, signature);
    if (method == 0) {
        printk(KERN_ERR "SpecialMethod not found: %s %s %s\n", className, methodname, signature);
        return;
    }

    c = (code_t) method->code;
    ret = callnative_special(params, obj, c, params_size);
}

void executeInterface(DomainDesc * domain, char *className, char *methodname, char *signature, ObjectDesc * obj, jint * params,
              int params_size)
{
    code_t c;
    jint ret;
    c = findVirtualMethodCode(domain, className, methodname, signature, obj);
    ret = callnative_special(params, obj, c, params_size);
}

u32 executeVirtual(DomainDesc * domain, char *className, char *methodname, char *signature, ObjectDesc * obj, jint * params,
            int params_size)
{
    code_t c;
    c = findVirtualMethodCode(domain, className, methodname, signature, obj);
    return callnative_special(params, obj, c, params_size);
}

char backup;

void set_breakpoint(DomainDesc * domain, char *classname, char *methodname, char *signature, int offset)
{
    MethodDesc *m = findMethod(domain, classname, methodname, signature);
    backup = ((u8 *) (m->code))[offset];
    ((u8 *) (m->code))[offset] = 0xcc;
}


