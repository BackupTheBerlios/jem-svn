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
// File: malloc.h
//
// Jem/JVM memory allocation interface.
//
//===========================================================================

#ifndef _JEMMQ_H
#define _JEMMQ_H


int     jemInitMQ(void);
int     jemDestroyMQ(void);
void    *jemQAlloc(size_t size, char *fName, unsigned int fLine);
int     jemQSend(void *mbuf, size_t size, int mode, char *fName, unsigned int fLine);
ssize_t jemQReceiveTO(void **bufp, char *fName, unsigned int fLine);
ssize_t jemQReceiveBlk(void **bufp, char *fName, unsigned int fLine);
ssize_t jemQReceiveNB(void **bufp, char *fName, unsigned int fLine);
void    jemQFree(void *buf);
int     jemQHandshake(void);


#endif

