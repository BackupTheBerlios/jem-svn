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
// Jem/JVM trace aspect.
//   This aspect adds code to printk function tracing messages. A command
//   is added to the cli to enable and disable the tracing at run time.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "libcli.h"

int tracingEnabled=0;

extern struct cli_def       *kcli;
extern struct cli_command   *jem_command;


static int trace_enable_cmd(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc == 0) {
        if (tracingEnabled) tracingEnabled=0;
        else tracingEnabled = 1;
    }
    else {
        if (strncmp(argv[0], "enable", 6)) {
            tracingEnabled = 1;
        }
        else {
            tracingEnabled = 0;
        }
    }

    if (tracingEnabled) {
        cli_print(cli, "Tracing is enabled.\r\n");
    }
    else {
        cli_print(cli, "Tracing is disabled.\r\n");
    }

    return CLI_OK;
}


before(): cflow(execution(int jemMalloc_test_cmd(struct cli_def *, char *, char *, int)))
{
    if (tracingEnabled) {
        printk(KERN_INFO "Trace: %s function %s in %s\n", this->kind, this->funcName, this->fileName);
    }
}

after(): execution(int jem_init()) 
{
    cli_register_command(kcli, jem_command, "trace", trace_enable_cmd,
                         PRIVILEGE_UNPRIVILEGED, MODE_EXEC, "Enable/Disable function tracing.");
}

