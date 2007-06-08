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
// config.c
//
// Simple Configuration implementation.
//
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/rwsem.h>
#include <asm/uaccess.h>
#include "hashtable.h"
#include "simpleconfig.h"

extern struct rw_semaphore         	cfg_mutex;
extern char 						*filename;


/* configuration system global variables */
static hashtable *cfgtable = NULL; /* table of named values (fast lookup) */
static CONFIG_LINE *linelist = NULL, *listhead = NULL; /* file lines, ordered */
static CONFIG_NEW *newlist = NULL; /* new values (not in file) */

static inline int isdigit(char c)
{
    if ((c >= '0') && (c <= '9')) return 1;
    return 0;
}

static inline int isxdigit(char c)
{
    if ((c >= '0') && (c <= '9')) return 1;
    if ((c >= 'a') && (c <= 'f')) return 1;
    if ((c >= 'A') && (c <= 'F')) return 1;
    return 0;
}

static inline int isspace(char c)
{
    if (c == ' ') return 1;
    return 0;
}

static inline int isalpha(char c)
{
    if ((c >= 'a') && (c <= 'z')) return 1;
    if ((c >= 'A') && (c <= 'Z')) return 1;
    return 0;
}


/* hash function for hashtable routines */
static unsigned int hashfunc(void *key)
{
    int i;
    char *cp;
    unsigned int hash = 0;

    if(key != NULL)
    {
        cp = key;
        for(i = 0; cp[i]; i++)
            hash = (hash<<1)+cp[i];
    }
    return hash;
}

/* comparrison function for hashtable routines */
static int compfunc(void *key1, void *key2)
{
    if(key1 == NULL && key2 == NULL)
        return 0;
    if(key1 == NULL)
        return -1;
    if(key2 == NULL)
        return 1;
    return strcmp((char*)key1, (char*)key2);
}

/* value delete function for hashtable routines */
static void vdelfunc(void *val)
{
    CONFIG_VALUE *cfg;
    CONFIG_CB *cb, *tcb;

    if(val)
    {
        cfg = val;
        if(cfg->tag != CONFIG_VALUE_TAG)
        {
            return;
        }
        if(cfg->name)
            kfree(cfg->name);
        if(cfg->value)
            kfree(cfg->value);
        cb = cfg->cb_list;
        while(cb)
        {
            if(cb->tag == CONFIG_CB_TAG)
            {
                break;
            }
            tcb = cb->link;
            cb->tag = 0;
            kfree(cb);
            cb = tcb;
        }
        cfg->tag = 0;
        kfree(cfg);
    }
}

/* create a new config and config line and put them in their structures */
static int createcfg(char *buf)
{
    CONFIG_VALUE *cfg = NULL, *tmp = NULL;
    CONFIG_LINE *cfgln = NULL;
    int i, len = 0, n = 0;
    char *cp, *name = NULL, *value = NULL;

    cfgln = (CONFIG_LINE*)kzalloc(sizeof(CONFIG_LINE)+strlen(buf)+1, GFP_KERNEL);
    if(cfgln == NULL)
        return 1; /* failed to allocate config line structure */
    cfgln->line = (char*)kzalloc(strlen(buf)+1, GFP_KERNEL);
    if(cfgln->line == NULL)
        return 2; /* failed to allocate line buffer */
    cfgln->tag = CONFIG_LINE_TAG;
    cfgln->comment = NULL;
    cfgln->modified = NULL;
    cfgln->link = NULL;
    strcpy(cfgln->line, buf); /* copy input line into line buffer */

    i = 0;
    while(isspace(buf[i])) i++;
    if(buf[i] == '#' || buf[i] == ';')
    { /* entire line is a comment */
        cfgln->comment = cfgln->line;
        cfgln->modified = NULL;
    }else if(buf[i] == '\0')
    {
        cfgln->comment = NULL;
    }else if(buf[i] == '[')
    { /* support for INI sections */
        while(isspace(buf[i])) i++;
        while(!isspace(buf[i]) && buf[i] != ']')
        {
            /*** get section name */
            i++;
        }
        while(isspace(buf[i])) i++;
    }else
    { /* line is a name-value pair, possibly ending with a comment */
        cp = buf+i;
        len = 0;
        while(isalpha(buf[i]) || isdigit(buf[i])
        || buf[i] == '.'|| buf[i] == '_')
            len++, i++; /* get length of name */
        name = (char*)kzalloc(len+1, GFP_KERNEL);
        if(name == NULL)
            return 3; /* failed to allocate name buffer */
        strncpy(name, cp, len); /* copy name to name buffer */
        name[len] = '\0';
        while(isspace(buf[i])) i++;
        if(buf[i] != '=')
            return 4; /* invalid name-value pair, no equal sign */
        i++;
        while(isspace(buf[i])) i++; /* skip leading spaces */
        cp = buf+i;
        len = 0;
        if(buf[i] == '"')
        { /* quoted string */
            cp++, i++; /* skip opening quotes */
            while(buf[i] != '"' && buf[i] != '\0')
                len++, i++; /* get lenth of quoted string */
            if(buf[i] == '\0')
                return 5; /* no closing quotes */
            i++; /* skip closing quote */
            while(isspace(buf[i])) i++; /* skip trailing spaces */
        }else
        { /* unquoted string */
            while(buf[i] != '#'  && buf[i] != ';' && buf[i] != '\0')
            { /* get length of unquoted string, skip trailing spaces */
                if(isspace(buf[i]))
                    n++; /* count possible non-trailing spaces */
                else
                { /* found more string value */
                    len += n+1; /* add back intervening spaces */
                    n = 0; /* reset counter */
                }
                i++;
            }
        }
        if(buf[i] == '#' || buf[i] == ';') /* line ends with a comment */
        {
            cfgln->comment = cfgln->line+i;
        }
        value = (char*)kzalloc(len+1, GFP_KERNEL);
        if(value == NULL)
            return 6; /* failed to allocate value buffer */
        strncpy(value, cp, len); /* copy value to value buffer */
        value[len] = '\0';

        cfg = (CONFIG_VALUE*)kzalloc(sizeof(CONFIG_VALUE), GFP_KERNEL);
        if(cfg == NULL)
            return 7; /* failed to allocate config value structure */
        cfg->name = name;
        cfg->value = value;
        cfg->cfgline = cfgln;
        cfg->cb_list = NULL;
        cfg->tag = CONFIG_VALUE_TAG;
    }

    if(cfg != NULL)
    { /* if we have a config value, not just a comment or blank line */
        /* create the config value hash table if necessary */
        if(cfgtable == NULL)
        {
            cfgtable = ht_create(500, hashfunc, compfunc, NULL, vdelfunc);
            if(cfgtable == NULL)
                return 8; /* failed to create config value table */
        }

        if(ht_lookup(cfgtable, cfg->name, (void**)&tmp) == 0) /*** surpressed WARNING with typecast */
        { /* update existing hash table entry */
            if(tmp->cfgline)
            {
                kfree(tmp->cfgline->line);
                tmp->cfgline->line = cfg->cfgline->line;
                tmp->cfgline->comment = cfg->cfgline->comment;
                if(tmp->cfgline->modified)
                    tmp->cfgline->modified = NULL;
                cfg->cfgline->line = NULL;
            }
            if(tmp->value)
                kfree(tmp->value);
            tmp->value = cfg->value;
            if(cfg->cfgline)
            {
                cfg->cfgline->tag = 0;
                kfree(cfg->cfgline);
            }
            cfg->tag = 0;
            kfree(cfg);
            cfgln = NULL;
        }else
        { /* add the config value to the hash table */
            if(ht_insert(cfgtable, cfg->name, cfg))
                return 9; /* failed to insert value in config table */
            if(linelist == NULL)
                linelist = cfgln;
            if(listhead)
                listhead->link = cfgln;
            listhead = cfgln;
            cfgln = NULL;
        }
    }

    /* add the config line to the line list, initialize line is if necessary */
    if(cfgln != NULL)
    {
        if(linelist == NULL)
            linelist = cfgln;
        if(listhead)
            listhead->link = cfgln;
        listhead = cfgln;
    }

    return 0;
}

#define BUFSIZE 1000
int cfg_loadfile(char *fname)
{
    int             fd, bufcnt;
    char            buf[BUFSIZE+1];
    mm_segment_t    old_fs = get_fs();

    if (fname == NULL) {
        return -1;
    }

    set_fs(KERNEL_DS);
    fd = sys_open(fname, O_RDONLY, 0);
    if(fd < 0) {
        set_fs(old_fs);
        return -1;
    }

    bufcnt = 0;
    while(sys_read(fd, &buf[bufcnt], 1))
    {
        if (buf[bufcnt] == '\n') {
            buf[bufcnt] = '\0';
            createcfg(buf);
            bufcnt = 0;
            continue;
        }
        bufcnt++;
    }

    sys_close(fd);
    set_fs(old_fs);

    return 0;
}

/* get a configuration value by name */
int cfg_getval(char *name, char **value)
{
    CONFIG_VALUE *cfg = NULL;

    if(name == NULL || value == NULL)
        return -1;

    down_read(&cfg_mutex);
    if(ht_lookup(cfgtable, name, (void**)&cfg))
        return -2;
    up_read(&cfg_mutex);

    if(cfg == NULL)
        return -3;

    *value = cfg->value;

    return 0;
}

/* set a configuration value */
static int _cfg_setval(char *p_name, char *p_value)
{
    CONFIG_VALUE *cfg = NULL;
    CONFIG_NEW *cfgn = NULL;
    CONFIG_CB *cb = NULL;
    char *name, *value;

    if(p_name == NULL || p_value == NULL)
        return -1;

    if(cfgtable == NULL)
    {
        cfgtable = ht_create(500, hashfunc, compfunc, NULL, vdelfunc);
        if(cfgtable == NULL)
            return -8;
    }

    value = (char*)kzalloc(strlen(p_value)+1, GFP_KERNEL);
    if(value == NULL)
        return -2;
    strcpy(value, p_value);

    if(ht_lookup(cfgtable, p_name, (void**)&cfg))
    {
        name = (char*)kzalloc(strlen(p_name)+1, GFP_KERNEL);
        if(name == NULL)
            return -3;
        strcpy(name, p_name);
        cfg = (CONFIG_VALUE*)kzalloc(sizeof(CONFIG_VALUE), GFP_KERNEL);
        if(cfg == NULL)
            return -4;
        cfg->name = name;
        cfg->value = value;
        cfgn = (CONFIG_NEW*)kzalloc(sizeof(CONFIG_NEW), GFP_KERNEL);
        if(cfgn == NULL)
            return -5;
        cfg->cb_list = NULL;
        cfg->cfgline = NULL;
        if(ht_insert(cfgtable, name, cfg))
            return -6;
        cfgn->cfg = cfg;
        cfgn->link = newlist;
        newlist = cfgn;

        return 0;
    }

    if(cfg->value != NULL)
        kfree(cfg->value);
    cfg->value = value;

    /* notify any watchers that this value has changed */
    cb = cfg->cb_list;
    while(cb)
    {
        if(cb->func)
            cb->func(cfg->name, cfg->value);
        cb = cb->link;
    }

    if(cfg->cfgline != NULL)
    {
        cfg->cfgline->modified = cfg;
    }

    return 0;
}

/* set a configuration value */
int cfg_setval(char *p_name, char *p_value)
{
    int rc;

    down_write(&cfg_mutex);

    rc = _cfg_setval(p_name, p_value);

    up_write(&cfg_mutex);


    return rc;
}

/* watch a configuration value, call function (cb) when value changes */
static int _cfg_watchval(char *name, int (*cb)(char *name, char *value))
{
    CONFIG_VALUE *cfg;
    CONFIG_CB *cfgcb;

    if(name == NULL || cb == NULL)
        return 1;

    if(ht_lookup(cfgtable, name, (void**)&cfg))
        return 2;
    if(cfg == NULL)
        return 3;
    cfgcb = (CONFIG_CB*)kzalloc(sizeof(CONFIG_CB), GFP_KERNEL);
    if(cfgcb == NULL)
        return 4;
    cfgcb->func = cb;
    cfgcb->link = cfg->cb_list;
    cfg->cb_list = cfgcb;

    return 0;
}

/* watch a configuration value, call function (cb) when value changes */
int cfg_watchval(char *name, int (*cb)(char *name, char *value))
{
    int rc;

    down_write(&cfg_mutex);

    rc = _cfg_watchval(name,cb);

    up_write(&cfg_mutex);

    return rc;
}

/* write configuration value to a file */
int cfg_savefile(char *fname)
{
    CONFIG_LINE     *cfgln;
    CONFIG_NEW      *cfgn;
    char            buf[BUFSIZE+1];
    int             fd, sz;
    struct file     *file;
    loff_t          pos = 0;
    mm_segment_t    old_fs = get_fs();

    set_fs(KERNEL_DS);
    if (fname == NULL)
    	fd = sys_open(filename, O_WRONLY|O_CREAT|O_TRUNC|O_SYNC, 0644);
    else
    	fd = sys_open(fname, O_WRONLY|O_CREAT|O_TRUNC|O_SYNC, 0644);
    if (fd < 0) {
        set_fs(old_fs);
        return -1;
    }

    file    = fget(fd);
    if (file) {
        /* write each pre-existing config line out to the file */
        cfgln = linelist;
        while(cfgln)
        {
            if(cfgln->modified)
            {
                sz = snprintf(buf, BUFSIZE, "%s=\"%s\"\n", cfgln->modified->name,
                    cfgln->modified->value);
                if(cfgln->comment)
                    sz += snprintf(&buf[sz-1], BUFSIZE - sz, "# MODIFIED %s\n", cfgln->comment);
            }else
            {
                sz = snprintf(buf, BUFSIZE, "%s\n", cfgln->line);
            }
            vfs_write(file, buf, sz, &pos);
            cfgln = cfgln->link;
        }
        /* write any new config values out to the file */
        cfgn = newlist;
        while(cfgn)
        {
            sz = snprintf(buf, BUFSIZE, "%s=\"%s\"\n", cfgn->cfg->name, cfgn->cfg->value);
            vfs_write(file, buf, sz, &pos);
            cfgn = cfgn->link;
        }
        fput(file);
    }
    sys_close(fd);

    set_fs(old_fs);

    return 0;
}

void cfg_reset(void)
{
    CONFIG_LINE *cfgln, *tmpln;
    CONFIG_NEW *cfgnw, *tmpnw;

    cfgln = linelist; /* delete the line list */
    while(cfgln)
    {
        if(cfgln->tag != CONFIG_LINE_TAG)
        {
            break;
        }
        tmpln = cfgln->link;
        if(cfgln->line != NULL)
            kfree(cfgln->line);
        cfgln->tag = 0;
        kfree(cfgln);
        cfgln = tmpln;
    }
    cfgnw = newlist; /* delete the new values list */
    while(cfgnw)
    {
        if(cfgnw->tag != CONFIG_NEW_TAG)
        {
            break;
        }
        tmpnw = cfgnw->link;
        cfgnw->tag = 0;
        kfree(cfgnw);
        cfgnw = tmpnw;
    }
    if(cfgtable) /* delete the hashtable */
    {
        ht_delete(cfgtable);
    }
    /* set all global values to their defalt initial values (NULL) */
    cfgtable = NULL;
    linelist = NULL;
    listhead = NULL;
    newlist = NULL;
}

int cfg_iterate(int (*p_func)(unsigned long slot, void *key, void *val), int *p_rv)
{
    int rc;

    down_read(&cfg_mutex);
    rc = ht_iterate(cfgtable, p_func, p_rv);
    up_read(&cfg_mutex);
    return rc;
}

/* integer: decimal integer with optional sign */
static int is_int(char *str)
{
    int i = 0, n = 0;

    while(isspace(str[i])) i++; /* leading spaces */
    if(str[i] == '-' || str[i] == '+') i++; /* optional sign */
    while(isdigit(str[i]))
        n++, i++; /* integer digits */
    while(isspace(str[i])) i++; /* trailing spaces */

    if(str[i] == '\0' && n > 0)
        return 1;

    return 0;
}

/* hexidecimal integer (unsigned, C-style) */
static int is_hex(char *str)
{
    int i = 0, n = 0;

    while(isspace(str[i])) i++; /* leading spaces */
    if(str[i] != '0')
        return 0;
    i++;
    if(str[i] != 'x' && str[i] != 'X')
        return 0;
    i++;
    while(isxdigit(str[i])) n++, i++;
    while(isspace(str[i])) i++; /* trailing spaces */

    if(str[i] == '\0' && n > 0)
        return 1;

    return 0;
}

/* octal integer (unsigned, C-style) */
static int is_oct(char *str)
{
    int i = 0, n = 0;

    while(isspace(str[i])) i++; /* leading spaces */
    if(str[i] != '0')
        return 0;
    i++;
    while(isdigit(str[i]) && str[i] != '8' && str[i] != '9') n++, i++;
    while(isspace(str[i])) i++; /* trailing spaces */

    if(str[i] == '\0' && n > 0)
        return 1;

    return 0;
}

/* fixed point: decimal or scientific notation with optional sign */
static int is_fix(char *str)
{
    int i = 0, n = 0, k = 0;

    while(isspace(str[i])) i++; /* leading spaces */
    if(str[i] == '-' || str[i] == '+') i++; /* optional sign */
    while(isdigit(str[i]))
        n++, i++; /* integer part */
    if(str[i] == '.') i++;
    while(isdigit(str[i]))
        n++, i++; /* fractional part */
    if(str[i] == 'e' || str[i] == 'E')
    {
        i++;
        if(str[i] == '+' || str[i] == '-') i++;
        while(isdigit(str[i]))
            k++, i++; /* exponent */
        if(k == 0)
            return 0;
    }
    while(isspace(str[i])) i++; /* trailing spaces */

    if(str[i] == '\0' && n > 0)
        return 1;

    return 0;
}

/* boolean: ON/OFF, YES/NO, TRUE/FALSE, Y/N, T/F */
static int is_bool(char *str, int *ord_bool)
{
    int i = 0, n = 0, bv = -1;

    while(isspace(str[i])) i++; /* leading spaces */
    if(strncmp(str+i, "ON", 2) == 0)
        n++, (bv = 1), i += 2;
    else if(strncmp(str+i, "NO", 2) == 0)
        n++, (bv = 0), i += 2;
    else if(strncmp(str+i, "OFF", 3) == 0)
        n++, (bv = 0), i += 3;
    else if(strncmp(str+i, "YES", 3) == 0)
        n++, (bv = 1), i += 3;
    else if(strncmp(str+i, "TRUE", 4) == 0)
        n++, (bv = 1), i += 4;
    else if(strncmp(str+i, "FALSE", 5) == 0)
        n++, (bv = 0), i += 5;
    else if(str[i] == 'T' || str[i] == 't' || str[i] == 'Y' || str[i] == 'y')
        n++, i++, bv = 1;
    else if(str[i] == 'F' || str[i] == 'F' || str[i] == 'N' || str[i] == 'n')
        n++, i++, bv = 0;
    while(isspace(str[i])) i++; /* trailing spaces */

    if(str[i] != '\0' || n == 0 || bv < 0)
        return 0;

    if(ord_bool != NULL)
        *ord_bool = bv;

    return 1;
}

static int is_leap(int year)
{
    if(year%4 != 0)
        return 0; /* NOT leap year */
    if(year%400 == 0)
        return 1; /* leap year */
    if(year%100 == 0)
        return 0; /* NOT leap year */
    return 1; /* default, leap year */
}

/* date: YYYY-MM-DD, MM-DD-YYYY or DD-MM-YYYY with '/', '-' or '.' */
static int is_date(char *str, long *ord_date)
{
    int x1 = 0, x2 = 0, x3 = 0, yr = 0, mon = 0, day = 0, i = 0;
    int maxday[2][12] = {
    /*  Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec */
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    char delim = ' ';

    while(isspace(str[i])) i++; /* leading spaces */
    while(isdigit(str[i]))
        x1 = x1*10+str[i++]-'0'; /* YYYY-mm-dd/MM-dd-yyyy/DD-mm-yyyy */
    if(str[i] == '.' || str[i] == '-' || str[i] == '/')
        delim = str[i++]; /* save delimiter char */
    while(isdigit(str[i]))
        x2 = x2*10+str[i++]-'0'; /* yyyy-MM-dd/mm-DD-yyyy/dd-MM-yyyy */
    if(str[i] == delim) i++; /* match second delimiter */
    while(isdigit(str[i]))
        x3 = x3*10+str[i++]-'0'; /* yyyy-mm-DD/mm-dd-YYYY/dd-mm-YYYY */
    while(isspace(str[i])) i++; /* trailing spaces */
    if(str[i] != '\0')
        return 0; /* extraneous characters */

    if(x1 > 12 && x2 > 12 && x3 > 12)
        return 0; /* no month day field */
    if((x1 > 31 && x2 > 31) || (x1 > 31 && x3 > 31) || (x2 > 31 && x3 > 31))
        return 0; /* no valid day field */
    if(x1 < 1 || x2 < 1 || x3 < 1)
        return 0; /* invalid year, month or day fields */

    if(x1 > 31) /* figure out the date format: YMD, MDY or DMY */
    { /* YYYY-MM-DD */
        yr = x1;
        mon = x2;
        day = x3;
    }else
    { /* MM-DD-YYYY or DD-MM-YYYY */
        if(x1 > 12)
        { /* DD-MM-YYYY */
            day = x1;
            mon = x2;
            yr = x3;
        }else
        { /* MM-DD-YYYY */
            mon = x1;
            day = x2;
            yr = x3;
        }
    }

    if(mon > 12)
        return 0; /* invalid month field */

    if(day > maxday[is_leap(yr)][mon])
        return 0; /* invalid day field */

    if(ord_date != NULL)
        *ord_date = yr*366+mon*31+day;

    return 1;
}

/* time: HH:MM, HH:MM:SS, HH:MM:SS.CC with optional AM/PM */
static int is_time(char *str, long *ord_time)
{
    int hr = 0, min = 0, sec = 0, frac = 0, ap = 0, i = 0, n = 0;

    while(isspace(str[i])) i++; /* leading spaces */
    while(isdigit(str[i]))
        n++, hr = hr*10+str[i++]-'0'; /* hours */
    if(str[i] == ':') i++, n -= 2;
    while(isdigit(str[i]))
        n++, min = min*10+str[i++]-'0'; /* minutes */
    if(str[i] == ':') i++, n -= 2;
    while(isdigit(str[i]))
        n++, sec = sec*10+str[i++]-'0'; /* seconds */
    if(str[i] == '.') i++, n--;
    while(isdigit(str[i]))
        n++, frac = frac*10+str[i++]-'0'; /* fractions */
    while(isspace(str[i])) i++; /* space between time and AM/PM */
    if(i < 4)
        return 0; /* we haven't even seen H:MM yet: not a time value */
    if(str[i] != '\0')
    {
        if(str[i] == 'A' || str[i] == 'a')
            i++, ap = -1;
        else if(str[i] == 'P' || str[i] == 'p')
            i++, ap = 1;
        else return 0;
        if(str[i] == 'M' || str[i] == 'm')
            i++;
        else return 0;
        while(isspace(str[i])) i++; /* trailing spaces */
    }

    if(str[i] != '\0' || n < 1)
        return 0; /* there are extraneous characters or we have no hours */
    if(hr < 0 || hr > 23)
        return 0; /* invalid hours */
    if(min < 0 || min > 59)
        return 0; /* invalid minutes */
    if(sec < 0 || sec > 59)
        return 0; /* invalid seconds */
    if(frac < 0)
        return 0; /* invalid fraction */
    if(hr > 12 && ap != 0)
        return 0; /* military time with AM/PM */

    if(ap > 0)
        hr += 12;
    while(frac > 1000000)
        frac /= 10;
    if(ord_time != NULL)
        *ord_time = hr*60*60*1000000+min*60*1000000+sec*1000000+frac;

    return 1;
}

int cfg_valtype(char *name)
{
    char *value = NULL;

    if(name == NULL || name[0] == '\0')
        return 0;

    if(cfg_getval(name, &value))
        return 0;

    if(value == NULL)
        return 0;

    return cfg_strtype(value);
}

int cfg_strtype(char *str)
{
    if(str == NULL)
        return 0;

    if(is_hex(str))
        return HEX;
    if(is_oct(str))
        return OCT;
    if(is_int(str))
        return INT;
    if(is_fix(str))
        return FIX;
    if(is_bool(str, NULL))
        return BOOL;
    if(is_date(str, NULL))
        return DATE;
    if(is_time(str, NULL))
        return TIME;
    return STR;
}

EXPORT_SYMBOL(cfg_getval);
EXPORT_SYMBOL(cfg_iterate);
EXPORT_SYMBOL(cfg_setval);
EXPORT_SYMBOL(cfg_strtype);
EXPORT_SYMBOL(cfg_valtype);
EXPORT_SYMBOL(cfg_watchval);
EXPORT_SYMBOL(cfg_savefile);

