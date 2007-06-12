//=================================================================================
// This file is part of Jem, a real time Java operating system designed for
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 2001-2002 Joerg Baumann. All rights reserved.
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
//=================================================================================
//
//
//=================================================================================

#ifdef CONFIG_JEM_ENABLE_GC
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include "jemtypes.h"
#include "malloc.h"
#include "jemConfig.h"
#include "object.h"
#include "domain.h"
#include "gc.h"
#include "portal.h"
#include "thread.h"
#include "code.h"
#include "gc_memcpy.h"
#include "gc_move.h"
#include "symfind.h"

extern unsigned char callnative_special_end[], callnative_special_portal_end[], callnative_static_end[], thread_exit_end[];
void return_from_java0(ThreadDesc * next, ContextDesc * restore);
void return_from_java1(long param, ContextDesc * restore, ThreadDesc * next);
void return_from_java2(long param1, long param2, ThreadDesc * next, ContextDesc * restore);
extern unsigned char return_from_javaX_end[], never_return_end[];
void never_return(void);

jboolean find_stackmap(MethodDesc * method, u32 * eip, u32 * ebp, jbyte * stackmap, u32 maxslots, u32 * nslots)
{

	SymbolDescStackMap *sym;
	jbyte *addr;
	char *symip, *symippre;
	int j, k;
	jint numSlots;
	jbyte b = 0;

	for (j = 0; j < method->numberOfSymbols; j++) {
		if ((method->symbols[j]->type != 16) && (method->symbols[j]->type != 17))
			continue;
		sym = (SymbolDescStackMap *) method->symbols[j];
		symip = (char *) method->code + (jint) (sym->immediateNCIndex);
		symippre = (char *) method->code + (jint) (sym->immediateNCIndexPre);

		if ((u32 *) symip == eip || (u32 *) symippre == eip) {
			addr = sym->map;
			numSlots = sym->n_bits;
			if (numSlots > maxslots) {
                printk(KERN_ERR "Stack frame too large.\n");
                return JNI_FALSE;
            }
			*nslots = numSlots;
			for (k = 0; k < sym->n_bits; k++) {
				if (k % 8 == 0)
					b = *addr++;
				stackmap[k] = b & 1;
				b >>= 1;
			}
			return JNI_TRUE;
		}
	}
	return JNI_FALSE;
}

void list_stackmaps(MethodDesc * method)
{
	SymbolDescStackMap *sym;
	jbyte *addr;
	char *symip;
	int j, k;
	jbyte b = 0;

	for (j = 0; j < method->numberOfSymbols; j++) {

		if ((method->symbols[j]->type != 16) && (method->symbols[j]->type != 17))
			continue;

		sym = (SymbolDescStackMap *) method->symbols[j];
		symip = (char *) method->code + (jint) (sym->immediateNCIndex);

		printk(KERN_INFO "   stackmap at: %p  (IPpre: %p)\n", symip, (char *) method->code + (jint) (sym->immediateNCIndexPre));

		addr = sym->map;
		for (k = 0; k < sym->n_bits; k++) {
			if (k % 8 == 0)
				b = *addr++;
			printk(KERN_INFO "%d", b & 1);
			b >>= 1;
		}

		printk(KERN_INFO "\n");
	}
}

void walkStack(DomainDesc * domain, ThreadDesc * thread, HandleReference_t handleReference)
{
    int i, k;
    u32 *ebp, *eip, *sp, *s, *prevSP;
    ClassDesc *classInfo;
    MethodDesc *method, *prevMethod;
    char *sig;
    jint bytecodePos, lineNumber;
    jint numSlots;
    jbyte stackmap[128];
    extern char _start[], end[];

    prevMethod = NULL;
    prevSP = NULL;

    ebp = (u32 *) thread->context[PCB_EBP];
    sp = (u32 *) thread->context[PCB_ESP];
    eip = (u32 *) thread->context[PCB_EIP];

    while (sp != NULL && sp < thread->stackTop) {

        prevMethod = method;
        if (eip >= (u32 *) _start && eip <= (u32 *) end) {
            /* our own text segment */
            if (eip_in_last_stackframe((u32) eip)) {
                if (*(eip - 1) == 0xfb) break;  // no more stack frames
            }
            if ((eip >= (u32 *) callnative_special && eip <= (u32 *) callnative_special_end)
                || (eip >= (u32 *) callnative_static && eip <= (u32 *) callnative_static_end)
                || (eip >= (u32 *) callnative_special_portal && eip <= (u32 *) callnative_special_portal_end)
                || (eip >= (u32 *) return_from_java0 && eip <= (u32 *) return_from_javaX_end)
                || (eip >= (u32 *) return_from_java1 && eip <= (u32 *) return_from_javaX_end)
                || (eip >= (u32 *) return_from_java2 && eip <= (u32 *) return_from_javaX_end)
                || (eip >= (u32 *) never_return && eip <= (u32 *) never_return_end)
               ) {
                /* C -> Java */
                /*  scan parameters */

                if (prevMethod != NULL) {
                    u32 *s = sp;
                    s += 2;
                    /* callnative_static  does not put an ObjectDesc onto the stack */
                    if (!(eip >= (u32 *) callnative_static && eip <= (u32 *)
                          callnative_static_end)) {
                        if (handleReference) {
                            handleReference(domain, (ObjectDesc **) s);
                        }
                        s++;
                    }
                    for (i = 1; i < prevMethod->numberOfArgs + 1; i++) {
                        if (isRef(prevMethod->argTypeMap, prevMethod->numberOfArgs, i - 1)) {
                            if (handleReference) {
                                handleReference(domain, (ObjectDesc **)
                                                s);
                            }
                        }
                        s++;
                    }

                }
                // break; /* no more Java stack frames; NOT TRUE: newString, executeSpecial, ... */
                /* no more Java stack frames for these functions: */
                if ((eip >= (u32 *) return_from_java0 && eip <= (u32 *) return_from_javaX_end)
                    || (eip >= (u32 *) return_from_java1 && eip <= (u32 *) return_from_javaX_end)
                    || (eip >= (u32 *) return_from_java2 && eip <= (u32 *) return_from_javaX_end)
                    || (eip >= (u32 *) never_return && eip <= (u32 *) never_return_end)
                   )
                    break;

            }
        } else {
            int q;
            q = findMethodAtAddrInDomain(domain, (char *) eip, &method, &classInfo, &bytecodePos, &lineNumber);
            if (q == 0) {
                if (!find_stackmap(method, eip, ebp, stackmap, sizeof(stackmap), &numSlots)) {
                    printk(KERN_WARNING "No stackmap for this frame! at %p; thread=%p\n", eip, thread);
                    list_stackmaps(method);
                    return;
                }
                for (s = ebp - 1, k = 0; s > (sp + 1); s--, k++) {
                    if (handleReference) {
                        if (stackmap[k]) {  /* found reference */
                            handleReference(domain, (ObjectDesc **) s);

                        }
                    }
                }
            } else {
                int q;
                q = findProxyCodeInDomain(domain, eip, &method, &sig, &classInfo);
                if (q != 0) {
                    printk(KERN_ERR "walkStack: Warning: Strange eip thread %d.%d (%p) eip=%p esp=%p ebp=%p\n",
                           TID(thread), thread, eip, sp, ebp);
                }
            }
        }
        prevSP = sp;

        sp = ebp;
        if (sp == NULL)
            break;
        ebp = (u32 *) * sp;
        eip = (u32 *) * (sp + 1);
    }
}
#endif				/* ENABLE_GC */
