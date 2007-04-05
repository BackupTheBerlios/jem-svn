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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <native/mutex.h>
#include <native/task.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "vmsupport.h"
#include "symfind.h"

const char *findCoreSymbol(jint addr, char *symname)
{
    char *modname;
    unsigned long symsize, offset;

    return kallsyms_lookup(addr, &symsize, &offset, &modname, symname);
}

unsigned long addrCoreSymbol(const char *name)
{
    return kallsyms_lookup_name(name);
}

#define NOINTIN(x) if (eip >= (void *)x && eip <= (void *)x + FKTSIZE_##x) return 1;

#ifndef FKTSIZE_thread_exit
#define FKTSIZE_thread_exit 0
#endif
#ifndef FKTSIZE_receive_portalcall
#define FKTSIZE_receive_portalcall 0
#endif
#ifndef FKTSIZE_callnative_special_portal
#define FKTSIZE_callnative_special_portal 0
#endif
#ifndef FKTSIZE_memory_set32
#define FKTSIZE_memory_set32 0
#endif

void jem_irq_exit(int cpuID);
extern unsigned char jem_irq_exit_end[];
extern unsigned char callnative_special_portal_end[];

int eip_in_last_stackframe(void *eip)
{
    NOINTIN(thread_exit);
    if (eip >= (void *) jem_irq_exit && eip <= (void *) jem_irq_exit_end)
        return 1;
    NOINTIN(receive_portalcall);

    return 0;
}


int in_portalcall(void *eip)
{
    NOINTIN(receive_portalcall);
    return 0;
}


