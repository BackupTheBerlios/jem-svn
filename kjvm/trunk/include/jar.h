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
// File: jar.h
//
// Jem/JVM jar file interface.
//
//===========================================================================

#ifndef _JAR_H
#define _JAR_H



typedef struct jarentry_s {
    char    filename[80];
    jint    uncompressed_size;
    jint    compression_method;
    char    *data;
    jint    isDirectory;
} jarentry;


void    jarReset(void);
int     jarNextEntry(jarentry * entry);
void    jarInit(char *jarstart, jint jarlen);


#endif
