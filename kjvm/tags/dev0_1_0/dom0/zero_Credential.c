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
// zero_Credential.c
//
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <native/mutex.h>
#include <native/task.h>
#include <native/event.h>
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
#include "gc_alloc.h"


/*
 * Credential Variable
 */
ClassDesc *credentialvariableClass;

static void credentialvariable_set(CredentialProxy * self, ObjectDesc * value)
{
    if (curdom()->id != self->signerDomainID) {
        ObjectDesc *self = createExceptionInDomain(curdom(),
                               "java/lang/RuntimeException",
                               "ATTEMPT TO SET CREDENTIAL DATE IN WRONG DOMAIN");
        exceptionHandler((jint *) self);
    }
    self->value = value;
}

static ObjectDesc *credentialvariable_get(CredentialProxy * self)
{
    return self->value;
}

static jint credentialvariable_getSignerDomainID(CredentialProxy * self)
{
    return self->signerDomainID;
}

MethodInfoDesc credentialvariableMethods[] = {
    {"set", "", (code_t) credentialvariable_set}
    ,
    {"get", "", (code_t) credentialvariable_get}
    ,
    {"getSignerDomainID", "", (code_t) credentialvariable_getSignerDomainID}
    ,
};

static jbyte credentialvariableTypeMap[] = { 1 };

void init_credential_portal(void)
{
    credentialvariableClass =
        init_zero_class("jx/zero/Credential", credentialvariableMethods, sizeof(credentialvariableMethods), 2,
                credentialvariableTypeMap, "<jx/zero/Credential>");
}
