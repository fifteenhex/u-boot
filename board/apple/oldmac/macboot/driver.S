/*
 * oldmac minimal Mac disk/CD driver (Phase A gate) - loader build
 *
 * Entered by the F1ACAD13 ROM via jsr (see reverse-engineered contract in the
 * project notes): a0 = Apple_HFS partition-map entry, d5 = SCSI target id.
 *
 * Job: read the HFS partition's boot blocks (first sector of the partition) off
 * the disk using the ROM SCSI Manager (_SCSIDispatch, per EMILE first/scsi.i) and
 * jump into them at offset 2 (bbEntry), exactly as the ROM would for a real boot
 * volume.  This makes our driver the first-stage loader.
 *
 * Runs under the ROM MMU, so we first set 68040 DTT0 to map the 0x5000_0000 IO
 * region transparently (needed for both our SCSI/SCC and the loaded boot block).
 */

	.cpu 68040
	.text
	.org 0

.equ _SCSIDispatch, 0xA815
.equ _SCSIGet,      1
.equ _SCSISelect,   2
.equ _SCSICmd,      3
.equ _SCSIComplete, 4
.equ _SCSIRead,     5
.equ BOOTBUF,       0x00500000	/* where to load the boot block (above u-boot) */
.equ READ_10,       0x28

.macro SCSIDispatch sel
	move.w	#\sel, -(%sp)
	dc.w	_SCSIDispatch
	move.w	(%sp)+, %d0
.endm

driver_base:
	/* transparent-translate the IO region (logical==physical, non-cachable) */
	move.l	#0x500fe060, %d1
	.long	0x4e7b1006		/* movec %d1,%dtt0 */
	nop

	/* d5 = SCSI target, a0 = HFS partition entry. Save the target. */
	move.w	%d5, %d6		/* d6 = target id (SCSI mgr preserves d3-d7) */

	/* patch READ(10) CDB with the HFS partition's start block (pmPyPartStart) */
	move.l	8(%a0), %d0		/* pmPyPartStart */
	lea	CDB(%pc), %a1
	move.l	%d0, 2(%a1)		/* CDB[2..5] = LBA */

	/* SCSI read sequence (EMILE first/scsi.i) */
	clr.w	-(%sp)
	SCSIDispatch _SCSIGet

	clr.w	-(%sp)
	move.w	%d6, -(%sp)		/* target id */
	SCSIDispatch _SCSISelect

	clr.w	-(%sp)
	pea	CDB(%pc)
	move.w	#10, -(%sp)
	SCSIDispatch _SCSICmd

	clr.w	-(%sp)
	pea	TIB(%pc)
	SCSIDispatch _SCSIRead

	clr.w	-(%sp)
	pea	stat(%pc)
	pea	message(%pc)
	move.l	#300, -(%sp)
	SCSIDispatch _SCSIComplete

	/* jump into the loaded boot block at bbEntry (offset 2) */
	movea.l	#BOOTBUF + 2, %a0
	jmp	(%a0)

	/* -------- data -------- */
	.align 2
CDB:	.byte	READ_10
	.byte	0
	.long	0			/* LBA (patched) */
	.byte	0
	.short	1			/* transfer length = 1 block */
	.byte	0

.equ op_inc,  1
.equ op_stop, 7
	.align 2
TIB:	.short	op_inc
	.long	BOOTBUF			/* destination buffer */
	.long	2048			/* bytes to transfer (one CD sector) */
	.short	op_stop
	.long	0
	.long	0

stat:		.short	0
message:	.short	0

driver_end:
