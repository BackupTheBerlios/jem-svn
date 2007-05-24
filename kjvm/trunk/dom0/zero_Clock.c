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
// zero_Clock.c
//
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
#include <native/timer.h>
#include "jemtypes.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "code.h"
#include "portal.h"
#include "thread.h"
#include "vmsupport.h"
#include "load.h"
#include "exception_handler.h"
#include "zero.h"
#include "malloc.h"

/*
 * ClockDEP
 */
static jint clock_getTimeInMillis(ObjectDesc * self)
{
    return (jint) rt_timer_tsc2ns(rt_timer_tsc()) / 1000000;
}

jlong clock_getTicks(ObjectDesc * self)
{
    return (jlong) rt_timer_tsc();
}

jint clock_getTicks_low(ObjectDesc * self)
{
    unsigned long long ret;
    ret = (unsigned long long) rt_timer_tsc();
    return (jint) (ret & 0x000000007fffffff);
}

jint clock_getTicks_high(ObjectDesc * self)
{
    unsigned long long ret;
    ret = (unsigned long long) rt_timer_tsc();
    return (jint) (ret >> 32);
}

jint clock_getCycles(ObjectDesc * self, ObjectDesc * cycleTime)
{
    unsigned long long ret;
    ret = (unsigned long long) rt_timer_tsc();
    cycleTime->data[0] = ret & 0x000000007fffffff;
    cycleTime->data[1] = ret >> 32;
    return 0;
}

jint clock_subtract(ObjectDesc * self, ObjectDesc * res, ObjectDesc * a, ObjectDesc * b)
{
    u64 ca = *((u64 *) a->data);
    u64 cb = *((u64 *) b->data);
    u64 cr = ca - cb;
    *((u64 *) res->data) = cr;
    return 0;
}

jint clock_toMicroSec(ObjectDesc * self, ObjectDesc * a)
{
    SRTIME ca = *((u64 *) a->data);
    return rt_timer_tsc2ns(ca) / 1000;
}

jint clock_toNanoSec(ObjectDesc * self, ObjectDesc * a)
{
    SRTIME ca = *((u64 *) a->data);
    return rt_timer_tsc2ns(ca);
}

jint clock_toMilliSec(ObjectDesc * self, ObjectDesc * a)
{
    SRTIME ca = *((u64 *) a->data);
    return rt_timer_tsc2ns(ca) / 1000000;
}

MethodInfoDesc clockMethods[] = {
    {"getTimeInMillis", "", (code_t) clock_getTimeInMillis}
    ,
    {"getTicks", "", (code_t) clock_getTicks}
    ,
    {"getTicks_low", "", (code_t) clock_getTicks_low}
    ,
    {"getTicks_high", "", (code_t) clock_getTicks_high}
    ,
    {"getCycles", "", (code_t) clock_getCycles}
    ,
    {"subtract", "", (code_t) clock_subtract}
    ,
    {"toMicroSec", "", (code_t) clock_toMicroSec}
    ,
    {"toNanoSec", "", (code_t) clock_toNanoSec}
    ,
    {"toMilliSec", "", (code_t) clock_toMilliSec}
    ,
};

void init_clock_portal(void)
{
    init_zero_dep_without_thread("jx/zero/Clock", "Clock", clockMethods, sizeof(clockMethods), "<jx/zero/Clock>");
}

