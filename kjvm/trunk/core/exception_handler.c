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
// exception_handler.c
// 
// Jem/JVM Exception handling.
// 
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
#include "execjava.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "exception_handler.h"
#include "load.h"
#include "gc_alloc.h"
#include "symfind.h"
#include "vmsupport.h"

static void uncaught_exception(ObjectDesc * self);

ObjectDesc *createExceptionInDomain(DomainDesc * domain, char *exception, char *details)
{
    ObjectDesc *self;
    jint params[1];
    jint ret;
    ClassDesc *exclass;
    code_t c;
    int i;

    exclass = findClassDesc(exception);
    if (exclass == NULL)
        return NULL;

    /* fixme: we alloc memory with irqs off */
    self = (ObjectDesc *) allocObjectInDomain(domain, exclass);
    if (self == NULL)
        return NULL;

    if (details == NULL) {
        /* call constructor <init>()V */
        for (i = 0; i < exclass->numberOfMethods; i++) {
            if ((strcmp("<init>", exclass->methods[i].name) == 0)
                && (strcmp("()V", exclass->methods[i].signature)
                    == 0)) {

                c = (code_t) exclass->methods[i].code;
                ret = callnative_special(params, self, c, 0);
                break;
            }
        }
    } else {
        /* call constructor <init>(Ljava/lang/String;)V */
        for (i = 0; i < exclass->numberOfMethods; i++) {
            if ((strcmp("<init>", exclass->methods[i].name) == 0)
                && (strcmp("(Ljava/lang/String;)V", exclass->methods[i].signature) == 0)) {

                params[0] = (jint) newString(domain, details);
                c = (code_t) exclass->methods[i].code;
                ret = callnative_special(params, self, c, 1);
                break;
            }
        }
    }

    return self;
}

void throw_RuntimeException(jint dummy)
{
}

void throw_NullPointerException(jint dummy)
{
    u32 *base;
    ObjectDesc *self;
    base = (u32 *) & dummy - 2;    /* stackpointer */
    self = createExceptionInDomain(curdom(), "java/lang/NullPointerException", NULL);
    throw_exception(self, base);
}

void throw_OutOfMemoryError(jint dummy)
{
}

void throw_IndexOutOfBounds(jint dummy)
{
    u32 *base;
    ObjectDesc *self;
    base = (u32 *) & dummy - 2;    /* stackpointer */
    self = createExceptionInDomain(curdom(), "java/lang/IndexOutOfBoundsException", "memory access out of range");
    throw_exception(self, base);
}

void throw_ArrayIndexOutOfBounds(jint dummy)
{
    u32 *base;
    ObjectDesc *self;
    base = (u32 *) & dummy - 2;    /* stackpointer */
    self = createExceptionInDomain(curdom(), "java/lang/IndexOutOfBoundsException", "array index out of bounds");
    throw_exception(self, base);
}

void throw_StackOverflowError()
{
    printk(KERN_ERR "Out of stack (%s:%d)\n", __FILE__, __LINE__);
    return;
}

void throw_ArithmeticException(jint dummy)
{
    u32 *base;
    ObjectDesc *self;
    base = (u32 *) & dummy - 2;    /* stackpointer */
    self = createExceptionInDomain(curdom(), "java/lang/ArithmeticException", NULL);
    throw_exception(self, base);
}

void exceptionHandlerInternal(char *msg)
{
    ObjectDesc *ex = createExceptionInDomain(curdom(), "java/lang/RuntimeException", msg);
    throw_exception(ex, ((u32 *) & msg - 2));
}

void exceptionHandler(jint * p)
{
    exceptionHandlerMsg(p, "unknown");
}

void exceptionHandlerMsg(jint * p, char *msg)
{
    u32 *base;
    ObjectDesc *self = NULL;

    base = (u32 *) & p - 2;    /* stackpointer */
    self = NULL;

    /* first check whether exception was thrown during GC -> terminate this domain */
    if (curdom()->gc.gcThread != NULL && curdom()->gc.gcThread->state != STATE_AVAILABLE) {
        domain_panic(curdom(), "exception during GC");
    }

    if (p == THROW_RuntimeException) {
        self = createExceptionInDomain(curdom(), "java/lang/RuntimeException", msg);
    } else if (p == THROW_NullPointerException) {
        self = createExceptionInDomain(curdom(), "java/lang/NullPointerException", NULL);
    } else if (p == THROW_OutOfMemoryError) {
        self = createExceptionInDomain(curdom(), "java/lang/OutOfMemoryError", NULL);
    } else if (p == THROW_MemoryIndexOutOfBounds) {
        self = createExceptionInDomain(curdom(), "java/lang/IndexOutOfBoundsException", "memory access out of range");
    } else if (p == THROW_StackOverflowError) {
        self = createExceptionInDomain(curdom(), "java/lang/StackOverflowError", NULL);
    } else if (p == THROW_ArithmeticException) {
        self = createExceptionInDomain(curdom(), "java/lang/ArithmeticException", "divied by zero");
    } else if (p == THROW_MagicNumber) {
        u32 *arg;
        arg = (u32 *) & p;
        domain_panic(curdom(), "wrong magicnumber");
    } else if (p == THROW_ParanoidCheck) {
        printk(KERN_WARNING "PARANOID CHECK EXCEPTION\n");
    } else if (p == THROW_StackJam) {
        printk(KERN_ERR "STACK MIXUP EXCEPTION\n");
    } else if (p == THROW_ArrayIndexOutOfBounds) {
        self = createExceptionInDomain(curdom(), "java/lang/IndexOutOfBoundsException", "array index out of bounds");
    } else if (p == THROW_UnsupportedByteCode) {
        self =
        createExceptionInDomain(curdom(), "java/lang/RuntimeException", "unsupported bytecode (long/double/float)");
    } else if (p == THROW_InvalidMemory) {
        self = createExceptionInDomain(curdom(), "java/lang/RuntimeException", "memory invalid (revoked)");
    } else if (p == THROW_MemoryExhaustedException) {
        self = createExceptionInDomain(curdom(), "jx/zero/MemoryExhaustedException", NULL);
    } else if (p == THROW_MemoryExhaustedException) {
        self = createExceptionInDomain(curdom(), "jx/zero/DomainTerminatedException", NULL);
    } else if (p != NULL) {
        self = (ObjectDesc *) p;
    }

    if (self != NULL)
        throw_exception(self, base);

    uncaught_exception(self);
}

static void uncaught_exception(ObjectDesc * self)
{
    thread_exit();
}

void throw_exception(ObjectDesc * exception, u32 * sp)
{
    u32 *ebp, *eip;
    jint bytecodePos, i;
    MethodDesc *method;
    ClassDesc *classInfo, *exclass;
    ThreadDesc *thread;
    DomainDesc *domain;
    ExceptionDesc *e;

    domain = curdom();
    exclass = obj2ClassDesc(exception);

    if (exclass != NULL) {
        thread = curthr();

        while (sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
            ebp = (u32 *) * sp++;
            eip = (u32 *) * sp++;
            /* catch all exceptions that reach a service entry point (callnative_special_portal) */
            if (in_portalcall(eip)) {
                ThreadDesc *source = curthr()->mostRecentlyCalledBy;
                /* check if domain still exists */
                if (curthr()->callerDomainID != curthr()->callerDomain->id
                    || curthr()->callerDomain->state != DOMAIN_STATE_ACTIVE) {
                    /* caller disappeared, ignore exception */
                    return;
                } else {
                    u32 quota = getJVMConfig()->receivePortalQuota;
                    source->portalReturn = copy_reference(curdom(), source->domain, exception, &quota);
                    source->portalReturnType = PORTAL_RETURN_TYPE_EXCEPTION;
                }
                reinit_service_thread();
            }

            if (findMethodAtAddrInDomain(curdom(), (char *) eip, &method, &classInfo, &bytecodePos, &i) == 0) {
                /* compute the top of the operand stack */
                sp = ebp - method->sizeLocalVars;
                /* lookup exception handler */
                for (i = 0; i < method->sizeOfExceptionTable; i++) {
                    e = &(method->exceptionTable[i]);

                    if (bytecodePos >= e->start && bytecodePos < e->end) {
                        if (e->type == NULL || check_assign(e->type, exclass) == JNI_TRUE) {
                            /* push exception */
                            sp--;
                            sp[0] = (u32) exception;
                            /* jump into exception handler (point of no return) */
                            callnative_handler(ebp, sp, (char *) ((u32) e->addr + (u32) method->code));
                        }
                    }
                }   /* end of exception table ( no exception handler found ) */
            }
            /* leaf method frame */
            sp = ebp;
        }       /* scan stack */
    }
    /* known exception */
    uncaught_exception(exception);
}
