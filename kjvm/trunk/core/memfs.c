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
// memfs.c
//
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "profile.h"
#include "malloc.h"
#include "jar.h"
#include "load.h"
#include "libcache.h"
#include "zero_Memory.h"
#include "memfs.h"
#include "exception_handler.h"



void memfs_init(void)
{
    /* we do nothing yet */
}

#define MEM_MAX_STRING 128
static char char_buffer[MEM_MAX_STRING];


const char *memfs_str2chr(ObjectDesc * string)
{
    stringToChar(string, char_buffer, MEM_MAX_STRING);
    return (const char *) char_buffer;
}


int memfs_lookup(const char *filename)
{
    unsigned long size;
    jarentry entry;

    if (libcache_lookup_jll(filename, &size) == NULL) {
        jarReset();
        for (;;) {
            if (jarNextEntry(&entry) == -1)
                return JNI_FALSE;
            if (strcmp(entry.filename, filename) == 0)
                break;
        }
    }

    return JNI_TRUE;
}


int memfs_link(const char *filename, u8 * mem)
{
    return -1;
}

int memfs_unlink(const char *filename)
{
    return -1;
}


MemoryProxyHandle memfs_mmap(FileDesc * fd, DomainDesc * domain, int flag)
{
    if (flag != MEMFS_RO) {
        exceptionHandlerMsg(THROW_RuntimeException, "BootFS can only create ReadOnlyMemory");
    }

    if (fd->mem == NULL) {
        fd->mem = (MemoryProxyHandle   *) allocReadOnlyMemory((MemoryProxy *) domain, (jint) fd->data, fd->size);
    }
    return (MemoryProxyHandle) fd->mem;
}


FileDesc *memfs_open(DomainDesc * domain, char *filename)
{
    FileDesc *desc;
    u8 *codefile;
    u32 size;
    jarentry entry;

    if ((codefile = libcache_lookup_jll(filename, (jint *) &size)) == NULL) {
        jarReset();
        for (;;) {
            if (jarNextEntry(&entry) == -1)
                return NULL;
            if (strcmp(entry.filename, filename) == 0) {
                codefile = entry.data;
                size = entry.uncompressed_size;
                break;
            }
        }
    }

    desc = jemMalloc(sizeof(FileDesc) MEMTYPE_OTHER);
    memset(desc, 0, sizeof(FileDesc));

    desc->magic     = MAGIC_FD;
    desc->pos       = 0;
    desc->data      = codefile;
    desc->size      = size;
    desc->domain    = domain;
    desc->filename  = filename;

    return desc;
}


int memfs_readByte(FileDesc * desc, jbyte * value)
{
    if (memfs_eof(desc))
        return -1;

    *value = *(jbyte *) (desc->data + desc->pos);
    desc->pos++;
    return 0;
}


int memfs_readInt(FileDesc * desc, jint * value)
{
    if (memfs_eof(desc))
        return -1;

    *value = *(jint *) (desc->data + desc->pos);
    desc->pos += 4;
    return 0;
}


int memfs_readStringData(FileDesc * desc, u8 * buf, jint length)
{
    if (memfs_eof(desc))
        return -1;

    memcpy(buf, desc->data + desc->pos, length);
    buf[length] = 0;
    desc->pos += length;
    return 0;
}


int memfs_readString(FileDesc * desc, u8 * buf, jint nbuf)
{
    jint length;

    if (memfs_readInt(desc, &length))
        return -1;

    if (length >= nbuf) {
        printk(KERN_ERR "Buffer too small!\n");
        return -1;
    }

    return memfs_readStringData(desc, buf, length);
}


char *memfs_getString(FileDesc * desc)
{
    jint length;
    u8 *strbuf;

    if (memfs_readInt(desc, &length))
        return NULL;

    if (length > 10000) {
        printk(KERN_ERR "Buffer too small!\n");
        return NULL;
    }

    strbuf = (u8 *) jemMallocCode(desc->domain, length + 1);

    if (memfs_readStringData(desc, strbuf, length))
        return NULL;

    return strbuf;
}


int memfs_readCode(FileDesc * desc, u8 * buf, jint nbuf)
{
    if (memfs_eof(desc))
        return -1;

    memcpy(buf, desc->data + desc->pos, nbuf);
    desc->pos += nbuf;

    return 0;
}


u32 memfs_getPos(FileDesc * desc)
{
    return desc->pos;
}


char *memfs_getFileName(FileDesc * desc)
{
    return desc->filename;
}


u32 memfs_getSize(FileDesc * desc)
{
    return desc->size;
}


int memfs_seek(FileDesc * desc, unsigned long pos)
{
    desc->pos = pos;
    return 0;
}


int memfs_testChecksum(FileDesc * desc)
{
    int i, checksum;

    checksum = 0;
    for (i = 0; i < desc->size - 4; i++) {
        checksum = (checksum ^ (*(jbyte *) (desc->data + i))) & 0xff;
    }

    if (checksum != *(jint *) (desc->data + desc->size - 4))
        return -1;

    return 0;
}


jint memfs_eof(FileDesc * desc)
{
    if (desc->pos < desc->size)
        return JNI_FALSE;
    return JNI_TRUE;
}


void memfs_close(FileDesc * desc)
{
    desc->magic = 0;
    desc->size = 0;
    jemFree(desc, sizeof(FileDesc) MEMTYPE_OTHER);
}

