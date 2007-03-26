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
// File: profile.h
//
// Jem/JVM profiler interface.
//
//===========================================================================

#ifndef PROFILE_H
#define PROFILE_H

#include "code.h"

#ifndef CONFIG_JEM_PROFILE

#define PROFILE_STOP(_thr_)
#define PROFILE_STOP_PORTAL(_thr_)
#define PROFILE_STOP_BLOCK(_thr_)
#define PROFILE_CONT(_thr_)
#define PROFILE_CONT_PORTAL(_thr_)
#define PROFILE_CONT_BLOCK(_thr_)

#else				/* ifdef PROFILE */

#define PROF_EXTRAS 1
#define PROF_SAFE   1
#define PROF_DRIFT_STACK_SIZE 512
#define PROF_HASH_SIZE        1024
#define PROF_HASH_OFFSET      2
#define PROF_FAST_TRACE 0

#define PROFILE_STOP(_thr_) profile_stop(_thr_)
#define PROFILE_STOP_PORTAL(_thr_) profile_stop_portal(_thr_)
#define PROFILE_STOP_BLOCK(_thr_) profile_stop_block(_thr_)

#define PROFILE_CONT(_thr_) profile_cont(_thr_)
#define PROFILE_CONT_PORTAL(_thr_) profile_cont_portal(_thr_)
#define PROFILE_CONT_BLOCK(_thr_) profile_cont_block(_thr_)


typedef struct profile_entry_s {
	unsigned long long      time;
	unsigned long           count;
	void                    *callee;
	void                    *caller;
	void                    *cmdesc;
	struct profile_entry_s  *next;
	unsigned long long      rtime;
} ProfileEntry;

typedef struct profile_s {
	unsigned long long  *drift_ptr;	/* !! must be the first field in the struct !! */
	/* flag to show if timer is stopped */
	int                 stop;		/* !! must be the second field in the struct !! */
	unsigned long       ecount;
	unsigned long long  stimer;	/* stop timer thread switch */
	unsigned long long  dtime;	/* total time spend in other threads */
	unsigned long long  *drift_stack;
	unsigned long long  *drift_stack_end;
	ProfileEntry        *entries[PROF_HASH_SIZE];
	unsigned long       count;
	unsigned long       nportal;
	unsigned long       nswitch;
	unsigned long       errors;
	unsigned long       exception_err;
	unsigned long       drift_overruns;
	unsigned long long  total_dtime;
	long long           total_dtime2;
	unsigned long       exceptions;
	unsigned long long  exception_time;
} ProfileDesc;

extern long profile_drift_in;
extern long profile_drift_out;

void profile_init(void);
ProfileDesc *profile_new_desc(void);
ProfileEntry *profile_new_entry(void);
void profile_trace(void);
void profile_call(void);
long *profile_call2(struct MethodDesc_s * cmethod, long *sp, unsigned long long te);
void profile_shell(void *);
void profile_stop(struct ThreadDesc_s *thr);
void profile_stop_portal(struct ThreadDesc_s *thr);
void profile_cont(struct ThreadDesc_s *thr);
void profile_cont_portal(struct ThreadDesc_s *thr);
void profile_dump(struct ThreadDesc_s *thr);


#endif				/* PROFILE */

#endif				/* PROFILE_H */
