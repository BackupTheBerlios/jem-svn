//=============================================================================
// This file is part of Kcfg, a simple configuration system in a Linux kernel
// module for embedded Linux applications.
//
// Copyright (C) 2007 Christopher Stone.
// Copyright (C) 2006 Jeffrey S. Dutky
//
// This file was derived directly from the simple configuration system,
// which is a user space library developed by Jeffrey S. Dutky.
//
// This file is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License, version 2.1, as published
// by the Free Software Foundation.
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
// simpleconfig.h
//
// Simple configuration interface
//
//=============================================================================

#ifndef CONFIG_H
#define CONFIG_H


typedef struct CONFIG_CB_STRUCT CONFIG_CB;
typedef struct CONFIG_VALUE_STRUCT CONFIG_VALUE;
typedef struct CONFIG_LINE_STRUCT CONFIG_LINE;
typedef struct CONFIG_NEW_STRUCT CONFIG_NEW;

#define CONFIG_CB_TAG 0x43664342
struct CONFIG_CB_STRUCT {
    long tag;
    int (*func)(char *name, char *value); /* watcher callback function */
    CONFIG_CB *link; /* cb list link */
};

#define CONFIG_VALUE_TAG 0x43666756
struct CONFIG_VALUE_STRUCT {
    long tag;
    char *name;
    char *value;
    CONFIG_CB *cb_list; /* list of watcher callback functions */
    CONFIG_LINE *cfgline; /* origin line, if any, NULL if new */
};

#define CONFIG_LINE_TAG 0x43664c6e
struct CONFIG_LINE_STRUCT {
    long tag;
    char *line; /* line read from configuration file */
    char *comment; /* points to start of comment in line, if any */
    CONFIG_VALUE *modified; /* points to cfg value if modified */
    CONFIG_LINE *link; /* line list link */
};

#define CONFIG_NEW_TAG 0x43664e77
struct CONFIG_NEW_STRUCT {
    long tag;
    CONFIG_VALUE *cfg;
    CONFIG_NEW *link;
};


/* read configuration file consisting of name-value pairs */

/*
** the configuration file is formatted as simple name-value pairs,
** each name is a simple alphanumeric string consisting of letters,
** numbers, underscores and periods.
** each value is a simple string, possibly quoted, ending at EOL.
** comments can be included in the file by starting the comment with
** a pound-sign (#), the comment ends at EOL.
*/

/* load the specified configuration file
** returns zero (0) on success, non-zero on failure */
int cfg_loadfile(char *fname);

/* get a named configuration value
** returns zero (0) on success, non-zero on failure */
int cfg_getval(char *name, char **value);

/* set a named configuration value
** returns zero (0) on success, non-zero on failure */
int cfg_setval(char *name, char *value);

/* watch a configuration value, call function (cb) when value changes
** returns zero (0) on success, non-zero on failure */
int cfg_watchval(char *name, int (*cb)(char *name, char *value));

/* save configuration values to file (previously loaded file if fname=NULL)
** returns zero (0) on success, non-zero on failure */
int cfg_savefile(char *fname);

/* reset the configuration environment */
void cfg_reset(void);

/* execute an iteration function on the config table */
int cfg_iterate(int (*p_func)(unsigned long slot, void *key, void *val), int *p_rv);

#define STR 1 /* anything that isn't INT, REAL, BOOL, DATE or TIME */
#define INT 2 /* signed decimal integer [+-]?[0-9]+ */
#define FIX 3 /* [0-9]+.[0-9]+, [0-9]+.[0-9]* or ([0-9]*].)?[0-9]+[Ee][+-][908]+ */
#define BOOL 4 /* 1, 0, Y, N, T, F, YES, NO, TRUE, FALSE, ON, OFF */
#define DATE 5 /* nnnn-nn-nn or nn-nn-nnnn using '/', '-' or '.' delimiters */
#define TIME 6 /* nn:nn, nn:nn:nn or nn:nn:nn.nn with optional AM/PM */
#define HEX 7 /* unsigned hexidecimal integer 0x[0-9]+ */
#define OCT 8 /* unsigned octal integer 0[0-7]+ */

/* return the apparant type of the named configuration value
** returns zero (0) if no such value item exists or if name is null */
int cfg_valtype(char *name);

/* return the aparant type of the specified string value
** returns zero if the string can't be typed or is null */
int cfg_strtype(char *str);

#endif
