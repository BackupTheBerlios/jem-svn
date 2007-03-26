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
// File: messages.h
//
// Jem/JVM message queue interface.
//
//===========================================================================

#ifndef _MESSAGES_H
#define _MESSAGES_H

#include "jemConfig.h"

#define TESTMSG         0
#define TESTMSGREPLY    1
#define GETCONFIG       2

struct jvmConfigMsg {
    unsigned int        cmd;
    struct jvmConfig    data;
};

struct jvmMessage {
    unsigned int    cmd;
};


#endif

