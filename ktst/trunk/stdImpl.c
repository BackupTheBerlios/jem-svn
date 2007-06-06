/*
 * COPYRIGHT AND PERMISSION NOTICE
 * 
 * Copyright (c) 2007 Christopher Stone 
 * Copyright (c) 2003 Embedded Unit Project
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, and/or sell copies of the Software, and to permit persons 
 * to whom the Software is furnished to do so, provided that the above 
 * copyright notice(s) and this permission notice appear in all copies 
 * of the Software and that both the above copyright notice(s) and this 
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT 
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY 
 * SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * Except as contained in this notice, the name of a copyright holder 
 * shall not be used in advertising or otherwise to promote the sale, 
 * use or other dealings in this Software without prior written 
 * authorization of the copyright holder.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "stdImpl.h"
#include "libcli.h"

extern struct cli_def       *kcli;

char* stdimpl_strcpy(char *dst, const char *src)
{
	return strcpy(dst, src);
}

char* stdimpl_strcat(char *dst, const char *src)
{
	return strcat(dst, src);
}

char* stdimpl_strncat(char *dst, const char *src,unsigned int count)
{
	return strncat(dst, src, count);
}

int stdimpl_strlen(const char *str)
{
	return strlen(str);
}

int stdimpl_strcmp(const char *s1, const char *s2)
{
	return strcmp(s1, s2);
}

static char* _xtoa(unsigned long v,char *string, int r, int is_neg)
{
	char *start = string;
	char buf[33],*p;

	p = buf;

	do {
		*p++ = "0123456789abcdef"[(v % r) & 0xf];
	} while (v /= r);

	if (is_neg) {
		*p++ = '-';
	}

	do {
		*string++ = *--p;
	} while (buf != p);

	*string = '\0';

	return start;
}

char* stdimpl_itoa(int v,char *string,int r)
{
    if ((r == 10) && (v < 0)) {
		return _xtoa((unsigned long)(-v), string, r, 1);
	}
	return _xtoa((unsigned long)(v), string, r, 0);
}

void stdimpl_print(const char *string)
{
	cli_print(kcli, "%s", string);
}
