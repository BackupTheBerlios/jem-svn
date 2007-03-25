
#ifndef GC_ALLOC_H
#define GC_ALLOC_H

//FIXME
//ObjectDesc*  allocObjectInDomain(DomainDesc *domain, ClassDesc *c);

CPUStateProxy *allocCPUStateProxyInDomain(DomainDesc * domain,
					  ClassDesc * c,
					  ThreadDesc * cpuState);
Proxy *allocProxyInDomain(DomainDesc * domain, ClassDesc * c,
			  struct DomainDesc_s *targetDomain,
			  u4_t targetDomainID, u4_t index);
CredentialProxy *allocCredentialProxyInDomain(DomainDesc * domain,
					      ClassDesc * c,
					      u4_t signerDomainID);
DEPDesc *allocServiceDescInDomain(DomainDesc * domain);

ArrayDesc *allocArrayInDomain(DomainDesc * domain, ClassDesc * elemClass,
			      jint size);

DomainProxy *allocDomainProxyInDomain(DomainDesc * domain, DomainDesc * domainValue, u4_t domainID);
ThreadDescProxy *allocThreadDescProxyInDomain(DomainDesc * domain, ClassDesc * c);
ThreadDescForeignProxy *allocThreadDescForeignProxyInDomain(DomainDesc * domain, ThreadDescProxy * src);
#ifdef ENABLE_MAPPING
MappedMemoryProxy *allocMappedMemoryProxyInDomain(DomainDesc * domain, char *mem, ClassDesc *cl);
#endif /* ENABLE_MAPPING */

CPUDesc *specialAllocCPUDesc();
DomainDesc *specialAllocDomainDesc();
ArrayDesc *vmSpecialAllocArray(ClassDesc * elemClass0, jint size);


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

#endif				/* GC_ALLOC_H */
