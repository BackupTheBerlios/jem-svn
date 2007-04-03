//=================================================================================
// This file is part of Jem, a real time Java operating system designed for 
// embedded systems.
//
// Copyright © 2007 JemStone Software LLC. All rights reserved.
// Copyright © 1997-2001 The JX Group. All rights reserved.
// Copyright © 1998-2002 Michael Golm. All rights reserved.
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
// call.S
// 
// 
//==============================================================================


#define SAVE_CALLER_REGISTERS \
	pushl %ebx;\
	pushl %esi;\
	pushl %edi
	
#define RESTORE_CALLER_REGISTERS \
	popl %edi;\
	popl %esi;\
	popl %ebx

	.text
	.align 16
/* Prototype:
 * word_t callnative_special(word_t *params, object_t *obj, char *function, int params_size);
*/
	.type	callnative_special,@function
	.globl	callnative_special
callnative_special:
	pushl	%ebp
	movl	%esp,%ebp
	SAVE_CALLER_REGISTERS
	movl	8(%ebp),%ecx	/* params */
        movl	20(%ebp),%edx	/* params_size */
        decl	%edx 
        js	.L29		/* jump if params_size == 0 */
.L31:
        movl	(%ecx,%edx,4),%eax  /* move *(ecx+edx+4) to eax */
	pushl	%eax
        decl	%edx
        jns	.L31
.L29:
	movl	12(%ebp),%eax /* obj */
        pushl	%eax
        movl	16(%ebp),%ecx /* function */
	call	*%ecx

	/* cleanup stack */	
        movl	20(%ebp),%edx	/* params_size */
	shll    $2,%edx
	addl	%edx,%esp	
	addl	$4,%esp
	RESTORE_CALLER_REGISTERS
	leave
	ret
.global callnative_special_end	
callnative_special_end:		
	.size callnative_special, callnative_special_end-callnative_special
	nop

	
	
	.text
	.align 16
/* Prototype:
 * word_t callnative_special_portal(word_t *params, object_t *obj, char *function, int params_size);
*/
	.type	callnative_special_portal,@function
	.globl	callnative_special_portal
callnative_special_portal:
	pushl	%ebp
	movl	%esp,%ebp
	SAVE_CALLER_REGISTERS
	
	movl	8(%ebp),%ecx	/* params */
        movl	20(%ebp),%edx	/* params_size */
        decl	%edx 
        js	.L129		/* jump if params_size == 0 */
.L131:
        movl	(%ecx,%edx,4),%eax  /* move *(ecx+edx+4) to eax */
	pushl	%eax
        decl	%edx
        jns	.L131
.L129:
	movl	12(%ebp),%eax /* obj */
        pushl	%eax
        movl	16(%ebp),%ecx /* function */
	call	*%ecx

	/* cleanup stack */	
        movl	20(%ebp),%edx	/* params_size */
	shll    $2,%edx
	addl	%edx,%esp	
	addl	$4,%esp
	RESTORE_CALLER_REGISTERS
	leave
	ret
.global callnative_special_portal_end	
callnative_special_portal_end:		
	.size callnative_special_portal, callnative_special_portal_end-callnative_special_portal
	nop

			
/* Prototype:
 * word_t callnative_static(word_t *params, char *function, int params_size);
 */
	.type	callnative_static,@function
	.globl	callnative_static
callnative_static:
	pushl	%ebp
	movl	%esp,%ebp
	SAVE_CALLER_REGISTERS
        movl	8(%ebp),%ecx /* params */
        movl	16(%ebp),%edx  /* params_size */
        decl	%edx 
        js	.L12  /* jump if params_size == 0 */
.L11:
        movl	(%ecx,%edx,4),%eax  /* move *(ecx+edx) to eax */
	pushl	%eax
        decl	%edx
        jns	.L11
.L12:
        movl	12(%ebp),%ecx /* function */
        call	*%ecx

	/* cleanup stack */	
        movl	16(%ebp),%edx	/* params_size */
	shll    $2,%edx
	addl	%edx,%esp	
	addl	$4,%esp
	RESTORE_CALLER_REGISTERS
	leave
	ret
.global callnative_static_end
callnative_static_end:		
	.size callnative_static, callnative_static_end-callnative_static
	nop

/* Prototype:
 *	word_t callnative_irq(object_t *obj, char *function);
 */
	.type	callnative_irq,@function
	.globl	callnative_irq
callnative_irq:
	pushl	%ebp
	movl	%esp,%ebp
	SAVE_CALLER_REGISTERS
	movl	8(%ebp),%eax	/* obj */
        pushl	%eax
        movl	12(%ebp),%ecx	/* function */
        call	*%ecx

	/* cleanup stack */	
	addl	$4,%esp
	RESTORE_CALLER_REGISTERS
	leave
	ret
.Lcallnative_irq_end:		
	.size callnative_irq, .Lcallnative_irq_end-callnative_irq

/* Prototype:
 * void callnative_handler(word_t *ebp, word_t *esp, char *handler);
 */
	.type	callnative_handler,@function
	.globl	callnative_handler
callnative_handler:
	movl    4(%esp),%ebp
	movl    12(%esp),%eax
	movl    8(%esp),%esp
	jmp     *%eax
.Lcallnative_handler_end:		
	.size callnative_handler, .Lcallnative_handler_end-callnative_handler
		
