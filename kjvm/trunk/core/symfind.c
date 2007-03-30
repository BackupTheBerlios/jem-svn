//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 Sombrio Systems Inc. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
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
//==============================================================================

#include "symfind.h"
#include "symbols.h"

char *findCoreSymbol(jint addr)
{
	jint i;
	for (i = 0; i < n_symbols; i++) {
		if (symbols[i].addr <= addr && symbols[i].addr + symbols[i].size >= addr) {
			return &(strings[symbols[i].name]);
		}
	}
	return 0;
}

u32 sizeCoreSymbol(const char *name)
{
	int i;
	for (i = 0; i < n_symbols; i++) {
		if (strcmp(&(strings[symbols[i].name]), name) == 0)
			return symbols[i].size;
	}
#ifdef DEBUG
	sys_panic("symbol: %s not found!", name);
#endif
	return 0;
}

char *addrCoreSymbol(const char *name)
{
	int i;
	for (i = 0; i < n_symbols; i++) {
		if (strcmp(&(strings[symbols[i].name]), name) == 0)
			return (char *) symbols[i].addr;
	}
#ifdef DEBUG
	sys_panic("symbol: %s not found!", name);
#endif
	return 0;
}

void printCoreSymbolInformation(char *name)
{
	jint i;
	for (i = 0; i < n_symbols; i++) {
		if (strcmp(name, &strings[symbols[i].name]) == 0) {
			printf("%s addr:0x%lx - 0x%lx (size: %ld)\n ", &strings[symbols[i].name], symbols[i].addr,
			       symbols[i].addr + symbols[i].size, symbols[i].size);
		}
	}
	return;
}

#define NOINTIN(x) if (eip >= (u32)x && eip <= (u32)x + FKTSIZE_##x) return 1;

void thread_exit();
void receive_portalcall();
#ifndef FKTSIZE_thread_exit
#define FKTSIZE_thread_exit 0
#endif
#ifndef FKTSIZE_receive_portalcall
#define FKTSIZE_receive_portalcall 0
#endif
#ifndef FKTSIZE_callnative_special_portal
#define FKTSIZE_callnative_special_portal 0
#endif
#ifndef FKTSIZE_memory_set32
#define FKTSIZE_memory_set32 0
#endif

void irq_exit();
extern unsigned char irq_exit_end[];

//void callnative_special_portal();
extern unsigned char callnative_special_portal_end[];

int eip_in_last_stackframe(u32 eip)
{
	NOINTIN(thread_exit);
#ifdef KERNEL
	if (eip >= (u32) irq_exit && eip <= (u32) irq_exit_end)
		return 1;
#endif
	//  if (eip >= (u32*)callnative_special_portal && eip <= (u32*)callnative_special_portal_end) return 1;

	NOINTIN(receive_portalcall);

	return 0;
}


int in_portalcall(u32 * eip)
{
	NOINTIN(receive_portalcall);
	//NOINTIN(callnative_special_portal);
	return 0;
}

#if 0
int in_fastportalcall(u32 * eip)
{
	NOINTIN(memory_set32);
	return 0;
}
#endif
