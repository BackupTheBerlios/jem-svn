//=============================================================================
// This file is part of Ktst, a unit testing system in a Linux kernel
// module for embedded Linux applications.
//
// Copyright (C) 2007 Christopher Stone.
// 
// Permission is hereby granted, free of charge, to any person obtaining 
// a copy of this software and associated documentation files (the 
// "Software"), to deal in the Software without restriction, including 
// without limitation the rights to use, copy, modify, merge, publish, 
// distribute, and/or sell copies of the Software, and to permit persons 
// to whom the Software is furnished to do so, provided that the above 
// copyright notice(s) and this permission notice appear in all copies 
// of the Software and that both the above copyright notice(s) and this 
// permission notice appear in supporting documentation.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT 
// OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
// HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY 
// SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER 
// RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
// CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
// 
// Except as contained in this notice, the name of a copyright holder 
// shall not be used in advertising or otherwise to promote the sale, 
// use or other dealings in this Software without prior written 
// authorization of the copyright holder.
//
// You will find documentation for Ktst at http://www.javadevices.com
//
// You will find the maintainers and current source code of Ktst at BerliOS:
//    http://developer.berlios.de/projects/jem/
//
//=============================================================================
// main.c
//
// Ktst module infrastructure.
//
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>
#include "libcli.h"

#define VERSION "1.0.0"

struct cli_def       *kcli;


void ktst_exit (void)
{
    printk(KERN_INFO "Ktst is unloaded.\n");
}

int ktst_init (void)
{
    printk(KERN_INFO "Ktst version %s\n", VERSION);
    
    kcli    = cli_get();
   
    return 0;
}

MODULE_AUTHOR("Christopher Stone");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("A kernel module unit testing interface for embedded devices.");
MODULE_VERSION(VERSION);
module_init(ktst_init);
module_exit(ktst_exit);
