//===========================================================================
// File: jemUPipe.h
//
// Jem/JVM user space pipe interface.
//
//===========================================================================

#ifndef _JEMUPIPE_H
#define _JEMUPIPE_H

#include <native/pipe.h>

int         jemInitUPipe(void);
int         jemDestroyUPipe(void);
RT_PIPE_MSG *jemUPAlloc(size_t size, char *fName, unsigned int fLine);
int         jemUPSend(RT_PIPE_MSG *mbuf, size_t size, int mode, char *fName, unsigned int fLine);
ssize_t     jemUPReceiveTO(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine);
ssize_t     jemUPReceiveBlk(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine);
ssize_t     jemUPReceiveNB(RT_PIPE_MSG **bufp, char *fName, unsigned int fLine);
void        jemUPFree(RT_PIPE_MSG *buf);
int         jemUPHandshake(void);


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

