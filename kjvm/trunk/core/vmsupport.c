//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 Christopher Stone. 
// Copyright (C) 1997-2001 The JX Group. 
// Copyright (C) 1998-2002 Michael Golm. 
// Copyright (C) 2001-2002 Christian Wawersich. 
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
// vmsupport.c
//
// Jem/JVM JVM support functions
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "vmsupport.h"
#include "load.h"
#include "exception_handler.h"
#include "zero.h"

static jint vm_spinlock = 1;
static jboolean isArrayClass(ClassDesc * c);


static jboolean vm_instanceof(ObjectDesc * obj, ClassDesc * c)
{
    ClassDesc *oclass;

    if (obj == NULL)
        return JNI_TRUE;

    oclass = handle2ClassDesc(&obj);

    if (oclass == c)
        return JNI_TRUE;

    return check_assign(c, oclass);
}

/*
 * in use
 * obj is checked by gc -> gc save
 */
static void vm_checkcast(ObjectDesc * obj, ClassDesc * c)
{
    ClassDesc *oclass;

    if (obj == NULL)
        return;     /* null reference can be casted to every type */

    oclass = handle2ClassDesc(&obj);
    if (check_assign(c, oclass) != JNI_TRUE) {
        ObjectDesc *ex = createExceptionInDomain(curdom(), "java/lang/ClassCastException", NULL);
        throw_exception(ex, ((u32 *) & obj - 2));
    }

    return;
}

static void vm_arraycopy(ObjectDesc * src, unsigned int soff, ObjectDesc * dst, unsigned int doff, unsigned int count)
{
    //ObjectDesc **sh,**dh;
    ClassDesc *sclass, *dclass;
    u32 *sdata, *ddata;
    jint ssize, dsize;

    sclass = obj2ClassDesc(src);
    if (!isArrayClass(sclass)) {
        if (strcmp(sclass->name, "<Array>") != 0)
            exceptionHandler(THROW_RuntimeException);
    }
    dclass = obj2ClassDesc(dst);
    if (!isArrayClass(dclass)) {
        if (strcmp(dclass->name, "<Array>") != 0)
            exceptionHandler(THROW_RuntimeException);
    }
    if (!check_assign(dclass, sclass)) {
        exceptionHandler(THROW_RuntimeException);
    }

    ssize = ((ArrayDesc *) src)->size;
    sdata = (u32 *) ((ArrayDesc *) src)->data;
    dsize = ((ArrayDesc *) dst)->size;
    ddata = (u32 *) ((ArrayDesc *) dst)->data;

    if ((doff + count > dsize) || (soff + count > ssize)) {
        exceptionHandler(THROW_ArrayIndexOutOfBounds);
    }

    if ((sdata == ddata) && (doff > soff)) {
        int i;
        for (i = 0; i < count; ++i) {
            ddata[--doff] = sdata[--soff];
        }
    } else {
        memcpy(ddata + doff, sdata + soff, count*sizeof(u32));
    }
}

static void vm_arraycopy_right(ObjectDesc * src, unsigned int soff, ObjectDesc * dst, unsigned int doff, unsigned int count)
{
    ClassDesc *sclass, *dclass;
    u32 *sdata, *ddata;
    jint ssize, dsize;

    sclass = obj2ClassDesc(src);
    dclass = obj2ClassDesc(dst);
    if (!check_assign(dclass, sclass))
        exceptionHandler(THROW_RuntimeException);

    ssize = ((ArrayDesc *) src)->size;
    sdata = (u32 *) ((ArrayDesc *) src)->data;
    dsize = ((ArrayDesc *) dst)->size;
    ddata = (u32 *) ((ArrayDesc *) dst)->data;

    if ((doff + count > dsize) || (soff + count > ssize)) {
        exceptionHandler(THROW_ArrayIndexOutOfBounds);
    }

    memcpy(ddata + doff, sdata + soff, count*sizeof(u32));

}

static void vm_arraycopy_left(ObjectDesc * src, unsigned int soff, ObjectDesc * dst, unsigned int doff, unsigned int count)
{
    exceptionHandler(THROW_RuntimeException);
}

static void vm_test_cinit(ClassDesc * c)
{
    JClass *cl;
    cl = classDesc2Class(curdom(), c);
    if (cl->state != CLASS_READY) {
        callClassConstructor(cl);
    }
    return;
}


/*
 * gc save
 */
static jint vm_getStaticsAddr(ClassDesc * c)
{
    return (jint) curdom()->sfields;
}

static jint vm_getStaticsAddr2(ClassDesc * c)
{
    return (jint) (curdom()->sfields[c->definingLib->ndx]);
}


static void vm_put_field32(ObjectDesc * obj, jint offset, jint value)
{
    jint *o = (jint *) obj;
    o[offset] = value;
}

static void vm_put_static_field32(ClassDesc * c, jint offset, jint value)
{
    jint *s;

    s = (curdom()->sfields[c->definingLib->ndx]);

    s[offset] = value;
}

static void vm_put_array_field32(ArrayDesc * arr, jint index, jint value)
{
    if (index < arr->size) {
        arr->data[index] = value;
    } else {
        exceptionHandler(THROW_ArrayIndexOutOfBounds);
    }
}


static void vm_map_put32(ObjectDesc * obj, jint offset, jint value)
{
    MappedMemoryProxy *o = (MappedMemoryProxy *) obj;
    *((u32 *) ((u8 *) o->mem + offset)) = value;
}

static jint vm_map_get32(ObjectDesc * obj, jint offset)
{
    jint *o = (jint *) obj;
    return o[offset];
}


/*
 * gc save
 */
static jint vm_breakpoint(jint a)
{
    asm("int $3");
    return 0;
}

/*
 * unsave
 */
static jint vm_getclassname(ObjectDesc * ref)
{
    JClass *cl;
    cl = obj2class(ref);
    return (jint) newStringFromClassname(curdom(), cl->classDesc->name);
}

/*
 * unused
 */
static jint vm_getinstancesize(ClassDesc * c)
{
    exceptionHandler(THROW_UnsupportedByteCode);
    return c->instanceSize;
}

/*
 * unused
 */
static jint vm_isprimitive(ClassDesc * c)
{
    exceptionHandler(THROW_UnsupportedByteCode);
    return c->classType == CLASSTYPE_PRIMITIVE;
}

/*
 * usnused
 */
static void vm_monitorenter(ObjectDesc * o)
{
    printk(KERN_INFO "MONTITORENTER\n");
}

/*
 * unused
 */
static void vm_monitorexit(ObjectDesc * o)
{
    printk(KERN_INFO "MONTITOREXIT\n");
}

/*
 * helper funktions
 */

static jboolean isArrayClass(ClassDesc * c)
{
    return *c->name == '[';
}

static ClassDesc *get_element_class(ClassDesc * c)
{
    return ((ClassDesc *) c)->elementClass;
}

static jboolean is_subclass_of(ClassDesc * subclass, ClassDesc * superclass)
{
    if (subclass == superclass)
        return JNI_TRUE;
    while ((subclass = subclass->superclass) != NULL) {
        if (subclass == superclass)
            return JNI_TRUE;
    }

    return JNI_FALSE;
}

static jboolean is_subinterface_of(ClassDesc * subif, ClassDesc * superif)
{
    if (subif == superif)
        return JNI_TRUE;
    while ((subif = subif->superclass) != NULL) {
        if (subif == superif)
            return JNI_TRUE;
        if (implements_interface(subif, superif))
            return JNI_TRUE;
    }

    return JNI_FALSE;
}

jboolean implements_interface(ClassDesc * c, ClassDesc * ifa)
{
    int i;

    if (is_subinterface_of(c, ifa))
        return JNI_TRUE;

    for (i = 0; i < c->numberOfInterfaces; i++) {
        if (implements_interface(c->interfaces[i], ifa))
            return JNI_TRUE;
        if (is_subinterface_of(c->interfaces[i], ifa))
            return JNI_TRUE;
    }

    return JNI_FALSE;
}

jboolean is_interface(ClassDesc * c)
{
    return c->classType == CLASSTYPE_INTERFACE;
}

jboolean check_assign(ClassDesc * variable_class, ClassDesc * reference_class)
{
    if (variable_class == reference_class)
        return JNI_TRUE;
    if (isArrayClass(reference_class)) {
        /* class must be java.lang.Object or
         * the element class of obj must be a subclass of the element class of class
         */
        if (variable_class == java_lang_Object)
            return JNI_TRUE;
        if (!isArrayClass(variable_class))
            return JNI_FALSE;
        if (check_assign(get_element_class(variable_class), get_element_class(reference_class)))
            return JNI_TRUE;
        return JNI_FALSE;
    }
    if (is_subclass_of(reference_class, variable_class))
        return JNI_TRUE;
    if (is_interface(variable_class)
        && implements_interface(reference_class, variable_class))
        return JNI_TRUE;
    return JNI_FALSE;
}

static ObjectDesc *vm_getnaming(ObjectDesc * src)
{
    return (ObjectDesc *) getInitialNaming();
}

void vm_unsupported()
{
    printk(KERN_ERR "vmfkt not supported\n");
}

vm_fkt_table_t vmsupport[] = {
    {"vm_unsupported", 0, (code_t) vm_unsupported},
    {"vm_instanceof", 0, (code_t) vm_instanceof},
    {"nil", 0, (code_t) vm_unsupported},    //vm_dep_send,
    {"vm_breakpoint", 0, (code_t) vm_breakpoint},
    {"vm_getclassname", 0, (code_t) vm_getclassname},
    {"vm_getinstancesize", 0, (code_t) vm_getinstancesize},
    {"vm_isprimitive", 0, (code_t) vm_isprimitive},
    {"vm_monitorenter", 0, (code_t) vm_monitorenter},
    {"vm_monitorexit", 0, (code_t) vm_monitorexit},
    {"vm_checkcast", 0, (code_t) vm_checkcast},
    {"vm_test_cinit", 0, (code_t) vm_test_cinit},
    {"vm_getStaticsAddr", 0, (code_t) vm_getStaticsAddr},
    {"vm_getStaticsAddr2", 0, (code_t) vm_getStaticsAddr2},
    {"vm_spinlock", 0, (code_t) & vm_spinlock},
    {"vm_arraycopy", 0, (code_t) vm_arraycopy},
    {"vm_arraycopy_right", 0, (code_t) vm_arraycopy_right},
    {"vm_arraycopy_left", 0, (code_t) vm_arraycopy_left},
    {"vm_getnaming", 0, (code_t) vm_getnaming},
    {"vm_stackoverflow", 0, (code_t) throw_StackOverflowError},
    {"vm_arrindex", 0, (code_t) throw_ArrayIndexOutOfBounds},
    {"vm_nullchk", 0, (code_t) throw_NullPointerException},
    {"vm_arith", 0, (code_t) throw_ArithmeticException},
    {"vm_put_field32", 0, (code_t) vm_put_field32},
    {"vm_put_array_field32", 0, (code_t) vm_put_array_field32},
    {"vm_put_static_field32", 0, (code_t) vm_put_static_field32},
    {"vm_map_get32", 0, (code_t) vm_map_get32},
    {"vm_map_put32", 0, (code_t) vm_map_put32},
};

jint numberVMOperations = sizeof(vmsupport) / sizeof(vm_fkt_table_t);

