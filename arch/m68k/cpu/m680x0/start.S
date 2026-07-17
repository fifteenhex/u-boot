/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Startup code for m68040
 *
 * Copyright (C) 2025, Kuan-Wei Chiu <visitorckw@gmail.com>
 */

#include <asm-offsets.h>
#include <config.h>
#include <linux/linkage.h>

.section .text

/*
 * DEBUG(temp): emit one character out the classic-Mac SCC, inline (no stack, no
 * BSS), guarded on the ROM's SCCRd low-mem global at 0x1d8.  On the Mac IIsi the
 * boot block keeps the MMU on and leaves low memory intact, so 0x1d8 holds the
 * real SCC base and these fire; on the Quadra the boot block zeroed 0x1d8, so
 * the guard skips them.  Lets us trace the SPL entry on the IIsi.  Clobbers
 * d0/d1/d2/a1 (all caller-saved / scratch here); preserves d7 (gd).
 */
.macro DBGPUTC ch
	move.l	0x1d8.w, %d1
	beq.s	.Ldbgskip\@
	movea.l	%d1, %a1
	addq.l	#2, %a1			/* SCC channel-A control */
	moveq	#\ch, %d0
.Ldbgpoll\@:
	move.b	#0, (%a1)
	move.b	(%a1), %d2
	btst	#2, %d2
	beq.s	.Ldbgpoll\@
	move.b	%d0, 4(%a1)
.Ldbgskip\@:
.endm

/*
 * Vector Table
 * m68k uses the first 1KB for the exception vector table.
 */
.balign 4
.global _vectors
_vectors:
	.long	CFG_SYS_INIT_SP_ADDR	/* 0x00: Initial SP */
	.long	_start	/* 0x04: Initial PC (Reset) */
	.long	_fault	/* 0x08: Bus Error */
	.long	_fault	/* 0x0C: Address Error */
	.long	_fault	/* 0x10: Illegal Instruction */
	.long	_fault	/* 0x14: Zero Divide */
	.long	_fault	/* 0x18: CHK */
	.long	_fault	/* 0x1C: TRAPV */
	.long	_fault	/* 0x20: Privilege */
	.long	_fault	/* 0x24: Trace */
	.long	_fault	/* 0x28: Line 1010 */
	.long	_fault	/* 0x2C: Line 1111 */
	.fill	0x400 - (.-_vectors), 1, 0

/*
 * Entry Point
 */
ENTRY(_start)
	/* Disable Interrupts */
	move.w	#0x2700, %sr

	DBGPUTC 'S'			/* DEBUG(temp): SPL _start reached */

	/* Setup initial stack pointer */
	move.l	#CFG_SYS_INIT_SP_ADDR, %sp

	/* Setup the debug uart if enabled */
#ifdef CONFIG_DEBUG_UART
	bsr.l	debug_uart_init
#endif

	/*
	 * Allocate Global Data (GD)
	 * board_init_f_alloc_reserve(top) returns the new top of stack in %d0
	 */
	move.l	%sp, -(%sp)
	bsr.l	board_init_f_alloc_reserve
	addq.l	#4, %sp

	/* Update Stack Pointer and set GD register */
	move.l	%d0, %sp
	move.l	%d0, %d7	/* %d7 is the gd register */

	/* Initialize Reserved Memory. */
	move.l	%d0, -(%sp)
	bsr.l	m68k_virt_init_reserve
	addq.l	#4, %sp

	DBGPUTC 'U'			/* DEBUG(temp): stack+gd set up, entering board_init_f */

	/* Enter board_init_f(0) */
	clr.l	-(%sp)
	bsr.l	board_init_f
	addq.l	#4, %sp

	DBGPUTC 'V'			/* DEBUG(temp): board_init_f returned */

#ifdef CONFIG_SPL_BUILD
	/*
	 * In SPL board_init_f() returns; hand off to the SPL board_init_r()
	 * which loads and starts the next stage (U-Boot proper).
	 */
	clr.l	-(%sp)
	clr.l	-(%sp)
	bsr.l	board_init_r
	addq.l	#8, %sp
#endif

	/* Should not return */
hang:
	bra.s	hang
ENDPROC(_start)

_fault:
	bra.s	_fault
