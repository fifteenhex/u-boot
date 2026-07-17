/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * 68040 MMU helpers for the oldmac SPL.
 *
 * The classic-Mac ROM hands U-Boot control with its MMU on, mapping some NuBus
 * slot and video windows non-identity.  U-Boot proper runs with the MMU off on
 * the 040 Macs and cannot reach those windows, so the SPL resolves the physical
 * addresses it needs here - while the ROM MMU is still on - then tears the MMU
 * down so proper runs in the flat physical space it expects.
 *
 * 68040-only: ptestr and movec of the transparent-translation / MMU-status
 * registers.  Callers gate on m68k_is_68040().  SPL only.
 */

	.cpu 68040
	.text

/*
 * ulong oldmac_mmu_xlate(ulong logical)
 *   Return the physical address the ROM's page tables map @logical to, or 0 if
 *   the page is not resident or the probe faulted.  Must run with the MMU on.
 *
 *   ptestr consults the transparent-translation registers as well as the page
 *   tables, so clear DTT0/DTT1 across the probe (and restore them after) to
 *   force a real table search - otherwise an address covered by DTT0's IO
 *   window would come back identity-mapped.  Only ptestr and a status read
 *   happen while the TTRs are off, so no memory or IO is touched meanwhile.
 */
	.global oldmac_mmu_xlate
oldmac_mmu_xlate:
	move.w	%sr, %d0
	move.l	%d0, -(%sp)		/* save + raise the interrupt mask */
	move.w	#0x2700, %sr
	movec	%dtt0, %d1
	move.l	%d1, -(%sp)
	movec	%dtt1, %d1
	move.l	%d1, -(%sp)
	moveq	#0, %d1
	movec	%d1, %dtt0		/* force ptestr through the page tables */
	movec	%d1, %dtt1
	move.l	16(%sp), %a0		/* logical: 4 (arg) + 12 (three saved longs) */
	ptestr	(%a0)
	movec	%mmusr, %d0
	move.l	(%sp)+, %d1
	movec	%d1, %dtt1		/* restore the TTRs before touching anything */
	move.l	(%sp)+, %d1
	movec	%d1, %dtt0
	move.l	(%sp)+, %d1
	move.w	%d1, %sr
	/* MMUSR: bit 0 = R(esident), bit 11 = B(us error), bits [31:12] = phys page. */
	btst	#0, %d0
	beq.s	1f			/* not resident -> 0 */
	btst	#11, %d0
	bne.s	1f			/* table-search bus error -> 0 */
	move.l	%d0, %d1
	andi.l	#0xfffff000, %d1	/* physical page frame */
	move.l	%a0, %d0
	andi.l	#0x00000fff, %d0	/* offset within the page (logical == physical) */
	or.l	%d1, %d0
	rts
1:	moveq	#0, %d0
	rts

/*
 * void oldmac_mmu_disable(void)
 *   Tear the ROM MMU down for U-Boot proper: clear TC (paging off) and
 *   ITT0/ITT1/DTT1, but keep DTT0 so the 0x5xxxxxxx IO region stays identity +
 *   cache-inhibited, exactly as the boot block's original 040 teardown left it.
 *   Flush the caches afterwards.  Interrupts stay masked, as they were across
 *   the boot block teardown and into U-Boot.
 */
	.global oldmac_mmu_disable
oldmac_mmu_disable:
	move.w	#0x2700, %sr
	moveq	#0, %d0
	movec	%d0, %tc		/* paging off */
	movec	%d0, %itt0
	movec	%d0, %itt1
	movec	%d0, %dtt1
	cpusha	%bc			/* flush + invalidate both caches */
	nop
	rts
