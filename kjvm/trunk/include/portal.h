//===========================================================================
// File: portal.h
//
// Jem/JVM portal interface.
//
//===========================================================================

#ifndef _PORTAL_H
#define _PORTAL_H


typedef jint(*dep_f) (jint * paramlist);


typedef struct MethodInfoDesc_s {
	char        *name;
	char        *signature;
	code_t      code;
} MethodInfoDesc;

typedef struct DEPTypeDesc_s {
	char        *type;
	jint        numMethods;
	char        **methods;
	code_t      code;
} DEPTypeDesc;

typedef struct ServiceThreadPool_s {
	u32                 magic;
	u32                 flags;
	u32                 refcount;
	struct ThreadDesc_s *firstReceiver;
	struct ThreadDesc_s *firstWaitingSender;	
	struct ThreadDesc_s *lastWaitingSender;
	u32                 index;
} ServiceThreadPool;

typedef struct DEPDesc_s {
	u32                         magic;
	u32                         flags;
	struct ServiceThreadPool_s  *pool;
	volatile u32                lock;
	struct DomainDesc_s         *domain;
	struct ObjectDesc_s         *obj;
	struct Proxy_s              *proxy; 
	struct ClassDesc_s          *interface;
	volatile u32                valid;
	volatile u32                refcount;
	volatile u32                serviceIndex;
	volatile u32                abortFlag;
	u32                         statistics_no_receiver;
	u32                         statistics_handoff;
} DEPDesc;


#define MAGIC_DEP 0xbabeface

typedef struct Proxy_s {
	code_t              *vtable;
	struct DomainDesc_s *targetDomain;
	u32                 targetDomainID;
	u32                 index;
} Proxy;

typedef struct CPUStateProxy_s {
	code_t              *vtable;
	struct ThreadDesc_s *cpuState;
} CPUStateProxy;

typedef struct AtomicVariableProxy_s {
	code_t              *vtable;
	struct ObjectDesc_s *value;
	struct ThreadDesc_s *blockedThread;
	int                 listMode;
} AtomicVariableProxy;

typedef struct CASProxy_s {
	code_t  *vtable;
	u32     index;
} CASProxy;

typedef struct VMObjectProxy_s {
	code_t              *vtable;
	struct DomainDesc_s *domain;
	u32                 domain_id;
	u32                 epoch;
	int                 type;
	struct ObjectDesc_s *obj;
	int                 subObjectIndex;
} VMObjectProxy;

typedef struct CredentialProxy_s {
	code_t              *vtable;
	u32                 signerDomainID;
	struct ObjectDesc_s *value;
} CredentialProxy;

typedef struct DomainProxy_s {
	code_t              *vtable;
	struct DomainDesc_s *domain;
	u32                 domainID;
} DomainProxy;

typedef struct InterceptOutboundInfo_s {
	struct DomainDesc_s      *source;
	struct DomainDesc_s      *target;
	struct MethodDesc_S      *method;
	struct ObjectDesc_s      *obj;
	struct ArrayDesc_s       *paramlist;
} InterceptOutboundInfo;

typedef struct InterceptInboundInfoProxy_s {
	code_t               *vtable;
	struct DomainDesc_s  *source;
	struct DomainDesc_s  *target;
	struct ObjectDesc_s  *method;
	struct ObjectDesc_s  *obj;
	jint                 *paramlist;
	int                  index;
} InterceptInboundInfoProxy;

typedef struct InterceptPortalInfoProxy_s {
	code_t              *vtable;
	struct DomainDesc_s *domain;
	u32                 index;
} InterceptPortalInfoProxy;


void service_decRefcount(DomainDesc * domain, u32 index);



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
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.javadevices.com
//=================================================================================

#endif
