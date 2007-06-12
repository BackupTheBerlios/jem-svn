//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm, All rights reserved.
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
// zero_ComponentManager.c
//
// DomainZero CPUManager
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
#include <native/timer.h>
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
#include "malloc.h"
#include "gc_alloc.h"
#include "symfind.h"

static unsigned int threadNum=0;

/*
 *
 * CpuManager Portal
 *
 */
jint cpuManager_receive(ObjectDesc * self, ObjectDesc * obj)
{
    exceptionHandler(THROW_RuntimeException);
    return -1;
}

static jint cpuManager_yield(ObjectDesc * self)
{
    rt_task_yield();
    return 0;
}

static void cpuManager_sleep(ObjectDesc * self, jint msec, jint nsec)
{
    printk(KERN_ERR "SLEEP NO LONGER SUPPORTED. USE jx.timer.SleepManager\n");
}

static jint cpuManager_wait(ThreadDesc * source)
{
    return 0;
}

static jint cpuManager_notify(ThreadDesc * source)
{
    return 0;
}

static jint cpuManager_notifyAll(ThreadDesc * source)
{
    return 0;
}

jint cpuManager_dump(ObjectDesc * self, ObjectDesc * msg, ObjectDesc * ref)
{
    char c[128];
    ClassDesc *cl;
    c[0] = '\0';
    if (msg != NULL)
        stringToChar(msg, c, sizeof(c));
    if (ref == NULL)
        return -1;
    if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_MEMORY) {
        printk(KERN_INFO "   MEMORY\n");
    } else if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_PORTAL) {
        printk(KERN_INFO "   PORTAL: index=%d\n", ((Proxy *) ref)->index);
    } else if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_OBJECT) {
        cl = obj2ClassDesc(ref);
        printk(KERN_INFO "     INSTANCE of class: %s\n", cl->name);
    } else if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_CAS) {
        printk(KERN_INFO "     CAS\n");
    } else if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_SERVICE) {
        DEPDesc *s = (DEPDesc *) ref;
        printk(KERN_INFO "     Service: interface=%s\n", s->interface->name);
    } else if ((getObjFlags(ref) & FLAGS_MASK) == OBJFLAGS_SERVICE_POOL) {
        printk(KERN_INFO "     Servicepool\n");
    } else {
        printk(KERN_INFO "     unknown object type. flags=(%p)\n", (void *) getObjFlags(ref));
    }
    return 0;
}

static jint cpuManager_switchTo(ObjectDesc * self, ObjectDesc * cpuState)
{
    return 0;
}

static ObjectDesc *cpuManager_getCPUState(ObjectDesc * self)
{
    return thread2CPUState(curthr());
}

static void cpuManager_block(ObjectDesc * self)
{
    threadblock();
    return;
}

static void cpuManager_blockIfNotUnblocked(ObjectDesc * self)
{
    if (!curthr()->unblockedWithoutBeingBlocked) {
        threadblock();
    }
}

static void cpuManager_clearUnblockFlag(ObjectDesc * self)
{
    curthr()->unblockedWithoutBeingBlocked = 0;
}

static jint cpuManager_waitUntilBlocked(ObjectDesc * self, CPUStateProxy * cpuStateProxy)
{
    ThreadDesc *cpuState = cpuState2thread((ObjectDesc *) cpuStateProxy);

    while (cpuState->state != STATE_BLOCKEDUSER) {
        rt_task_yield();
    }
    return 0;
}

static void cpuManager_join(ObjectDesc * self, CPUStateProxy * cpuStateProxy)
{
    ThreadDesc *cpuState = cpuState2thread((ObjectDesc *) cpuStateProxy);
    while (cpuState->state != STATE_ZOMBIE) {
        rt_task_yield();
    }
}

/* needs only one Parameter */
static inline jboolean _cpuManager_unblock(ThreadDesc * cpuState)
{
    jboolean ret;
    if (cpuState->state != STATE_BLOCKEDUSER) {
        ret = JNI_FALSE;
    } else {
        threadunblock(cpuState);
        ret = JNI_TRUE;
    }
    return ret;
}

static jboolean cpuManager_unblock(ObjectDesc * self, CPUStateProxy * cpuStateProxy)
{
    jboolean ret;
    ThreadDesc *cpuState;
    if (cpuStateProxy == NULL)
        exceptionHandler(THROW_RuntimeException);

    cpuState = cpuState2thread((ObjectDesc *) cpuStateProxy);
    if (cpuState == NULL) {
        ret = JNI_FALSE;
        goto finish;
    }
    if (cpuState->state != STATE_BLOCKEDUSER) {
        cpuState->unblockedWithoutBeingBlocked = 1;
        ret = JNI_FALSE;
    } else {
        threadunblock(cpuState);
        ret = JNI_TRUE;
    }
  finish:
    return ret;
}

static void start_thread_using_entry(void *dummy)
{
    ObjectDesc *entry = (ObjectDesc *) curthr()->entry;
    executeInterface(curdom(), "jx/zero/ThreadEntry", "run", "()V", entry, 0, 0);
}

static ObjectDesc *cpuManager_createCPUState(ObjectDesc * self, ObjectDesc * entry)
{
    ThreadDesc  *thread;
    DomainDesc  *sourceDomain;
    char        tName[15];

    sourceDomain = curdom();
    sprintf(tName, "wk%d", threadNum++);

    thread =
        createThreadInMem(sourceDomain, start_thread_using_entry, NULL, entry, 4096, STATE_INIT,
                          getJVMConfig()->defaultThreadPrio, tName);
    return thread2CPUState(thread);
}

static char *get_state(ThreadDesc * t)
{
	switch (t->state) {
	case STATE_INIT:
		return "INIT";
	case STATE_RUNNABLE:
		return "RUNNABLE";
	case STATE_ZOMBIE:
		return "ZOMBIE";
	case STATE_SLEEPING:
		return "SLEEPING";
	case STATE_WAITING:
		return "WAITING";
	case STATE_PORTAL_WAIT_FOR_RCV:
		return "PORTAL_WAIT_FOR_RCV";
	case STATE_PORTAL_WAIT_FOR_SND:
		return "PORTAL_WAIT_FOR_SND";
	case STATE_PORTAL_WAIT_FOR_RET:
		return "PORTAL_WAIT_FOR_RET";
	case STATE_BLOCKEDUSER:
		return "BLOCKEDUSER";
	case STATE_AVAILABLE:
		return "AVAILABLE";
	case STATE_WAIT_ONESHOT:
		return "STATE_WAIT_ONESHOT";
	case STATE_PORTAL_WAIT_FOR_PARAMCOPY:
		return "STATE_PORTAL_WAIT_FOR_PARAMCOPY";
	case STATE_PORTAL_WAIT_FOR_RETCOPY:
		return "STATE_PORTAL_WAIT_FOR_RETCOPY";
	}
	return "UNKNOWN";
}

static jboolean cpuManager_start(ObjectDesc * self, CPUStateProxy * cpuStateProxy)
{
    ThreadDesc *cpuState = cpuState2thread(( ObjectDesc *)cpuStateProxy);
    jboolean result = JNI_TRUE;
    int rc;

    if (cpuState->state != STATE_INIT) {
        printk(KERN_WARNING "Start: Thread %p is in state %d (%s)\n", cpuState, cpuState->state, get_state(cpuState));
        result = JNI_FALSE;
    } else {
        cpuState->state = STATE_RUNNABLE;
        if ((rc = rt_task_start(&cpuState->task, start_thread_using_entry, cpuState->createParam)) < 0) {
            printk(KERN_ERR "Error starting Jem thread task, rc=%d\n", rc);
            return JNI_FALSE;
        }
    }
    return result;
}

static void cpuManager_printStackTrace(ObjectDesc * self)
{
    //u32 *base = (u32 *) & self - 2;
    //printStackTrace("STACK: ", curthr(), base);
}

extern ClassDesc *atomicvariableClass;
extern ClassDesc *casClass;
extern ClassDesc *credentialvariableClass;

static ObjectDesc *cpuManager_getAtomicVariable(ObjectDesc * self)
{
    return (ObjectDesc *) allocAtomicVariableProxyInDomain(curdom(), atomicvariableClass);
}

static ObjectDesc *cpuManager_getCAS(ObjectDesc * self, ObjectDesc * classNameObj, ObjectDesc * fieldNameObj)
{
    CASProxy *p;
    char value[128];
    ClassDesc *c;
    u32 o;
    int i;

    /* TODO: check access permissions to class and field ! */
    if (classNameObj == NULL || fieldNameObj == NULL)
        exceptionHandlerInternal("classname or fieldname == null");

    stringToChar(classNameObj, value, sizeof(value));
    for (i = 0; i < strlen(value); i++) {
        if (value[i] == '.')
            value[i] = '/';
    }
    c = findClassDesc(value);
    if (c == NULL)
        exceptionHandlerInternal("no such class");
    stringToChar(fieldNameObj, value, sizeof(value));
    o = findFieldOffset(c, value);
    if (o == -1)
        exceptionHandlerInternal("no such field");
    p = allocCASProxyInDomain(curdom(), casClass, o);
    return (ObjectDesc *) p;
}

static void cpuManager_setThreadName(ObjectDesc * self, ObjectDesc * name)
{
}

static void cpuManager_attachToThread(ObjectDesc * self, ObjectDesc * portalParameter)
{
    curthr()->portalParameter = portalParameter;
}

static ObjectDesc *cpuManager_getAttachedObject(ObjectDesc * self)
{
    return curthr()->portalParameter;
}

static ObjectDesc *cpuManager_getCredential(ObjectDesc * self)
{
    return (ObjectDesc *) allocCredentialProxyInDomain(curdom(), credentialvariableClass, curdom()->id);
}

static void cpuManager_recordEvent(ObjectDesc * self, jint nr)
{
}

static void cpuManager_recordEventWithInfo(ObjectDesc * self, jint nr, jint info)
{
}

static jint cpuManager_createNewEvent(ObjectDesc * self, ObjectDesc * label)
{
    return -1;
}

static ObjectDesc *cpuManager_getClass(ObjectDesc * self, ObjectDesc * nameObj)
{
    char name[128];
    JClass *cl;
    int i;
    ObjectDesc *vmclassObj;
    if (nameObj == NULL)
        return NULL;
    stringToChar(nameObj, name, sizeof(name));
    for (i = 0; i < strlen(name); i++) {
        if (name[i] == '.')
            name[i] = '/';
    }
    cl = findClass(curdom(), name);
    if (cl == NULL)
        return NULL;
    vmclassObj = class2Obj(cl);
    return vmclassObj;
}

static ObjectDesc *cpuManager_getVMClass(ObjectDesc * self, ObjectDesc * obj)
{
    ClassDesc *c = obj2ClassDesc(obj);
    JClass *cl = classDesc2Class(curdom(), c);
    ObjectDesc *vmclassObj = class2Obj(cl);
    return vmclassObj;
}

static VMObjectProxy *cpuManager_getVMObject(ObjectDesc * self)
{
    return allocVMObjectProxyInDomain(curdom());
}

static void cpuManager_assertInterruptEnabled(ObjectDesc * self)
{
}

static void cpuManager_executeClassConstructors(ObjectDesc * self, jint id)
{
    LibDesc *lib = (LibDesc *) id;
    SharedLibDesc *sharedLib = lib->sharedLib;
    int i;
    for (i = 0; i < sharedLib->numberOfNeededLibs; i++) {
        LibDesc *l = sharedLib2Lib(curdom(), sharedLib->neededLibs[i]);
        if (!l->initialized)
            callClassConstructors(curdom(), l);
    }
    callClassConstructors(curdom(), lib);
}

static jint cpuManager_inheritServiceThread(ObjectDesc * self)
{
    return 0;
}

static void cpuManager_reboot(ObjectDesc * self)
{
    return;
}

static jint cpuManager_getStackDepth(ObjectDesc * self)
{
    ThreadDesc *thread = curthr();
    u32 n = 0;
    u32 *eip, *ebp;
    u32 *sp = (u32 *) & self - 2; /* stackpointer */

    while (sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
        ebp = (u32 *) * sp++;
        eip = (u32 *) * sp++;
        sp = ebp;
        n++;
    }
    return n;
}

static ObjectDesc *cpuManager_getStackFrameClassName(ObjectDesc * self, jint depth)
{
    ThreadDesc *thread = curthr();
    u32 n = 0;
    u32 *eip=NULL, *ebp;
    u32 *sp = (u32 *) & self - 2; /* stackpointer */
    jint bytecodePos, i;
    MethodDesc *method;
    ClassDesc *classInfo;
    while (n <= depth && sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
        ebp = (u32 *) * sp++;
        eip = (u32 *) * sp++;
        sp = ebp;
        n++;
    }
    if (findMethodAtAddrInDomain(curdom(), (char *) eip, &method, &classInfo, &bytecodePos, &i) == 0) {
        return newString(curdom(), classInfo->name);
    } else {
        return newString(curdom(), "core:");
    }
}

static ObjectDesc *cpuManager_getStackFrameMethodName(ObjectDesc * self, jint depth)
{
    ThreadDesc *thread = curthr();
    u32 n = 0;
    u32 *eip=NULL, *ebp;
    u32 *sp = (u32 *) & self - 2; /* stackpointer */
    jint bytecodePos, i;
    MethodDesc *method;
    ClassDesc *classInfo;
    while (n <= depth && sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
        ebp = (u32 *) * sp++;
        eip = (u32 *) * sp++;
        sp = ebp;
        n++;
    }
    if (findMethodAtAddrInDomain(curdom(), (char *) eip, &method, &classInfo, &bytecodePos, &i) == 0) {
        return newString(curdom(), method->name);
    } else {
        char cname[KSYM_NAME_LEN+1];
        if (findCoreSymbol((jint) eip, cname) != NULL)
            return newString(curdom(), cname);

        return NULL;
    }
}

static jint cpuManager_getStackFrameLine(ObjectDesc * self, jint depth)
{
    ThreadDesc *thread = curthr();
    u32 n = 0;
    u32 *eip=NULL, *ebp;
    u32 *sp = (u32 *) & self - 2; /* stackpointer */
    jint bytecodePos, i;
    MethodDesc *method;
    ClassDesc *classInfo;
    while (n <= depth && sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
        ebp = (u32 *) * sp++;
        eip = (u32 *) * sp++;
        sp = ebp;
        n++;
    }
    if (findMethodAtAddrInDomain(curdom(), (char *) eip, &method, &classInfo, &bytecodePos, &i) == 0) {
        return i;
    } else {
        return -1;
    }
}

static jint cpuManager_getStackFrameBytecode(ObjectDesc * self, jint depth)
{
    ThreadDesc *thread = curthr();
    u32 n = 0;
    u32 *eip=NULL, *ebp;
    u32 *sp = (u32 *) & self - 2; /* stackpointer */
    jint bytecodePos, i;
    MethodDesc *method;
    ClassDesc *classInfo;
    while (n <= depth && sp > (u32 *) thread->stack && sp < (u32 *) thread->stackTop) {
        ebp = (u32 *) * sp++;
        eip = (u32 *) * sp++;
        sp = ebp;
        n++;
    }
    if (findMethodAtAddrInDomain(curdom(), (char *) eip, &method, &classInfo, &bytecodePos, &i) == 0) {
        return bytecodePos;
    } else {
        return -1;
    }
}

static jint cpuManager_inhibitScheduling(ObjectDesc * self)
{
    return 0;
}

static jint cpuManager_allowScheduling(ObjectDesc * self)
{
    return 0;
}

MethodInfoDesc cpuManagerMethods[] = {
    {
     "receive", "", (code_t) cpuManager_receive}
    , {
       "yield", "", (code_t) cpuManager_yield}
    , {
       "sleep", "", (code_t) cpuManager_sleep}
    , {
       "wait", "", (code_t) cpuManager_wait}
    , {
       "notify", "", (code_t) cpuManager_notify}
    , {
       "notifyAll", "", (code_t) cpuManager_notifyAll}
    , {
       "dump", "", (code_t) cpuManager_dump}
    , {
       "switchTo", "", (code_t) cpuManager_switchTo}
    , {
       "getCPUState", "", (code_t) cpuManager_getCPUState}
    , {
       "block", "", (code_t) cpuManager_block}
    , {
       "blockIfNotUnblocked", "", (code_t) cpuManager_blockIfNotUnblocked}
    , {
       "clearUnblockFlag", "", (code_t) cpuManager_clearUnblockFlag}
    , {
       "join", "", (code_t) cpuManager_join}
    , {
       "waitUntilBlocked", "", (code_t) cpuManager_waitUntilBlocked}
    , {
       "unblock", "", (code_t) cpuManager_unblock}
    , {
       "createCPUState", "", (code_t) cpuManager_createCPUState}
    , {
       "start", "", (code_t) cpuManager_start}
    , {
       "printStackTrace", "", (code_t) cpuManager_printStackTrace}
    , {
       "getAtomicVariable", "", (code_t) cpuManager_getAtomicVariable}
    , {
       "setThreadName", "", (code_t) cpuManager_setThreadName}
    , {
       "attachToThread", "", (code_t) cpuManager_attachToThread}
    , {
       "getAttachedObject", "", (code_t) cpuManager_getAttachedObject}
    , {
       "getCredential", "", (code_t) cpuManager_getCredential}
    , {
       "createNewEvent", "", (code_t) cpuManager_createNewEvent}
    , {
       "recordEvent", "", (code_t) cpuManager_recordEvent}
    , {
       "recordEventWithInfo", "", (code_t) cpuManager_recordEventWithInfo}
    , {
       "getCAS", "", (code_t) cpuManager_getCAS}
    , {
       "getClass", "", (code_t) cpuManager_getClass}
    , {
       "getVMClass", "", (code_t) cpuManager_getVMClass}
    , {
       "getVMObject", "", (code_t) cpuManager_getVMObject}
    , {
       "assertInterruptEnabled", "", (code_t) cpuManager_assertInterruptEnabled}
    , {
       "executeClassConstructors", "", (code_t) cpuManager_executeClassConstructors}
    , {
       "inheritServiceThread", "", (code_t) cpuManager_inheritServiceThread}
    , {
       "reboot", "", (code_t) cpuManager_reboot}
    , {
       "getStackDepth", "", (code_t) cpuManager_getStackDepth}
    , {
       "getStackFrameClassName", "", (code_t) cpuManager_getStackFrameClassName}
    , {
       "getStackFrameMethodName", "", (code_t) cpuManager_getStackFrameMethodName}
    , {
       "getStackFrameLine", "", (code_t) cpuManager_getStackFrameLine}
    , {
       "getStackFrameBytecode", "", (code_t) cpuManager_getStackFrameBytecode}
    , {
       "inhibitScheduling", "", (code_t) cpuManager_inhibitScheduling}
    , {
       "allowScheduling", "", (code_t) cpuManager_allowScheduling}
    ,
};

void init_cpumanager_portal(void)
{
    init_zero_dep_without_thread("jx/zero/CPUManager", "CPUManager", cpuManagerMethods, sizeof(cpuManagerMethods),
                     "<jx/zero/CPUManager>");
}

