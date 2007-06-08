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
// Jem/JVM cli aspect.
//   This aspect adds the base code to be able to use the Kcli module.
//
//==============================================================================

#include "libcli.h"

struct cli_def       *kcli;
struct cli_command   *jem_command;

after(): call(void loadConfig()) 
{
    kcli        = cli_get();
    jem_command = cli_register_command(kcli, NULL, "jem", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, NULL);
}

before(): execution(void jem_exit()) 
{
    cli_unregister_command(kcli, "jem");
}
