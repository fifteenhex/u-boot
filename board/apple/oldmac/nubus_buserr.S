/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Bus-error-safe probing helpers for NuBus slot enumeration.
 *
 * Empty NuBus slots have no memory behind them, so touching a slot's
 * declaration-ROM area raises a bus error (access fault, vector 2).  Classic
 * Mac System software copes with this the same way: it installs a temporary
 * bus-error handler while probing.  We do likewise.
 *
 * nubus_read_safe() records the current stack pointer and a recovery label,
 * then performs the risky read.  If the access faults, the handler restores the
 * saved stack pointer and longjmps back to the recovery label with a non-zero
 * result, discarding the (68040 format $7) exception frame entirely so the
 * awkward instruction-restart semantics never come into play.
 */

	.cpu 68040
	.text

/* void nubus_buserr_begin(void): save VBR, point it at our own vector table
 * whose bus-error/address-error entries reach our handler. */
	.global nubus_buserr_begin
nubus_buserr_begin:
	movec	%vbr, %d0
	move.l	%d0, saved_vbr
	lea	nb_vectors(%pc), %a0
	lea	nubus_buserr_handler(%pc), %a1
	move.l	%a1, 8(%a0)		/* vector 2: bus error (access fault) */
	move.l	%a1, 12(%a0)		/* vector 3: address error */
	move.l	%a0, %d0
	movec	%d0, %vbr
	rts

/* void nubus_buserr_end(void): restore the previous VBR. */
	.global nubus_buserr_end
nubus_buserr_end:
	move.l	saved_vbr(%pc), %d0
	movec	%d0, %vbr
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
	move.b	(%a0), %d0		/* the risky access */
	move.b	%d0, (%a1)
	moveq	#0, %d0
	rts
.Lrecover:
	moveq	#-1, %d0
	rts

/* Bus-error handler: flag the fault, unwind to the saved SP, and jump back to
 * the recovery label.  We deliberately abandon the exception frame. */
nubus_buserr_handler:
	move.l	#1, nb_fault
	move.l	nb_rec_sp(%pc), %sp
	move.l	nb_rec_pc(%pc), %a0
	jmp	(%a0)

	.bss
	.balign 16
nb_vectors:	.space 64		/* vectors 0..15, only 2/3 populated */
saved_vbr:	.space 4
nb_fault:	.space 4
nb_rec_sp:	.space 4
nb_rec_pc:	.space 4
