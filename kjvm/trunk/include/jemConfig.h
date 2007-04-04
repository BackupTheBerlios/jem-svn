//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
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
// File: jemConfig.h
//
// Jem/JVM configuration.
//
//===========================================================================

#ifndef _JEMCONFIG_H
#define _JEMCONFIG_H

#ifdef __KERNEL__
#include <linux/types.h>

#ifndef CONFIG_SMP
#define CONFIG_NR_CPUS 1
#endif

#endif  // __KERNEL__

#define PORTALPARAMS_SIZE 1024

#define CONFIG_CODE_BYTES (512 * 1024)
#define XMOFF 1

struct jvmConfig {
    unsigned int    maxDomains;
    unsigned int    heapBytesDom0;
    unsigned int    codeBytesDom0;
    unsigned int    maxNumberLibs;
    unsigned int    domScratchMemSz;
    unsigned int    maxRegistered;
    unsigned int    maxHeapSamples;
    unsigned int    heapReserve;
    unsigned int    defaultServicePrio;
    unsigned int    serviceStackSz;
    unsigned int    serviceParamsSz;
    unsigned int    receivePortalQuota;
};


struct jvmConfig *getJVMConfig(void);


#endif

