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
// libcache.h
// 
//  The libcache is used by the online compiler to insert
//  new jll-files into the system.
// 
//==============================================================================

#ifndef LIBCACHE_H
#define LIBCACHE_H


typedef struct libcache_entry_s {
    ObjectDesc *name;   /* Java String object */
    ObjectDesc *codefile;   /* Java Memory object */
    struct libcache_entry_s *next;
} libcache_entry;

void libcache_init(void);

char *libcache_lookup_jll(const char *name, jint * size);

void libcache_register_jll(ObjectDesc * self,
                           ObjectDesc * string_obj,
                           ObjectDesc * memory_obj);

#endif
