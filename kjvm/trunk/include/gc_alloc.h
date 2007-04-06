//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
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

#ifndef GC_ALLOC_H
#define GC_ALLOC_H

CPUStateProxy           *allocCPUStateProxyInDomain(DomainDesc * domain,
                                                    ClassDesc * c, ThreadDesc * cpuState);
Proxy                   *allocProxyInDomain(DomainDesc * domain, ClassDesc * c,
                                            struct DomainDesc_s *targetDomain, u32 targetDomainID, u32 index);
CredentialProxy         *allocCredentialProxyInDomain(DomainDesc * domain,
                                                      ClassDesc * c, u32 signerDomainID);
DEPDesc                 *allocServiceDescInDomain(DomainDesc * domain);
ArrayDesc               *allocArrayInDomain(DomainDesc * domain, ClassDesc * elemClass, jint size);
DomainProxy             *allocDomainProxyInDomain(DomainDesc * domain, DomainDesc * domainValue, u32 domainID);
ThreadDescProxy         *allocThreadDescProxyInDomain(DomainDesc * domain, ClassDesc * c);
ThreadDescForeignProxy  *allocThreadDescForeignProxyInDomain(DomainDesc * domain, ThreadDescProxy * src);
DomainDesc              *specialAllocDomainDesc(void);
ArrayDesc               *vmSpecialAllocArray(ClassDesc * elemClass0, jint size);
u32                     *specialAllocStaticFields(DomainDesc * domain, int numberFields);
InterceptInboundInfoProxy *allocInterceptInboundInfoProxyInDomain(DomainDesc * domain);
ObjectDesc              *allocObjectInDomain(DomainDesc * domain, ClassDesc * c);
ServiceThreadPool       *allocServicePoolInDomain(DomainDesc * domain);
VMObjectProxy           *allocVMObjectProxyInDomain(DomainDesc * domain);
CASProxy                *allocCASProxyInDomain(DomainDesc * domain, ClassDesc * c, u32 index);
AtomicVariableProxy     *allocAtomicVariableProxyInDomain(DomainDesc * domain, ClassDesc * c);


#endif              /* GC_ALLOC_H */
