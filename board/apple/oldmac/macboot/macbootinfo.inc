/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Shared boot-block helper: query the ROM/Toolbox for the machine's model,
 * RAM size and framebuffer and build a Macintosh bootinfo record list at a
 * fixed physical address.  Both boot blocks (direct and SPL) emit it here, and
 * U-Boot proper reads it from the same place, so the ROM-provided values reach
 * U-Boot whether it is entered straight from the boot block or via the SPL.
 *
 * The address lives in the 0x500000..0x700000 RAM gap that both boot blocks
 * deliberately preserve.  Keep it in sync with MAC_BOOTINFO_ADDR in
 * arch/m68k/include/asm/bootinfo.h.
 *
 * build_bootinfo must run while the ROM/Toolbox is still alive (before the MMU
 * and caches are torn down).
 */

.equ _Gestalt,      0xA1AD
.equ MACH_MAC,          3
.equ gestaltMachineType, 0x6d616368	/* 'mach' */
.equ MemTop,        0x0108		/* low-mem: top of RAM */
.equ MainDevice,    0x08a4		/* low-mem: main GDevice handle */
.equ SCCRd,         0x01d8		/* low-mem: SCC read base (per model) */
.equ MAC_BOOTINFO,  0x00501000		/* fixed, in the preserved gap */

/* emit one bootinfo record: tag, size (bytes incl. header), long data.
 * uses %a4 as the running write pointer. */
.macro BIREC tag, longval
	move.w	#\tag, (%a4)+
	move.w	#8, (%a4)+
	move.l	\longval, (%a4)+
.endm

/* Build the record list at MAC_BOOTINFO.  Clobbers a0,a1,a4,d0,d3,d4. */
.macro build_bootinfo
	movea.l	#MAC_BOOTINFO, %a4

	move.l	#MACH_MAC, %d3
	BIREC	0x0001, %d3		/* BI_MACHTYPE */

	/* BI_MEMCHUNK: base=0, size=MemTop */
	move.w	#0x0005, (%a4)+
	move.w	#12, (%a4)+
	clr.l	(%a4)+
	move.l	MemTop, (%a4)+

	/* BI_MAC_MODEL = Gestalt(gestaltMachineType) */
	moveq	#0, %d3
	move.l	#gestaltMachineType, %d0
	dc.w	_Gestalt		/* d0=err, a0=response */
	tst.w	%d0
	bne.s	77f
	move.l	%a0, %d3
77:	BIREC	0x8000, %d3		/* BI_MAC_MODEL */

	/* framebuffer: MainDevice -> GDevice -> PixMap */
	movea.l	MainDevice, %a0		/* GDHandle */
	move.l	%a0, %d0
	beq.s	78f
	movea.l	(%a0), %a0		/* GDevice* */
	movea.l	22(%a0), %a1		/* gdPMap (PixMapHandle) */
	move.l	%a1, %d0
	beq.s	78f
	movea.l	(%a1), %a1		/* PixMap* */

	move.l	(%a1), %d3		/* baseAddr @0 */
	BIREC	0x8001, %d3		/* BI_MAC_VADDR */

	moveq	#0, %d3
	move.w	32(%a1), %d3		/* pixelSize @32 */
	BIREC	0x8002, %d3		/* BI_MAC_VDEPTH */

	moveq	#0, %d3
	move.w	4(%a1), %d3		/* rowBytes @4 */
	and.l	#0x3fff, %d3
	BIREC	0x8003, %d3		/* BI_MAC_VROW */

	moveq	#0, %d3
	move.w	10(%a1), %d3		/* bounds.bottom @10 */
	sub.w	6(%a1), %d3		/* - bounds.top @6 = height */
	swap	%d3
	move.w	12(%a1), %d4		/* bounds.right @12 */
	sub.w	8(%a1), %d4		/* - bounds.left @8 = width */
	move.w	%d4, %d3		/* d3 = (height<<16) | width */
	BIREC	0x8004, %d3		/* BI_MAC_VDIM */
78:
	/* BI_MAC_SCCBASE = SCC read base from low memory (channel A ctrl is
	 * base+2), so the serial console tracks the model without hard-coding. */
	move.l	SCCRd, %d3
	BIREC	0x8006, %d3		/* BI_MAC_SCCBASE */

	/* BI_LAST */
	move.w	#0x0000, (%a4)+
	move.w	#4, (%a4)+
.endm
