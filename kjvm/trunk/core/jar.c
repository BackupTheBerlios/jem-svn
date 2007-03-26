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
//==============================================================================
// jar.c
// 
// Jem/JVM jar module processing
// 
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "jemtypes.h"
#include "jar.h"
#include "malloc.h"

/*
 * Read a jarfile with uncompressed entries
 */


static int LREC_SIZE = 30;
static int ECREC_SIZE = 22;


static int LOCAL_HEADER_SIGNATURE = 0x04034b50;
static int L_FILENAME_LENGTH = 26;
static int L_EXTRA_FIELD_LENGTH = 28;

static int CENTRAL_HEADER_SIGNATURE = 0x02014b50;
static int C_COMPRESSION_METHOD = 10;
static int C_UNCOMPRESSED_SIZE = 24;
static int C_FILENAME_LENGTH = 28;
static int C_EXTRA_FIELD_LENGTH = 30;
static int C_COMMENT_LENGTH = 32;
static int C_RELATIVE_OFFSET_LOCAL_HEADER = 42;
static int C_FILENAME = 46;

static int END_CENTRAL_DIR_SIGNATURE = 0x06054b50;
static int TOTAL_ENTRIES_CENTRAL_DIR = 10;
static int SIZE_CENTRAL_DIRECTORY = 12;



static char *jar;
static jint count;
static jint dirofs;
static char *mempos;
static char *dirbuf;
static jint jarCurrent;


static void zseek(jint pos)
{
	if (pos < 0) {
		printk(KERN_ERR "Seeking to negative position in jar file\n");
        pos = 0;
	}
	mempos = jar + pos;
}


static void zread(char *buf, jint len)
{
	memcpy(buf, mempos, len);
}


void jarReset(void)
{
	mempos = jar;
	dirofs = 0;
	jarCurrent = 0;
}


static jint makeword(char *b, int offset)
{
	return (((jint) (b[offset + 1]) << 8) | ((jint) (b[offset]) & 0xff));
}


static jint makelong(char *b, int offset)
{
	return ((((jint) b[offset + 3]) << 24) & 0xff000000)
	    | ((((jint) b[offset + 2]) << 16) & 0x00ff0000)
	    | ((((jint) b[offset + 1]) << 8) & 0x0000ff00)
	    | ((jint) b[offset + 0] & 0xff);
}


static void makestring(char *buf, char *b, int offset, int len)
{
	strncpy(buf, b + offset, len);
	buf[len] = '\0';
}


void jarInit(char *jarstart, jint jarlen)
{
	jint signature, dir_size;
	char buffer[ECREC_SIZE];
	int len = jarlen;
	jar = jarstart;
	mempos = jar;
	zseek(len - ECREC_SIZE);
	zread(buffer, ECREC_SIZE);
	signature = makelong(buffer, 0);
	if (signature != END_CENTRAL_DIR_SIGNATURE) {
		printk(KERN_ERR "Wrong jar signature=0x%lx\n", signature);
		return;
	}
	count = makeword(buffer, TOTAL_ENTRIES_CENTRAL_DIR);
	dir_size = makelong(buffer, SIZE_CENTRAL_DIRECTORY);
	zseek(len - (dir_size + ECREC_SIZE));
	dirbuf = jemMalloc(dir_size MEMTYPE_OTHER);
	zread(dirbuf, dir_size);
	dirofs = 0;
	jarCurrent = 0;

    printk(KERN_INFO "Jar code file detected at 0x%x.\n", (unsigned int) jarstart);
}


int jarNextEntry(jarentry * entry)
{
	jint signature, filename_length, cextra_length, comment_length, local_header_offset, filestart;
	char header[LREC_SIZE];
	if (jarCurrent == count)
		return -1;
	jarCurrent++;
	signature = makelong(dirbuf, dirofs + 0);
	if (signature != CENTRAL_HEADER_SIGNATURE) {
		printk(KERN_ERR "Wrong central header signature 0x%lx\n", signature);
		return -1;
	}
	entry->uncompressed_size = makelong(dirbuf, dirofs + C_UNCOMPRESSED_SIZE);

	filename_length = makeword(dirbuf, dirofs + C_FILENAME_LENGTH);
	cextra_length = makeword(dirbuf, dirofs + C_EXTRA_FIELD_LENGTH);
	comment_length = makeword(dirbuf, dirofs + C_COMMENT_LENGTH);
	local_header_offset = makelong(dirbuf, dirofs + C_RELATIVE_OFFSET_LOCAL_HEADER);

	/* read local header */
	zseek(local_header_offset);
	zread(header, LREC_SIZE);
	if (makelong(header, 0) != LOCAL_HEADER_SIGNATURE) {
		printk(KERN_ERR "Wrong local header signature\n");
		return -1;
	}

	filestart = local_header_offset + LREC_SIZE + makeword(header, L_FILENAME_LENGTH)
	    + makeword(header, L_EXTRA_FIELD_LENGTH);

	makestring(entry->filename, dirbuf, dirofs + C_FILENAME, filename_length);
	entry->data = jar + filestart;

	entry->compression_method = makeword(dirbuf, dirofs + C_COMPRESSION_METHOD);
	if (entry->compression_method != 0) {
		printk(KERN_INFO "Filename: %s\n", entry->filename);
		printk(KERN_ERR "Compression not supported.\n");
		return -1;
	}

	dirofs += C_FILENAME + filename_length + cextra_length + comment_length;
	return 0;
}






