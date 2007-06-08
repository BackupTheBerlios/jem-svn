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
#include "jemConfig.h"
#include "simpleconfig.h"
// @aspect include

#define VERSION "0.1.0"

static int 	jConfig[jemConfSize];
static char *confNames[] = {"jem_codeFragments",
							"jem_maxServices",
							"jem_maxDomains",
							"jem_maxNumberLibs",
							"jem_domScratchMemSz",
							"jem_heapBytesDom0",
							"jem_codeBytesDom0",
							"jem_codeBytes",
						   };

int getJVMConfig(unsigned int id) 
{
	return jConfig[id];
} 

static void loadConfig(void)
{
	char *val;
	int  sv=0, i;
	
	for (i=0; i<jemConfSize; i++)
	{
	    if (cfg_getval(confNames[i], &val))
	    {
	    	switch (i)
	    	{ 
	    		case codeFragments:
	    			val="30";
	    			break;
	    		case maxServices:
	    			val = "1500";
	    			break;
	    		case maxDomains:
	    			val = "5";
	    			break;
	    		case maxNumberLibs:
	    			val = "40";
	    			break;
	    		case domScratchMemSz:
	    			val = "8192";
	    			break;
	    		case heapBytesDom0:
	    			val = "8192";
	    			break;
	    		case codeBytesDom0:
	    			val = "8192";
	    			break;
	    		case codeBytes:
	    			val = "8192";
	    			break;
	    		default:
	    			val = "0";
	    	}
	    	if(cfg_setval(confNames[i], val))
            {
                printk(KERN_ERR "Failed to set %s to %s in loadConfig().\n", confNames[i], val);
            }
            else
            {
                printk(KERN_INFO "Set %s to %s in loadConfig().\n", confNames[i], val);
            }
	    	sv = 1;
	    }
	    sscanf(val, "%d", &jConfig[i]);
	}
	
	if (sv) cfg_savefile(NULL);
	
	return;
}

void jem_exit (void)
{
	// @aspect begin
    printk(KERN_INFO "Jem/JVM is shutdown.\n");
}

int jem_init (void)
{
    int result;

    printk(KERN_INFO "Jem/JVM version %s\n", VERSION);
    
    loadConfig();
    
    // @aspect cli
    // @aspect test

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


