//===========================================================================
// File: javascheduler.h
//
// Jem/JVM java scheduler interface.
//
//===========================================================================

#ifndef _JAVASCHEDULER_H
#define _JAVASCHEDULER_H

#include "thread.h"


typedef struct HLSchedDesc_s {
	struct ObjectDesc_s *SchedObj;
	struct ThreadDesc_s *SchedThread;
	struct ThreadDesc_s *SchedActivateThread;
	jboolean            maybeRunnable;
	java_method0_t      registered_code;
	java_method0_t      SchedPreempted_code;
	java_method0_t      SchedInterrupted_code;
	jboolean            resume;
	java_method0_t      activated_code;
	java_method1_t      preempted_code;
	java_method1_t      interrupted_code;
	java_method1_t      yielded_code;
	java_method1_t      unblocked_code;
	java_method1_t      created_code;
	java_method1_t      blocked_code;
	java_method1_t      blockedInPortalCall_code;
	java_method1_t      portalCalled_code;
	java_method1_t      switchedTo_code;
	java_method1_t      destroyed_code;
	java_method2_t      startGC_code;
	java_method1_t      GCdestroyed_code;
	java_method1_t      GCunblocked_code;
	java_method0_t      dump_code;
} HLSchedDesc_s;


typedef struct LLSchedDesc_s {
	struct ObjectDesc_s *SchedObj;
	struct ThreadDesc_s *SchedThread;
	java_method1_t      registered_code;
	java_method1_t      registerDomain_code;
	java_method1_t      unregisterDomain_code;
	java_method2_t      setTimeSlice_code;
	java_method0_t      activateCurrDomain_code;
	java_method0_t      activateNextDomain_code;
	java_method0_t      dump_code;
} LLSchedDesc;

typedef struct CpuDesc_s {
	LLSchedDesc LowLevel;
} CpuSchedDesc;

//Variables
extern CpuSchedDesc *CpuInfo[CONFIG_NR_CPUS];

//Prototypes
void smp_scheduler_init(void);

//void Sched_locked_created(struct ThreadDesc_s *thread);
void Sched_unblocked(struct ThreadDesc_s * thread);
void Sched_yielded(struct ThreadDesc_s * thread);
void Sched_blocked(struct ThreadDesc_s * thread);
void Sched_blockedInPortalCall(struct ThreadDesc_s * thread);
void Sched_switchedTo(struct ThreadDesc_s * thread);
void Sched_destroyed(struct ThreadDesc_s * thread);
jboolean Sched_portalCalled(struct ThreadDesc_s * thread);

void DomZero_dump(int cpu_id);
void DomZero_activated(int cpu_id);
void DomZero_preempted(int cpu_id, struct ObjectDesc_s * thread);
void DomZero_interrupted(int cpu_id, struct ObjectDesc_s * thread);

void LLSched_register(struct DomainDesc_s * domain, struct ObjectDesc_s * new_sched);
struct HLSchedDesc_s *createHLSchedDesc(struct DomainDesc_s * ObjDomain,
			       struct ObjectDesc_s * SchedObj);
void HLSched_register(struct DomainDesc_s * domain, struct HLSchedDesc_s * new_HLSched);

/* disable IRQs before calling!! */
void destroy_activate_nextDomain(void);
void destroy_activate_nextThread(void);

/* in schedSWITCH.S */
jint switch_to_nextThread(void);
jint switch_to_nextDomain(void);



//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
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
// As a special exception, if other files instantiate templates or use macros or 
// inline functions from this file, or you compile this file and link it with other 
// works to produce a work based on this file, this file does not by itself cause 
// the resulting work to be covered by the GNU General Public License. However the 
// source code for this file must still be made available in accordance with 
// section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this
// file might be covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================

#endif /*__JAVASCHEDULER_H*/
