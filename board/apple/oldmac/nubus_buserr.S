/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Bus-error-safe probing helpers for NuBus slot enumeration.
 *
 * Empty NuBus slots have no memory behind them, so touching a slot's
 * declaration-ROM area raises a bus error (access fault, vector 2).  We install
 * a temporary handler while probing, the same way classic Mac System software
 * does; nubus_read_safe() records a recovery point, performs the risky read, and
 * on a fault the handler unwinds to the saved stack and longjmps back with a
 * non-zero result, discarding the 68040 format-$7 exception frame entirely.
 *
 * Robustness on a real 68040 (QEMU is far more forgiving):
 *  - VBR is pointed at a FULL 1 KiB copy of the live vector table with only the
 *    bus/address-error entries redirected.  A short table would send any other
 *    exception taken while probing (an interrupt, etc.) off its end, fetching a
 *    junk "vector" and branching into random code - which looked like U-Boot
 *    restarting.  With a full copy every other exception reaches its real
 *    handler unchanged.
 *  - Interrupts are masked across the whole probe so nothing perturbs our
 *    handler or stack mid-recovery.
 *  - A 'nop' after recording the recovery state forces the 68040 to retire the
 *    preceding writes before the faulting access, so the recovery pointers are
 *    committed to memory when the frame is abandoned.
 */

	.cpu 68040
	.text

/* void nubus_buserr_begin(void) */
	.global nubus_buserr_begin
nubus_buserr_begin:
	move.w	%sr, saved_sr
	move.w	#0x2700, %sr		/* mask interrupts for the probe */
	movec	%vbr, %d0
	move.l	%d0, saved_vbr
	movea.l	%d0, %a0		/* src = live vector table */
	lea	nb_vectors(%pc), %a1	/* dst = our 1 KiB copy */
	move.w	#255, %d1
1:	move.l	(%a0)+, (%a1)+		/* copy all 256 vectors */
	dbra	%d1, 1b
	lea	nb_vectors(%pc), %a1
	lea	nubus_buserr_handler(%pc), %a0
	move.l	%a0, 8(%a1)		/* vector 2: bus error (access fault) */
	move.l	%a0, 12(%a1)		/* vector 3: address error */
	move.l	%a1, %d0
	movec	%d0, %vbr
	rts

/* void nubus_buserr_end(void) */
	.global nubus_buserr_end
nubus_buserr_end:
	move.l	saved_vbr(%pc), %d0
	movec	%d0, %vbr
	move.w	saved_sr(%pc), %sr	/* restore the interrupt mask */
	rts

/* int nubus_read_safe(void *addr, unsigned char *out)
 *   returns 0 and stores the byte on success, -1 on bus error. */
	.global nubus_read_safe
nubus_read_safe:
	move.l	4(%sp), %a0		/* addr */
	move.l	8(%sp), %a1		/* out  */
	lea	.Lrecover(%pc), %a2
	move.l	%a2, nb_rec_pc
	move.l	%sp, nb_rec_sp
	clr.l	nb_fault
	nop				/* commit the recovery state to memory */
	move.b	(%a0), %d0		/* the risky access */
	move.b	%d0, (%a1)
	moveq	#0, %d0
	rts
.Lrecover:
	moveq	#-1, %d0
	rts

/* Bus-error handler: flag the fault, unwind to the saved SP, jump to recovery.
 * The faulting access is a byte read and the nop above drained earlier writes,
 * so abandoning the format-$7 frame leaves no pending state behind. */
nubus_buserr_handler:
	move.l	#1, nb_fault
	move.l	nb_rec_sp(%pc), %sp
	move.l	nb_rec_pc(%pc), %a0
	jmp	(%a0)

	.bss
	.balign 16
nb_vectors:	.space 1024		/* full 256-entry vector table */
saved_vbr:	.space 4
saved_sr:	.space 4
nb_fault:	.space 4
nb_rec_sp:	.space 4
nb_rec_pc:	.space 4
