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


#endif /*__JAVASCHEDULER_H*/
