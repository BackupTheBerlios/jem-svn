// Additional Copyrights:
//	None
//==============================================================================
//
// Jem/JVM kernel module main.
//
//==============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/initrd.h>
#include <linux/moduleparam.h>
#include "jemtypes.h"
#include "malloc.h"
// @aspect include

#define VERSION "1.0.0"

void jem_exit (void)
{
	// @aspect
    printk(KERN_INFO "Jem/JVM is shutdown.\n");
}

int jem_init (void)
{
    int result;

    printk(KERN_INFO "Jem/JVM version %s\n", VERSION);
    
    // @aspect insert

    // Initialize memory subsystem
    if ((result = jemMallocInit()) < 0) return result;

    printk(KERN_INFO "Jem/JVM initialization complete.\n");

    return 0;
}

MODULE_AUTHOR("Christopher Stone");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("A Java Virtual Machine for embedded devices.");
MODULE_VERSION(VERSION);
module_init(jem_init);
module_exit(jem_exit);


