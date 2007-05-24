//=============================================================================
// This file is part of Kcli, a command line interface in a Linux kernel
// module for embedded Linux applications.
//
// Copyright © 2007 JavaDevices Software LLC.
//
// This file was derived directly from diet libc, which is a user space
// C library developed by Felix von Leitner. The original source code
// file contains no specific copyright notice, but, it was released under
// the GNU General Public License, version 2, and this file retains that
// license.
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
// rx.h
//
// Kcli regular expressions interface.
//
//=============================================================================

#ifndef _REGEX_H
#define _REGEX_H

typedef ptrdiff_t regoff_t;

typedef struct {
  regoff_t rm_so;
  regoff_t rm_eo;
} regmatch_t;

#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOSUB 4
#define REG_NEWLINE 8

#define REG_NOTBOL 1
#define REG_NOTEOL 2

#define REG_NOMATCH -1

#define RE_DUP_MAX 8192

struct __regex_t;

typedef int (*matcher)(void*,const char*,int ofs,struct __regex_t* t,int plus,int eflags);

typedef struct __regex_t {
  struct regex {
    matcher m;
    void* next;
    int pieces;
    int num;
    struct branch* b;
  } r;
  int brackets,cflags;
  regmatch_t* l;
} regex_t;
#define re_nsub r.pieces

int regcomp(regex_t* preg, const char* regex, int cflags);
int regexec(const regex_t* preg, const char* string, size_t nmatch, regmatch_t pmatch[], int eflags);
size_t regerror(int errcode, const regex_t* preg, char* errbuf, size_t errbuf_size);
void regfree(regex_t* preg);

enum __regex_errors {
  REG_NOERROR,
  REG_BADRPT, /* Invalid use of repetition operators such as using `*' as the first character. */
  REG_BADBR, /* Invalid use of back reference operator. */
  REG_EBRACE, /* Un-matched brace interval operators. */
  REG_EBRACK, /* Un-matched bracket list operators. */
  REG_ERANGE, /* Invalid use of the range operator, eg. the ending point of the
		 range occurs  prior  to  the  starting point. */
  REG_ECTYPE, /* Unknown character class name. */
  REG_ECOLLATE, /* Invalid collating element. */
  REG_EPAREN, /* Un-matched parenthesis group operators. */
  REG_ESUBREG, /* Invalid back reference to a subexpression. */
  REG_EEND, /* Non specific error.  This is not defined by POSIX.2. */
  REG_EESCAPE, /* Trailing backslash. */
  REG_BADPAT, /* Invalid use of pattern operators such as group or list. */
  REG_ESIZE, /* Compiled  regular  expression  requires  a  pattern  buffer
		larger than 64Kb.  This is not defined by POSIX.2. */
  REG_ESPACE /* regcomp ran out of space */
};

char* re_comp(char* regex);
int re_exec(char* string);

static inline int isdigit(char c)
{
    if ((c >= '0') && (c <= '9')) return 1;
    return 0;
}

static inline int isalnum(char c)
{
    if ((c >= '0') && (c <= '9')) return 1;
    if ((c >= 'a') && (c <= 'z')) return 1;
    if ((c >= 'A') && (c <= 'Z')) return 1;
    return 0;
}

static inline char tolower(char c)
{
    if ((c >= 'A') && (c <= 'Z')) return c & 0xdf;
    return c;
}

#endif

