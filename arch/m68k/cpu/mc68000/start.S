/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm-offsets.h>
#include <config.h>
#include <asm/cache.h>

.text
_vectors:
_initial_sp:
	/*
	 * For real hardware we need the bottom of the image to have
	 * space for the vectors, on QEMU we end up entering the ELF
	 * at 0 and trying to run this.. we don't use the initial
	 * sp either way so use it to jump over the vector table.
	 */
	jmp		0x400			/* Initial stack pointer */
_initial_pc:
	.long (CFG_SYS_FLASH_BASE + _start)	/* Start of program Code */
	.balign 0x400

.text
.globl _start
_start:
	/* put relocation table address to a5 */
	move.l  #__got_start, %a5

	/* setup stack initially on top of the init ram */
	move.l	#(CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_RAM_SIZE), %sp

	/*
	 * if configured, malloc_f arena will be reserved first,
	 * then (and always) gd struct space will be reserved
	 */
	move.l	%sp, -(%sp)
	bsr		board_init_f_alloc_reserve

	/* update stack and frame-pointers */
	move.l 	%d0, %sp
	move.l	%sp, %fp

	/* initialize reserved area */
	move.l	%d0, -(%sp)
	move.l	#board_init_f_init_reserve, %a1
	jsr		(%a1)

	/* run low-level CPU init code (from flash) */
	bsr		cpu_init_f
	clr.l	%sp@-

	/* run low-level board init code (from flash) */
	move.l	#board_init_f, %a1
	jsr		(%a1)

	/* board_init_f() does not return */

#define SAVE_ALL					\
	/* space for 15 regs */			\
	subl	#60,%sp;				\
	moveml	%d0-%d7/%a0-%a6,%sp@;

#define RESTORE_ALL					\
	moveml	%sp@,%d0-%d7/%a0-%a6;	\
	/* space for 15 regs */			\
	addl	#60,%sp; \
	rte;

/* exception code */
.globl _fault
_fault:
	bra	_fault

 .macro int_sled num group func
.globl _int_sled_\num
_int_sled_\num:
	SAVE_ALL
	/* push the location of the frame */
	movel	%sp,%sp@-
	/* push the group on the stack */
	movel	#\group,%sp@-
	/* push vector number on stack */
	movel	#\num,%sp@-
	jbsr	\func
	/* pop stack */
	addl	#12,%sp
	RESTORE_ALL
.endm

int_sled 4 1 exc_handler
int_sled 5 2 exc_handler
int_sled 8 1 exc_handler

.align 4
