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
//==============================================================================
#ifndef ZERO_H
#define ZERO_H

#include "load.h"
#include "thread.h"
#include "zero_Memory.h"



extern jint gc_methodindex;

extern ObjectDesc *memoryManagerInstance;
extern DEPDesc *ihandlers[NR_IRQS];
extern DomainDesc *idomains[CONFIG_NR_CPUS][NR_IRQS];
extern ThreadDesc *ithreads[CONFIG_NR_CPUS][NR_IRQS];
extern u32 imissed[NR_IRQS];
extern u32 idelayed[NR_IRQS];
extern u32 iprocessed[CONFIG_NR_CPUS][NR_IRQS];
extern ObjectDesc *ifirstlevel_object[CONFIG_NR_CPUS][NR_IRQS];
extern u32 ifirstlevel_happened[CONFIG_NR_CPUS][NR_IRQS];
extern u32 ifirstlevel_processed[CONFIG_NR_CPUS][NR_IRQS];
extern struct irqInfos iInfos[NR_IRQS];

struct nameValue_s {
	Proxy *obj;
	char *name;
	struct nameValue_s *next;
};
extern struct nameValue_s *nameValue;

extern code_t extern_panic;

/* CPUState */

static inline ObjectDesc *thread2CPUState(ThreadDesc * thread)
{
	if (thread == NULL)
		return NULL;
	return (ObjectDesc *) (((u32 *) thread) - 1);
}

static inline ThreadDesc *cpuState2thread(ObjectDesc * obj)
{
	if (obj == NULL)
		return NULL;
	if ((getObjFlags(obj) & FLAGS_MASK) == OBJFLAGS_CPUSTATE) {
		return &(((ThreadDescProxy *)obj)->desc);
	} 
    else if ((getObjFlags(obj) & FLAGS_MASK) == OBJFLAGS_FOREIGN_CPUSTATE) {
		return findThreadDesc((ThreadDescForeignProxy*)obj);
	} 
	return NULL;
}


/* CPU */

static inline ObjectDesc *cpuDesc2Obj(CPUDesc * cpu)
{
	return (ObjectDesc *) (((u32 *) cpu) - 1);
}

static inline CPUDesc *obj2cpuDesc(ObjectDesc * obj)
{
	return (CPUDesc *) (((u32 *) obj) + 1);
}

/**
 * Class -> jx/zero/VMClass (Object)
 */
static inline ObjectDesc *class2Obj(Class * cl)
{
	return (ObjectDesc *) & (cl->objectDesc_vtable);
}

/**
 * jx/zero/VMClass (Object) -> Class
 */
static inline JClass *obj2class(ObjectDesc * obj)
{
	return (JClass *) (((u32 *) obj) - 1 - XMOFF);
}

/**
 * Method -> jx/zero/VMMethod (Object)
 */
static inline ObjectDesc *method2Obj(MethodDesc * m)
{
	return (ObjectDesc *) & (m->objectDesc_vtable);
}

/**
 * jx/zero/VMMethod (Object) -> Method
 */
static inline MethodDesc *obj2method(ObjectDesc * obj)
{
	return (MethodDesc *) (((u32 *) obj) - 1 - XMOFF);
}



/*************/
/* functions */

void init_irq_data(void);
void init_zero_from_lib(DomainDesc * domain, SharedLibDesc * zeroLib);
ClassDesc *createObjectClassDesc();
JClass *createObjectClass(ClassDesc * java_lang_Object);
void createArrayObjectVTableProto(DomainDesc * domain);
ClassDesc *init_zero_class(char *ifname, MethodInfoDesc * methods,
			   jint size, jint instanceSize, jbyte * typeMap,
			   char *subname);
jint findZeroLibMethodIndex(DomainDesc * domain, char *className,
			    char *methodName, char *signature);
void SMPcpuManager_register_LLScheduler(ObjectDesc * self,
					ObjectDesc * cpu,
					ObjectDesc * new_sched);
void installObjectVtable(ClassDesc * c);
void installInitialNaming(DomainDesc * srcDomain, DomainDesc * dstDomain,
			  Proxy * naming);
Proxy *getInitialNaming();


/* in zero_DomainManager */
DomainProxy *domainManager_createDomain(ObjectDesc * self,
					ObjectDesc * dname,
					ArrayDesc * cpuObjs,
					ArrayDesc * HLSNames,
					ObjectDesc * dcodeName,
					ArrayDesc * libsName,
					ObjectDesc * startClassName,
					jint gcinfo0, jint gcinfo1, jint gcinfo2, ObjectDesc* gcinfo3, jint gcinfo4,
					jint codeSize,
					ArrayDesc * argv,
					ObjectDesc * naming,
					ArrayDesc * portals,
					jint gcImpl,
					ArrayDesc *schedinfo);


#endif				/* ZERO_H */
