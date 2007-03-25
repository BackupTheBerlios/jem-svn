//===========================================================================
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

#define CONFIG_CODE_FRAGMENTS   30
#define CONFIG_CODE_BYTES (16 * 40 * 1024)
#define XMOFF 1

struct jvmConfig {
    unsigned int    maxDomains;
    unsigned int    heapBytesDom0;
    unsigned int    codeBytesDom0;
    unsigned int    maxNumberLibs;
    unsigned int    domScratchMemSz;
};


struct jvmConfig *getJVMConfig(void);



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

#endif
