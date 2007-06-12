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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "libcli.h"
#include "jemtypes.h"

struct cli_def       *kcli;
struct cli_command   *jem_command;
struct cli_command	 *jemtst_command;

static int jem_test_mode(struct cli_def *cli, char *command, char *argv[], int argc)
{
    cli_set_configmode(cli, CLI_TEST_MODE, "utest");	
    return CLI_OK;
}

after(): call(void loadConfig()) 
{
    kcli            = cli_get();
    jem_command     = cli_register_command(kcli, NULL, "jem", NULL, PRIVILEGE_UNPRIVILEGED,
                                           MODE_EXEC, NULL);
    jemtst_command  = cli_register_command(kcli, NULL, "utest", NULL, PRIVILEGE_UNPRIVILEGED, 
                                           MODE_EXEC, NULL);
    cli_register_command(kcli, jemtst_command, "enter", jem_test_mode,
                         PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Enter jem test mode.");
}

before(): execution(void jem_exit()) 
{
    cli_unregister_command(kcli, "jem");
    cli_unregister_command(kcli, "utest");
}
