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
	/* Cache-inhibit the NuBus super-slot region (0xF0000000-0xFFFFFFFF) via
	 * DTT1 for the duration of the probe.  U-Boot otherwise leaves 0xF
	 * cacheable, so a byte read of a card's slow byte-wide declaration ROM
	 * pulls a 16-byte 68040 burst line-fill the ROM can't service, corrupting
	 * the bytes (they read back as 0xff on the lanes the burst couldn't
	 * refill).  base 0xF0, mask 0x0F, E=1, cache-inhibit. */
	movec	%dtt1, %d0
	move.l	%d0, saved_dtt1
	move.l	#0xf00fe060, %d0
	movec	%d0, %dtt1
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
	move.l	saved_dtt1(%pc), %d0
	movec	%d0, %dtt1		/* restore the transparent translation */
	move.w	saved_sr(%pc), %sr	/* restore the interrupt mask */
	rts

/* int nubus_read_safe(void *addr, unsigned char *out)
 *   returns 0 and stores the byte on success, -1 on bus error. */
	.global nubus_read_safe
nubus_read_safe:
	/* Use only scratch registers (d0/d1, a0/a1): a2-a6 and d2-d7 are
	 * callee-saved, and the fault path longjmps out without restoring them, so
	 * touching one would corrupt a caller's live variable (e.g. a probe loop
	 * counter) on every caught bus error. */
	lea	.Lrecover(%pc), %a1
	move.l	%a1, nb_rec_pc
	move.l	%sp, nb_rec_sp
	clr.l	nb_fault
	move.l	4(%sp), %a0		/* addr */
	move.l	8(%sp), %a1		/* out  */
	nop				/* commit the recovery state to memory */
	move.b	(%a0), %d0		/* the risky access */
	move.b	%d0, (%a1)
	moveq	#0, %d0
	rts
.Lrecover:
	moveq	#-1, %d0
	rts

/* int nubus_read16_safe(void *addr, unsigned short *out)
 *   as nubus_read_safe() but a 16-bit access, to probe strict-16-bit regions
 *   that bus-error on a byte access. */
	.global nubus_read16_safe
nubus_read16_safe:
	lea	.Lrecover16(%pc), %a1
	move.l	%a1, nb_rec_pc
	move.l	%sp, nb_rec_sp
	clr.l	nb_fault
	move.l	4(%sp), %a0
	move.l	8(%sp), %a1
	nop
	move.w	(%a0), %d0		/* 16-bit risky access */
	move.w	%d0, (%a1)
	moveq	#0, %d0
	rts
.Lrecover16:
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

/* int board_m68k_has_fpu(void)
 *   Returns 1 if the CPU has a working FPU (full 68040), 0 if not (68LC040).
 *
 *   Detects the actual chip rather than trusting the model, because many Macs
 *   that shipped with an FPU-less 68LC040 were later upgraded to a full 68040
 *   (gaining an FPU).  Probes by executing an FPU no-op with a temporary
 *   Line-1111 ("F-line") handler installed: a CPU with no FPU cannot execute
 *   F-line instructions, so fnop traps to vector 11 and we report 0; a CPU with
 *   an FPU executes it and we report 1.  Uses the same full-vector-table / VBR /
 *   masked-interrupt discipline as the bus-error probe above so any unrelated
 *   exception taken mid-probe still reaches its real handler, and unwinds out of
 *   the trap via a saved SP (an rte would just re-run the faulting fnop). */
	.global board_m68k_has_fpu
board_m68k_has_fpu:
	move.w	%sr, -(%sp)
	move.w	#0x2700, %sr		/* mask interrupts for the probe */
	movec	%vbr, %d0
	move.l	%d0, fpu_saved_vbr
	movea.l	%d0, %a0		/* src = live vector table */
	lea	fpu_vectors(%pc), %a1	/* dst = our 1 KiB copy */
	move.w	#255, %d1
1:	move.l	(%a0)+, (%a1)+		/* copy all 256 vectors */
	dbra	%d1, 1b
	lea	fpu_vectors(%pc), %a1
	lea	fpu_fline_handler(%pc), %a0
	move.l	%a0, 0x2c(%a1)		/* vector 11: Line 1111 (F-line) */
	move.l	%a1, %d0
	movec	%d0, %vbr
	lea	.Lno_fpu(%pc), %a0	/* recovery point for the F-line trap */
	move.l	%a0, fpu_rec_pc
	move.l	%sp, fpu_rec_sp
	nop				/* commit the recovery state to memory */
	fnop				/* F-line: traps to vector 11 if no FPU */
	moveq	#1, %d0			/* fell through -> FPU present */
	bra.s	.Lfpu_done
.Lno_fpu:
	moveq	#0, %d0
.Lfpu_done:
	move.l	fpu_saved_vbr(%pc), %d1
	movec	%d1, %vbr		/* restore the vector base */
	move.w	(%sp)+, %sr		/* restore the interrupt mask */
	rts

/* F-line handler: no FPU.  Abandon the exception frame and unwind to the saved
 * SP / recovery PC, exactly like the bus-error handler - we must not rte, as the
 * frame's PC is the faulting fnop and it would re-trap forever. */
fpu_fline_handler:
	move.l	fpu_rec_sp(%pc), %sp
	move.l	fpu_rec_pc(%pc), %a0
	jmp	(%a0)

	.bss
	.balign 16
nb_vectors:	.space 1024		/* full 256-entry vector table */
saved_vbr:	.space 4
saved_sr:	.space 4
saved_dtt1:	.space 4
nb_fault:	.space 4
nb_rec_sp:	.space 4
nb_rec_pc:	.space 4
fpu_vectors:	.space 1024		/* full 256-entry vector table (FPU probe) */
fpu_saved_vbr:	.space 4
fpu_rec_sp:	.space 4
fpu_rec_pc:	.space 4
