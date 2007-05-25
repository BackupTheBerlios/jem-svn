//=============================================================================
// This file is part of Kcfg, a simple configuration system in a Linux kernel
// module for embedded Linux applications.
//
// Copyright (C) 2007 Christopher Stone.
//
// Kcfg is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License, version 2, as published by the
// Free Software Foundation.
//
// Kcfg is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You will find documentation for Kcfg at http://www.javadevices.com
//
// You will find the maintainers and current source code of Kcfg at BerliOS:
//    http://developer.berlios.de/projects/jem/
//
//=============================================================================
// main.c
//
// Kcfg module infrastructure.
//
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rwsem.h>
#include "simpleconfig.h"
#ifdef CONFIG_KCFG_CLI
#include "libcli.h"
#endif

#define VERSION "1.1.3"

static char *filename = CONFIG_KCFG_FILENAME;
module_param(filename, charp, 444);
MODULE_PARM_DESC(filename, "Configuration file name.");


//-----------------------------------------------------------------------------
//
// Global data.
//
//-----------------------------------------------------------------------------

#ifdef CONFIG_KCFG_CLI
static struct cli_def       *kcli;
static struct cli_command   *cfg_command;
#endif
struct rw_semaphore         cfg_mutex;


//-----------------------------------------------------------------------------
//
// Driver registration and attributes.
//
//-----------------------------------------------------------------------------

struct kcfg_device_driver {
    struct device_driver    driver;
};

static struct kcfg_device_driver kcfg_driver = {
    .driver = {
        .name   = "kcfg",
        .bus    = &platform_bus_type,
        .owner  = THIS_MODULE,
    },
};


//-----------------------------------------------------------------------------
//
// Kcfg cli commands.
//
//-----------------------------------------------------------------------------

#ifdef CONFIG_KCFG_CLI
static int cmd_get(struct cli_def *cli, char *command, char *argv[], int argc)
{
    char *val;

    if(argv[0] == NULL)
    {
        cli_print(kcli, "No config name specified.");
    }else if(cfg_getval(argv[0], &val))
    {
        cli_print(kcli, "Failed to get value of \"%s\"", argv[0]);
    }else
    {
        cli_print(kcli, "%s = \"%s\"", argv[0], val);
    }

    return CLI_OK;
}

static int cmd_set(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    if(cmds[0] == NULL)
    {
        cli_print(kcli, "No config name specified.");
    }else if(cmds[1] == NULL)
    {
        cli_print(kcli, "No value specified for \"%s\"", cmds[0]);
    }else if(cfg_setval(cmds[0], cmds[1]))
    {
        cli_print(kcli, "Failed to set \"%s\" to \"%s\"", cmds[0], cmds[1]);
    }

    return CLI_OK;
}

static char *typename[9] = {"N/A","STR","INT","FIX","BOOL","DATE","TIME","HEX","OCT"};
static int iterfunc(unsigned long n, void *key, void *val)
{
    CONFIG_VALUE *cfg;
    char *name;

    name = key;
    cfg = val;

    if(name == NULL)
    {
        cli_print(kcli, "%d: no name", n);
    }else if(cfg == NULL)
    {
        cli_print(kcli, "%d: %s has no value", n, name);
    }else
    {
        cli_print(kcli, "%d: %s = \"%s\" (%s) %s%s", n, name, cfg->value,
            typename[cfg_valtype(name)],
            ((cfg->cfgline && cfg->cfgline->comment)?"COMMENT: ":""),
            ((cfg->cfgline && cfg->cfgline->comment)?cfg->cfgline->comment:""));
    }
    return 0;
}

static int cmd_list(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    int rv;

    if(cfg_iterate(iterfunc, &rv))
    {
        cli_print(kcli, "Failed to iterate over config table.");
    }

    return CLI_OK;
}

static int cmd_save(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    if(cmds[0] == NULL)
    {
        cli_print(kcli, "Save to current config file");
        if(cfg_savefile(filename))
        {
            cli_print(kcli, "Failed to save current config file");
        }
    }else
    {
        cli_print(kcli, "Save to file \"%s\"", cmds[0]);
        if(cfg_savefile(cmds[0]))
        {
            cli_print(kcli, "Failed to save to \"%s\"", cmds[0]);
        }
    }

    return CLI_OK;
}

static int cmd_load(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    if(cmds[0] == NULL)
    {
        cli_print(kcli, "No config file name specified.");
    }else if(cfg_loadfile(cmds[0]))
    {
        cli_print(kcli, "Failed to load config file \"%s\"", cmds[0]);
    }else
    {
        cli_print(kcli, "Loaded config file \"%s\"", cmds[0]);
    }

    return CLI_OK;
}

static int cmd_reset(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    cfg_reset();
    cli_print(kcli, "Config system reset.");

    return CLI_OK;
}

static int cmd_help(struct cli_def *cli, char *command, char *cmds[], int argc)
{
    cli_print(kcli, "Kcfg commands:");
    cli_print(kcli, "\tlist               - list config values");
    cli_print(kcli, "\tget <name>         - show a named config value");
    cli_print(kcli, "\tset <name> <value> - set a named config value");
    cli_print(kcli, "\tload <name>        - load a config file");
    cli_print(kcli, "\tsave [<name>]      - save the current config");
    cli_print(kcli, "\treset              - reset the config system");

    return CLI_OK;
}
#endif

//-----------------------------------------------------------------------------
//
// Module initialization and cleanup.
//
//-----------------------------------------------------------------------------

static void kcfg_exit (void)
{
#ifdef CONFIG_KCFG_CLI
    cli_unregister_command(kcli, "kcfg");
#endif
    cfg_reset();
    driver_unregister(&kcfg_driver.driver);
}

static int __init kcfg_init (void)
{
    int rc;

    printk(KERN_INFO "Kcfg version %s\n", VERSION);

    init_rwsem(&cfg_mutex);

    if (cfg_loadfile(filename)) {
        printk(KERN_INFO "kcfg: Config file %s loaded.\n", filename);
    }

    if ((rc = driver_register(&kcfg_driver.driver))) {
        printk(KERN_WARNING "kcfg: Could not register driver, rc=%d\n", rc);
        return rc;
    }

#ifdef CONFIG_KCFG_CLI
    kcli    = cli_get();
    cfg_command = cli_register_command(kcli, NULL, "kcfg", NULL, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Enter Kcfg mode");
    cli_register_command(kcli, cfg_command, "get", cmd_get, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Get configuration value");
    cli_register_command(kcli, cfg_command, "set", cmd_set, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Set configuration value");
    cli_register_command(kcli, cfg_command, "list", cmd_list, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "List configuration values.");
    cli_register_command(kcli, cfg_command, "save", cmd_save, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Write out new configuration file.");
    cli_register_command(kcli, cfg_command, "load", cmd_load, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Load a new configuration file.");
    cli_register_command(kcli, cfg_command, "reset", cmd_reset, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Reset (unload) a configuration.");
    cli_register_command(kcli, cfg_command, "help", cmd_help, PRIVILEGE_UNPRIVILEGED,
        MODE_EXEC, "Print out command help.");
#endif

    return 0;
}

MODULE_AUTHOR("Christopher Stone");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("An embedded module configuration interface.");
MODULE_VERSION(VERSION);
module_init(kcfg_init);
module_exit(kcfg_exit);

