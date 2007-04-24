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
// rx.c
// 
// Kcli regular expressions implementation.
// 
//=============================================================================

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include "regex.h"

#define NDEBUG

/* this is ugly.
 * the idea is to build a parse tree, then do some poor man's OOP with a
 * generic matcher function call that is always that the start of each
 * record, and a next pointer.  When the parse tree is done, we need to
 * recursively set the next pointers to point to the part of the parse
 * tree that needs to match next.
 * This is the prototype of the generic match function call pointer.
 * The first argument is the "this" pointer, the second is the text to
 * be matched against, ofs is the offset from the start of the matched
 * text (so we can match "^") and matches is an array where match
 * positions are stored. */
/* now declared in regex.h: */
/* typedef int (*matcher)(void*,const char*,int ofs,regmatch_t* matches,int plus,int eflags); */

/* one would think that this is approach is an order of magnitude slower
 * than the standard NFA approach, but it isn't.  The busybox grep took
 * 0.26 seconds for a fixed string compared to 0.19 seconds for the
 * glibc regex. */

/* first part: parse a regex into a parse tree */
struct bracketed {
    unsigned int cc[32];
};

/* now declared in regex.h:
struct regex {
  matcher m;
  void* next;
  int pieces;
  int num;
  struct branch *b;
}; */

struct atom {
    matcher m;
    void* next;
    enum {
        ILLEGAL, EMPTY, REGEX, BRACKET, ANY, LINESTART, LINEEND, WORDSTART, WORDEND, CHAR,
    } type;
    int bnum;
    union {
        struct regex r;
        struct bracketed b;
        char c;
    } u;
};

struct piece {
    matcher m;
    void* next;
    struct atom a;
    unsigned int min,max;
};

struct branch {
    matcher m;
    void* next;
    int num;
    struct piece *p;
};

static void clearcc(unsigned int* x) {
    memset(x,0,sizeof(struct bracketed));
}

static void setcc(unsigned int* x,unsigned int bit) {
    x[bit/32]|=(1<<((bit%32)-1));
}

static int issetcc(unsigned int* x,unsigned int bit) {
    return x[bit/32] & (1<<((bit%32)-1));
}

static const char* parsebracketed(struct bracketed*__restrict__ b,const char*__restrict__ s,regex_t*__restrict__ rx) {
    const char* t;
    int i,negflag=0;
    if (*s!='[') return s;
    t=s+1;
    clearcc(b->cc);
    if (*t=='^') {
        negflag=1; ++t;
    }
    do {
        if (*t==0) return s;
        setcc(b->cc,rx->cflags&REG_ICASE?tolower(*t):*t);
        if (t[1]=='-' && t[2]!=']') {
            for (i=*t+1; i<=t[2]; ++i) setcc(b->cc,rx->cflags&REG_ICASE?tolower(i):i);
            t+=2;
        }
        ++t;
    } while (*t!=']');
    if (negflag) for (i=0; i<32; ++i) b->cc[i]=~b->cc[i];
    return t+1;
}

static const char* parseregex(struct regex* r,const char* s,regex_t* rx);

static int matchatom(void*__restrict__ x,const unsigned char*__restrict__ s,int ofs,struct __regex_t*__restrict__ preg,int plus,int eflags) {
    register struct atom* a=(struct atom*)x;
    int matchlen=0;
    switch (a->type) {
    case EMPTY:
#ifdef DEBUG
        printk("matching atom EMPTY against \"%.20s\"\n",s);
        printk("a->bnum is %d\n",a->bnum);
#endif
        if (a->bnum>=0) preg->l[a->bnum].rm_so=preg->l[a->bnum].rm_eo=ofs;
        goto match;
    case REGEX:
#ifdef DEBUG
        printk("matching atom REGEX against \"%.20s\"\n",s);
        printk("a->bnum is %d\n",a->bnum);
#endif
        if ((matchlen=a->u.r.m(&a->u.r,(const char*)s,ofs,preg,0,eflags))>=0) {
            preg->l[a->bnum].rm_so=ofs;
            preg->l[a->bnum].rm_eo=ofs+matchlen;
            goto match;
        }
        break;
    case BRACKET:
#ifdef DEBUG
        printk("matching atom BRACKET against \"%.20s\"\n",s);
#endif
        matchlen=1;
        if (*s=='\n' && (preg->cflags&REG_NEWLINE)) break;
        if (*s && issetcc(a->u.b.cc,(preg->cflags&REG_ICASE?tolower(*s):*s)))
            goto match;
        break;
    case ANY:
#ifdef DEBUG
        printk("matching atom ANY against \"%.20s\"\n",s);
#endif
        if (*s=='\n' && (preg->cflags&REG_NEWLINE)) break;
        matchlen=1;
        if (*s) goto match;
        break;
    case LINESTART:
#ifdef DEBUG
        printk("matching atom LINESTART against \"%.20s\"\n",s);
#endif
        if (ofs==0 && (eflags&REG_NOTBOL)==0) {
            goto match;
        }
        break;
    case LINEEND:
#ifdef DEBUG
        printk("matching atom LINEEND against \"%.20s\"\n",s);
#endif
        if ((*s && *s!='\n') || (eflags&REG_NOTEOL)) break;
        goto match;
    case WORDSTART:
#ifdef DEBUG
        printk("matching atom WORDSTART against \"%.20s\"\n",s);
#endif
        if ((ofs==0 || !isalnum(s[-1])) && isalnum(*s))
            goto match;
        break;
    case WORDEND:
#ifdef DEBUG
        printk("matching atom WORDEND against \"%.20s\"\n",s);
#endif
        if (ofs>0 && isalnum(s[-1]) && !isalnum(*s))
            goto match;
        break;
    case CHAR:
#ifdef DEBUG
        printk("matching atom CHAR %c against \"%.20s\"\n",a->u.c,s);
#endif
        matchlen=1;
        if (((preg->cflags&REG_ICASE)?tolower(*s):*s)==a->u.c) goto match;
        break;
    default:
        break;
    }
    return -1;
    match:
    if (a->next)
        return((struct atom*)(a->next))->m(a->next,(const char*)s+matchlen,ofs+matchlen,preg,plus+matchlen,eflags);
    else
        return plus+matchlen;
}

static const char* parseatom(struct atom*__restrict__ a,const char*__restrict__ s,regex_t*__restrict__ rx) {
    const char *tmp;
    a->m=(matcher)matchatom;
    a->bnum=-1;
    switch (*s) {
    case '(':
        a->bnum=++rx->brackets;
        if (s[1]==')') {
            a->type=EMPTY;
            return s+2;
        }
        a->type=REGEX;
        tmp=parseregex(&a->u.r,s+1,rx);
        if (*tmp==')')
            return tmp+1;
    case 0:
    case '|':
    case ')':
        return s;
    case '[':
        a->type=BRACKET;
        if ((tmp=parsebracketed(&a->u.b,s,rx))!=s)
            return tmp;
        return s;
    case '.':
        a->type=ANY;
        break;
    case '^':
        a->type=LINESTART;
        break;
    case '$':
        a->type=LINEEND;
        break;
    case '\\':
        if (!*++s) return s;
        if (*s=='<') {
            a->type=WORDSTART;
            break;
        } else if (*s=='>') {
            a->type=WORDEND;
            break;
        }
    default:
        a->type=CHAR;
        a->u.c=rx->cflags&REG_ICASE?tolower(*s):*s;
        break;
    }
    return s+1;
}

/* needs to do "greedy" matching, i.e. match as often as possible */
static int matchpiece(void*__restrict__ x,const char*__restrict__ s,int ofs,struct __regex_t*__restrict__ preg,int plus,int eflags) {
    register struct piece* a=(struct piece*)x;
    int matchlen=0;
    int tmp=0,num=0;
    unsigned int *offsets;
#ifdef DEBUG
    printk("alloca(%d)\n",sizeof(int)*a->max);
#endif
    offsets=kmalloc(sizeof(int)*a->max, GFP_KERNEL);
    offsets[0]=0;
//  printk("allocating %d offsets...\n",a->max);
//  printk("matchpiece \"%s\"...\n",s);
    /* first, try to match the atom as often as possible, up to a->max times */
    if (a->max == 1 && a->min == 1)
        return a->a.m(&a->a,s+matchlen,ofs+matchlen,preg,0,eflags);
    while ((unsigned int)num<a->max) {
        void* save=a->a.next;
        a->a.next=0;
        if ((tmp=a->a.m(&a->a,s+matchlen,ofs+matchlen,preg,0,eflags))>=0) {
            a->a.next=save;
            ++num;
            matchlen+=tmp;
//      printk("setting offsets[%d] to %d\n",num,tmp);
            offsets[num]=tmp;
        } else {
            a->a.next=save;
            break;
        }
    }
    if ((unsigned int)num<a->min) return -1;      /* already at minimum matches; signal mismatch */
    /* then, while the rest does not match, back off */
    for (;num>=0;) {
        if (a->next)
            tmp=((struct atom*)(a->next))->m(a->next,s+matchlen,ofs+matchlen,preg,plus+matchlen,eflags);
        else
            tmp=plus+matchlen;
        if (tmp>=0) break;  /* it did match; don't back off any further */
//    printk("using offsets[%d] (%d)\n",num,offsets[num]);
        matchlen-=offsets[num];
        --num;
    }
    return tmp;
}

static const char* parsepiece(struct piece*__restrict__ p,const char*__restrict__ s,regex_t*__restrict__ rx) {
    const char* tmp=parseatom(&p->a,s,rx);
    if (tmp==s) return s;
    p->m=matchpiece;
    p->min=p->max=1;
    switch (*tmp) {
    case '*': p->min=0; p->max=RE_DUP_MAX; break;
    case '+': p->min=1; p->max=RE_DUP_MAX; break;
    case '?': p->min=0; p->max=1; break;
    case '{':
        if (isdigit(*++tmp)) {
            p->min=*tmp-'0'; p->max=RE_DUP_MAX;
            while (isdigit(*++tmp)) p->min=p->min*10+*tmp-'0';
            if (*tmp==',') {
                if (isdigit(*++tmp)) {
                    p->max=*tmp-'0';
                    while (isdigit(*++tmp)) p->max=p->max*10+*tmp-'0';
                }
            } else
                p->max=p->min;
            if (*tmp!='}') return s;
            ++tmp;
        }
    default:
        return tmp;
    }
    return tmp+1;
}

/* trivial, just pass through */
static int matchbranch(void*__restrict__ x,const char*__restrict__ s,int ofs,struct __regex_t*__restrict__ preg,int plus,int eflags) {
    register struct branch* a=(struct branch*)x;
    int tmp;
#ifdef DEBUG
    printk("%08p matching branch against \"%.20s\"\n",a,s);
    printk("%p %p\n",&a->p->m,a->p->m);
#endif
    tmp=a->p->m(a->p,s,ofs,preg,plus,eflags);
    if (tmp>=0) {
        if (a->next)
            return((struct atom*)(a->next))->m(a->next,s+tmp,ofs+tmp,preg,plus+tmp,eflags);
        else
            return plus+tmp;
    }
    return -1;
}

static int matchempty(void*__restrict__ x,const char*__restrict__ s,int ofs,struct __regex_t*__restrict__ preg,int plus,int eflags) {
    return 0;
}

static const char* parsebranch(struct branch*__restrict__ b,const char*__restrict__ s,regex_t*__restrict__ rx,int*__restrict__ pieces) {
    struct piece p;
    const char *tmp = NULL;
    b->m=matchbranch;
    b->num=0; b->p=0;
    for (;;) {
        if (*s=='|' && b->num==0) {
            tmp=s+1;
            p.a.type=EMPTY;
            p.a.m=matchempty;
            p.min=p.max=1;
            p.m=matchpiece;
        } else {
            tmp=parsepiece(&p,s,rx);
            if (tmp==s) return s;
        }
//    printk("b->p from %p to ",b->p);
        {
            struct piece* tmp;
            if (!(tmp=kmalloc(++b->num*sizeof(p), GFP_KERNEL))) return s;
//      printk("piece realloc: %p -> %p (%d*%d)\n",b->p,tmp,b->num,sizeof(p));
            memcpy(tmp, b->p, b->num*sizeof(p));
            kfree(b->p);
            b->p=tmp;
        }
//    printk("%p (size %d)\n",b->p,b->num*sizeof(p));
        b->p[b->num-1]=p;
//    printk("assigned piece %d in branch %p\n",b->num-1,b->p);
        if (*tmp=='|') break;
        s=tmp;
    }
    *pieces+=b->num;
    return tmp;
}

/* try the branches one by one */
static int matchregex(void*__restrict__ x,const char*__restrict__ s,int ofs,struct __regex_t*__restrict__ preg,int plus,int eflags) {
    register struct regex* a=(struct regex*)x;
    int i,tmp;
#ifdef DEBUG
    printk("%08p matching regex against \"%.20s\"\n",a,s);
#endif
    for (i=0; i<a->num; ++i) {
        tmp=a->b[i].m(&a->b[i],s,ofs,preg,plus,eflags);
        if (tmp>=0) {
            if (a->next)
                return((struct atom*)(a->next))->m(a->next,s+tmp,ofs+tmp,preg,plus+tmp,eflags);
            else
                return plus+tmp;
        }
    }
    return -1;
}

static const char* parseregex(struct regex*__restrict__ r,const char*__restrict__ s,regex_t*__restrict__ p) {
    struct branch b;
    const char *tmp;
    r->m=matchregex;
    r->num=0; r->b=0; r->pieces=0;
    p->brackets=0;
    b.next=0;
    if (*s==')' || !*s) {
        r->m=matchempty;
        return s;
    }
    for (;;) {
        tmp=parsebranch(&b,s,p,&r->pieces);
        if (tmp==s && *s!=')') return s;
//    printk("r->b from %p to ",r->b);
        {
            struct branch* tmp;
            if (!(tmp=kmalloc(++r->num*sizeof(b), GFP_KERNEL))) return s;
//      printk("branch realloc: %p -> %p (%d*%d)\n",r->b,tmp,r->num,sizeof(b));
            memcpy(tmp, r->b, r->num*sizeof(b));
            kfree(r->b);
            r->b=tmp;
        }
//    printk("%p (size %d)\n",r->b,r->num*sizeof(b));
        r->b[r->num-1]=b;
        if (*s==')') {
            r->b[r->num-1].m=matchempty;
            return s;
        }
//    printk("assigned branch %d at %p\n",r->num-1,r->b);
        s=tmp;
        if (*s==')') return s;
        if (*s=='|') ++s;
    }
    return tmp;
}


/* The matcher relies on the presence of next pointers, of which the
 * parser does not know the correct destination.  So we need an
 * additional pass through the data structure that sets the next
 * pointers correctly. */
static void regex_putnext(struct regex* r,void* next);

static void atom_putnext(struct atom*__restrict__ a,void*__restrict__ next) {
    a->next=next;
    if (a->type==REGEX)
        regex_putnext(&a->u.r,0);
}

static void piece_putnext(struct piece*__restrict__ p,void*__restrict__ next) {
    p->next=next;
    atom_putnext(&p->a,next);
}

static void branch_putnext(struct branch*__restrict__ b,void*__restrict__ next) {
    int i;
    if (b->m!=matchempty) {
        for (i=0; i<b->num-1; ++i) {
            if (b->p[i+1].min==1 && b->p[i+1].max==1)
                piece_putnext(&b->p[i],&b->p[i+1].a);
            else
                piece_putnext(&b->p[i],&b->p[i+1]);
        }
        piece_putnext(&b->p[i],0);
    }
    b->next=next;
}

static void regex_putnext(struct regex*__restrict__ r,void*__restrict__ next) {
    int i;
    for (i=0; i<r->num; ++i)
        branch_putnext(&r->b[i],next);
    r->next=next;
}



int regcomp(regex_t*__restrict__ preg, const char*__restrict__ regex, int cflags) {
    const char* t;
    preg->cflags=cflags;
    t=parseregex(&preg->r,regex,preg);
    if (t==regex && *regex!=0) return -1;
    regex_putnext(&preg->r,0);
    return 0;
}

int regexec(const regex_t*__restrict__ preg, const char*__restrict__ string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    int matched;
    const char *orig=string;
    for (matched=0; (unsigned int)matched<nmatch; ++matched)
        pmatch[matched].rm_so=-1;
#ifdef DEBUG
    printk("alloca(%d)\n",sizeof(regmatch_t)*(preg->brackets+3));
#endif
    ((regex_t*)preg)->l=kmalloc(sizeof(regmatch_t)*(preg->brackets+3), GFP_KERNEL);
    while (1) {
        matched=preg->r.m((void*)&preg->r,string,string-orig,(regex_t*)preg,0,eflags);
//    printk("ebp on stack = %x\n",stack[1]);
        if (matched>=0) {
            preg->l[0].rm_so=string-orig;
            preg->l[0].rm_eo=string-orig+matched;
            if ((preg->cflags&REG_NOSUB)==0) memcpy(pmatch,preg->l,nmatch*sizeof(regmatch_t));
            return 0;
        }
        if (!*string) break;
        ++string; eflags|=REG_NOTBOL;
    }
    return REG_NOMATCH;
}

static void __regfree(struct regex* r) {
    int i;
    for (i=0; i<r->num; ++i) {
        int j,k;
        k=r->b[i].num;
        for (j=0; j<k; ++j)
            if (r->b[i].p[j].a.type==REGEX)
                __regfree(&r->b[i].p[j].a.u.r);
        kfree(r->b[i].p);
    }
    kfree(r->b);
}

void regfree(regex_t* preg) {
    __regfree(&preg->r);
    memset(preg,0,sizeof(regex_t));
}

size_t regerror(int errcode, const regex_t*__restrict__ preg, char*__restrict__ errbuf, size_t errbuf_size) {
    strncpy(errbuf,"invalid regular expression (sorry)",errbuf_size);
    return strlen(errbuf);
}

EXPORT_SYMBOL(regcomp);
EXPORT_SYMBOL(regexec);
EXPORT_SYMBOL(regfree);

