//==============================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright (C) 2007 JavaDevices Software. 
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
//
// Jem/JVM malloc statistics aspect.
//   This aspect adds code that will keep track of how much memory has
//   been allocated, and it classifies each allocation into one of
//   nine different categories. There is also a cli command added to 
//   display the statistics.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include "libcli.h"
#include "jemtypes.h"
#include "malloc.h"

extern struct cli_def       *kcli;
extern struct cli_command   *jem_command;

struct alloc_stat_s {
    u32 heap;
    u32 code;
    u32 stack;
    u32 memobj;
    u32 profiling;
    u32 emulation;
    u32 tmp;
    u32 dcb;
    u32 other;
};

static struct alloc_stat_s alloc_stat;

static unsigned long  totalAllocedRam;
static unsigned long  peakAllocedRam;
static unsigned long  totalAllocations;
static unsigned long  maxAllocSize;

#define MEMTYPE_OTHER  0
#define MEMTYPE_HEAP   1
#define MEMTYPE_STACK 2
#define MEMTYPE_MEMOBJ 3
#define MEMTYPE_CODE   4
#define MEMTYPE_PROFILING   5
#define MEMTYPE_EMULATION   6
#define MEMTYPE_TMP   7
#define MEMTYPE_DCB   8


static void jemMallocIncrement(u32 size, int memtype)
{
    totalAllocedRam += size;
    totalAllocations++;
    if (totalAllocedRam > peakAllocedRam) peakAllocedRam = totalAllocedRam;
    if (size > maxAllocSize) maxAllocSize = size;

    switch (memtype) {
    case MEMTYPE_HEAP:
        alloc_stat.heap += size;
        break;
    case MEMTYPE_CODE:
        alloc_stat.code += size;
        break;
    case MEMTYPE_MEMOBJ:
        alloc_stat.memobj += size;
        break;
    case MEMTYPE_PROFILING:
        alloc_stat.profiling += size;
        break;
    case MEMTYPE_STACK:
        alloc_stat.stack += size;
        break;
    case MEMTYPE_EMULATION:
        alloc_stat.emulation += size;
        break;
    case MEMTYPE_TMP:
        alloc_stat.tmp += size;
        break;
    case MEMTYPE_DCB:
        alloc_stat.dcb += size;
        break;
    case MEMTYPE_OTHER:
        alloc_stat.other += size;
        break;
    default:
        printk(KERN_WARNING "Allocation MEMTYPE unknown.\n");
    }
}


static void jemMallocDecrement(u32 size, int memtype)
{
    switch (memtype) {
    case MEMTYPE_HEAP:
        alloc_stat.heap -= size;
        break;
    case MEMTYPE_CODE:
        alloc_stat.code -= size;
        break;
    case MEMTYPE_MEMOBJ:
        alloc_stat.memobj -= size;
        break;
    case MEMTYPE_PROFILING:
        alloc_stat.profiling -= size;
        break;
    case MEMTYPE_STACK:
        alloc_stat.stack -= size;
        break;
    case MEMTYPE_EMULATION:
        alloc_stat.emulation -= size;
        break;
    case MEMTYPE_TMP:
        alloc_stat.tmp -= size;
        break;
    case MEMTYPE_DCB:
        alloc_stat.dcb -= size;
        break;
    case MEMTYPE_OTHER:
        alloc_stat.other -= size;
        break;
    default:
        printk(KERN_WARNING "Free MEMTYPE unknown\n");
    }

    totalAllocedRam -= size;
    totalAllocations--;
}

#define K(x) ((x) << (PAGE_SHIFT - 10))

static int jemMalloc_stat_cmd(struct cli_def *cli, char *command, char *argv[], int argc)
{
    struct sysinfo mi;

    si_meminfo( &mi );

  	cli_print(kcli, "Allocated memory: ......... %ld kB\r\n", totalAllocedRam/1024);
  	cli_print(kcli, "Peak allocated memory: .... %ld kB\r\n", peakAllocedRam/1024);
    if (totalAllocations > 0)
        cli_print(kcli, "Average allocation size: .. %ld Bytes\r\n", totalAllocedRam / totalAllocations);
  	cli_print(kcli, "Available memory: ......... %ld kB\r\n", K(mi.freeram));
  	cli_print(kcli, "Used for:\r\n");
  	cli_print(kcli, "   Heap      %9ld\r\n", alloc_stat.heap);
  	cli_print(kcli, "   Code      %9ld\r\n", alloc_stat.code);
  	cli_print(kcli, "   Stack     %9ld\r\n", alloc_stat.stack);
  	cli_print(kcli, "   Memobj    %9ld\r\n", alloc_stat.memobj);
  	cli_print(kcli, "   Profiling %9ld\r\n", alloc_stat.profiling);
  	cli_print(kcli, "   Emulation %9ld\r\n", alloc_stat.emulation);
  	cli_print(kcli, "   Tmp       %9ld\r\n", alloc_stat.tmp);
  	cli_print(kcli, "   DCB       %9ld\r\n", alloc_stat.dcb);
  	cli_print(kcli, "   Other     %9ld\r\n", alloc_stat.other);

    return CLI_OK;
}

void *jemStatMalloc(u32 size, int memtype)
{
    jemMallocIncrement(size, memtype);
    return jemMalloc(size);
}

void jemStatFree(void * addr, u32 size, int memtype)
{
    jemMallocDecrement(size,memtype);
    jemFree(addr);
    return;
}

void jemStatFreeCode(void *addr, u32 size)
{
    jemMallocDecrement(size, MEMTYPE_CODE);
    jemFreeCode(addr);
    return;
}


void * around(u32 size): call(void *jemMalloc(u32)) && infunc(jemMallocThreadDescProxy) && args(size)
{
    // redirect call to jemMalloc in the function jemMallocThreadDescProxy
    return jemStatMalloc(size, MEMTYPE_DCB);
}

void * around(u32 size): call(void *jemMalloc(u32)) && infunc(jemMallocTmp) && args(size)
{
    // redirect the call to jemMalloc in the function jemMallocTmp
    return jemStatMalloc(size, MEMTYPE_TMP);
}

void around(void *addr): call(void jemFree(void *)) && infunc(jemFreeThreadDesc) && args(addr)
{
    // redirect the call to jemFree in the function jemFreeThreadDesc
    jemStatFree(addr, sizeof(ThreadDescProxy), MEMTYPE_DCB);
    return;
}

void around(void *addr): call(void jemFree(void *)) && infunc(jemFreeTmp)  && args(addr)
{
    // redirect the call to jemFree in the function jemFreeTmp
    TempMemory *m = addr;
    jemStatFree(addr, m->size, MEMTYPE_TMP);
    return;
}

before(u32 chunksize): call(char *jem_vmalloc(u32)) && infunc(jemMallocCode) && args(chunksize)
{
    // add code before calls to vmalloc in the function jemMallocCode
    jemMallocIncrement(chunksize, MEMTYPE_CODE);
}

void around(void *addr, DomainDesc *domain): call(void jemFreeCode(void *)) && infunc(terminateDomain)  && args(addr, domain)
{
    // redirect call to jemFreeCode in the function terminateDomain
    jemStatFreeCode(addr, domain->code_bytes);
    return;
}

int around(): execution(int jemMallocInit()) 
{
    // add new code to the function jemMallocInit
    totalAllocedRam      = 0;
    peakAllocedRam       = 0;
    totalAllocations     = 0;
    maxAllocSize         = 0;
    alloc_stat.heap      = 0;
    alloc_stat.code      = 0;
    alloc_stat.memobj    = 0;
    alloc_stat.profiling = 0;
    alloc_stat.stack     = 0;
    alloc_stat.emulation = 0;
    alloc_stat.tmp       = 0;
    alloc_stat.dcb       = 0;
    alloc_stat.other     = 0;
    
    cli_register_command(kcli, jem_command, "mem", jemMalloc_stat_cmd, 
          PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Display memory usage statistics");

    return proceed();
}

