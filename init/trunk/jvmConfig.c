//===========================================================================
// File: jvmConfig.c
//
// Runtime Jem/JVM configuration.
//
//===========================================================================

#include <simpleconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include "jvmConfig.h"

void jvmConfig(void) {
    printf("Loading Jem/JVM configuration file.\n");

    if (cfg_loadfile("jvmconfig")) {
        printf("Could not load Jem/JVM configuration file.\n");
        printf("All configuration values will revert to programmed defaults.\n");
    }
}


// Look up value of name, and return it as an integer.
int getIntVal(char * name) {
    char *value;
    int  rValue;

    if (cfg_getval(name, &value)) {
        printf("Configuration file does not contain a value for %s\n", name);
        printf("Value will revert to programmed default.\n");
        return 0;
    }

    sscanf(value, "%d", &rValue);
    return rValue;
}

// Look up value of name and return it in a string.
char *getStringVal(char *name) {
    char        *value;

    if (cfg_getval(name, &value)) {
        printf("Configuration file does not contain a value for %s\n", name);
        printf("Value will revert to programmed default.\n");
        return NULL;
    }

    return value;
  // Bouml preserved body end 00022C82
}



//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright (C) 2007 Sombrio Systems Inc.
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
// As a special exception, if other files instantiate templates or use macros or 
// inline functions from this file, or you compile this file and link it with other 
// works to produce a work based on this file, this file does not by itself cause 
// the resulting work to be covered by the GNU General Public License. However the 
// source code for this file must still be made available in accordance with 
// section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on this
// file might be covered by the GNU General Public License.
//
// Alternative licenses for Jem may be arranged by contacting Sombrio Systems Inc. 
// at http://www.sombrio.com
//=================================================================================

