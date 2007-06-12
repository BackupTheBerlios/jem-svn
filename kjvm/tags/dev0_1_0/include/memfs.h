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
// memfs.h
//
//
//==============================================================================

#ifndef _MEMFS_H
#define _MEMFS_H

#define MAGIC_FD 0x00932756
#define MEMFS_RO  0
#define MEMFS_RW  1

typedef struct filedesc_s {
	u32                 magic;
	u32                 pos;
	u8                  *data;
	u32                 size;
	char                *filename;
	DomainDesc          *domain;
	MemoryProxyHandle   *mem;
} FileDesc;


void                memfs_init(void);
const char          *memfs_str2chr(ObjectDesc * string);
int                 memfs_lookup(const char *filename);
int                 memfs_link(const char *filename, u8 * mem);
int                 memfs_unlink(const char *filename);
MemoryProxyHandle   memfs_mmap(FileDesc * fd, DomainDesc * domain, int flag);
FileDesc            *memfs_open(DomainDesc * domain, char *filename);
int                 memfs_readByte(FileDesc * desc, jbyte * value);
char                *memfs_getString(FileDesc * desc);
void                memfs_close(FileDesc * desc);
jint                memfs_eof(FileDesc * desc);
int                 memfs_testChecksum(FileDesc * desc);
int                 memfs_seek(FileDesc * desc, unsigned long pos);
u32                 memfs_getSize(FileDesc * desc);
char                *memfs_getFileName(FileDesc * desc);
u32                 memfs_getPos(FileDesc * desc);
int                 memfs_readCode(FileDesc * desc, u8 * buf, jint nbuf);
int                 memfs_readString(FileDesc * desc, u8 * buf, jint nbuf);
int                 memfs_readStringData(FileDesc * desc, u8 * buf, jint length);
int                 memfs_readInt(FileDesc * desc, jint * value);




#endif // _MEMFS_H
