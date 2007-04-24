//=============================================================================
// This file is part of Kcli, a command line interface in a Linux kernel
// module for embedded Linux applications.
//
// Copyright © 2007 JavaDevices Software LLC. 
//
// Kcli is free software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License, version 2, as published by the 
// Free Software Foundation.
//
// Kcli is distributed in the hope that it will be useful, but WITHOUT ANY 
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
// details.
//
// You will find documentation for Kcli at http://www.javadevices.com
// 
// You will find the maintainers and current source code of Kcli at BerliOS:
//    http://developer.berlios.de/projects/jem/
// 
//=============================================================================
// main.c
// 
// Kcli module infrastructure.
// 
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/in.h>
#include <linux/syscalls.h>
#include <linux/net.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include "libcli.h"

#define VERSION "1.1.2"

unsigned int     cliPort = CONFIG_KCLI_PORT;
module_param(cliPort, uint, 444);
MODULE_PARM_DESC (cliPort, "Port that the cli will listen on.");


//-----------------------------------------------------------------------------
// 
// Prototypes.
// 
//-----------------------------------------------------------------------------

static void     kcli_start_cli(void);


//-----------------------------------------------------------------------------
// 
// Global data.
// 
//-----------------------------------------------------------------------------

static struct sockaddr_in       cliServerAddr;
static struct socket            *serverSocket;
static struct cli_def           *kcli;
static struct workqueue_struct  *cli_workqueue;
static int                      shutdown_flag = 0;
struct socket                   *clientSocket;


//-----------------------------------------------------------------------------
// 
// Driver registration and attributes.
// 
//-----------------------------------------------------------------------------

struct kcli_device_driver {
    struct device_driver    driver;
};

static struct kcli_device_driver kcli_driver = {
    .driver = {
        .name   = "kcli",
        .bus    = &platform_bus_type,
        .owner  = THIS_MODULE,
    },
};

static ssize_t kcli_banner_attribute_show(struct device_driver *driver, char *buf) 
{
    return sprintf(buf, "%s\n", kcli->banner);
}

static ssize_t kcli_banner_attribute_store(struct device_driver *driver, 
                                          const char *buf, size_t count) 
{
    cli_set_banner(kcli, (char *) buf);
    return count;
}

static DRIVER_ATTR(banner, 0644, kcli_banner_attribute_show, kcli_banner_attribute_store);

static ssize_t kcli_hostname_attribute_show(struct device_driver *driver, char *buf) 
{
    return sprintf(buf, "%s\n", kcli->hostname);
}

static ssize_t kcli_hostname_attribute_store(struct device_driver *driver, 
                                          const char *buf, size_t count) 
{
    cli_set_hostname(kcli, (char *) buf);
    return count;
}

static DRIVER_ATTR(hostname, 0644, kcli_hostname_attribute_show, kcli_hostname_attribute_store);

static struct driver_attribute *const kcli_drv_attrs[] = {
    &driver_attr_banner,
    &driver_attr_hostname,
};


//-----------------------------------------------------------------------------
// 
// Exported functions..
// 
//-----------------------------------------------------------------------------

struct cli_def *cli_get(void)
{
    return kcli;
}
EXPORT_SYMBOL(cli_get);

//-----------------------------------------------------------------------------
// 
// Module tasks.
// 
//-----------------------------------------------------------------------------

struct cli_loop_work {
    struct work_struct      work;
};

static struct cli_loop_work *cliwq;

static void kcli_cli_conn(struct work_struct *work)
{
    struct cli_loop_work    *workp = container_of(work, struct cli_loop_work, work);
    int                     on = 1;
    int                     rc = 0;

    if ((rc = sock_create_kern(PF_INET, SOCK_STREAM, 0, &serverSocket)) < 0) {
        printk(KERN_ERR "Socket creation error for CLI, result=%d.\n", rc);
        return;
    }

    if ((rc = kernel_setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on))) < 0) {
        printk(KERN_ERR "Socket option error for CLI, result=%d.\n", rc);
        return;
    }

    memset(&cliServerAddr, 0, sizeof(struct sockaddr));
    cliServerAddr.sin_family = PF_INET;
    cliServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    cliServerAddr.sin_port = htons(cliPort);
    if ((rc = kernel_bind(serverSocket, (struct sockaddr *) &cliServerAddr, sizeof(struct sockaddr))) < 0) {
        printk(KERN_ERR "Socket bind error for CLI, result=%d.\n", rc);
        return;
    }

    if ((rc = kernel_listen(serverSocket, 50)) < 0) {
        printk(KERN_ERR "Socket listen error for CLI, result=%d.\n", rc);
        return;
    }

    rc = kernel_accept(serverSocket, &clientSocket, 0);

    if (rc < 0) {
        printk(KERN_ERR "kcli_cli_conn: accept error, rc=%d\n", rc);
    }
    else {
        if (!shutdown_flag) {
            cli_loop(kcli, 0);
        }
    }
    sock_release(clientSocket);
    sock_release(serverSocket);
    kfree(workp);
    if (!shutdown_flag) kcli_start_cli();
}


//-----------------------------------------------------------------------------
// 
// Module initialization and cleanup.
// 
//-----------------------------------------------------------------------------

static void kcli_start_cli(void)
{
    cliwq = kmalloc(sizeof(struct cli_loop_work), GFP_KERNEL);
    INIT_WORK(&cliwq->work, kcli_cli_conn);
    queue_work(cli_workqueue, &cliwq->work);
}

static void kcli_exit (void)
{
    int i;
    struct socket *aSocket;

    shutdown_flag=1;
    cliServerAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sock_create_kern(PF_INET, SOCK_STREAM, 0, &aSocket);
    kernel_connect(aSocket, (struct sockaddr *) &cliServerAddr, sizeof(struct sockaddr), 0);
    sock_release(aSocket);
    cli_done(kcli);
    flush_workqueue(cli_workqueue);
    destroy_workqueue(cli_workqueue);

    for (i=0; i<ARRAY_SIZE(kcli_drv_attrs); i++) {
        driver_remove_file(&kcli_driver.driver, kcli_drv_attrs[i]);
    }

    driver_unregister(&kcli_driver.driver);
}

static int __init kcli_init (void)
{
    int rc, i;

    printk(KERN_INFO "Kcli version %s\n", VERSION);

    kcli = cli_init();
    if (kcli == NULL) {
        return -ENOMEM;
    }

    if ((rc = driver_register(&kcli_driver.driver))) {
        printk(KERN_WARNING "kcli: Could not register driver, rc=%d\n", rc);
        cli_done(kcli);
        return rc;
    }

    for (i=0; i<ARRAY_SIZE(kcli_drv_attrs); i++) {
        if ((rc = driver_create_file(&kcli_driver.driver, kcli_drv_attrs[i]))) {
            printk(KERN_WARNING "Failed to add sysfs attribute for kcli driver.\n");
            return rc;
        }
    }

    cli_set_banner(kcli, "Kcli Command Line Interface");
    cli_set_hostname(kcli, "Kcli");
    cli_allow_user(kcli, "java", "rules");
    cli_allow_enable(kcli, "javadevices");
    cli_workqueue = create_singlethread_workqueue("cli_wq");

    kcli_start_cli();

    return 0;
}

MODULE_AUTHOR("JavaDevices Software LLC");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("An embedded command line interface.");
MODULE_VERSION(VERSION);
module_init(kcli_init);
module_exit(kcli_exit);

