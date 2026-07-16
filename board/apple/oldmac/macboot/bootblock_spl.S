/*
 * oldmac boot block - loads the U-Boot SPL off the CD
 *
 * Same first-stage contract as bootblock.S, but loads spl/u-boot-spl.bin to
 * CONFIG_SPL_TEXT_BASE and jumps to the SPL _start; the SPL then loads U-Boot
 * proper from a SCSI disk.  Build-specific addresses come from --defsym.
 */

	.cpu 68040
	.text
	.org 0

.equ SCC_CTL_A,     0x5000c022
.equ SCC_DAT_A,     0x5000c026
.equ _SCSIDispatch, 0xA815
.equ _SCSIGet,      1
.equ _SCSISelect,   2
.equ _SCSICmd,      3
.equ _SCSIComplete, 4
.equ _SCSIRead,     5
.equ READ_10,       0x28

.include "macbootinfo.inc"

.equ PAYLOAD_LBA,   128
.equ PAYLOAD_BLOCKS,64
.equ READBUF,       0x00600000

.ifndef SPL_SIZE
.equ SPL_SIZE,      0x00006000
.endif
.ifndef SPL_LOAD
.equ SPL_LOAD,      0x00100000
.endif
.ifndef SPL_ENTRY
.equ SPL_ENTRY,     0x00100400
.endif

.macro pString str
99900:	.byte	99901f - 99902f - 1
99902:	.string	"\str"
99901:
	.fill	16 - (99901b - 99900b), 1, 0
.endm

.macro SCSIDispatch sel
	move.w	#\sel, -(%sp)
	dc.w	_SCSIDispatch
	move.w	(%sp)+, %d0
.endm

begin:
ID:		.short	0x4C4B
Entry:		bra.w	start
Version:	.short	0x4418
PageFlags:	.short	0x0000
SysName:	pString	"oldmac"
ShellName:	pString	"Finder"
Dbg1Name:	pString	"Macsbug"
Dbg2Name:	pString	"DisAsm"
ScreenName:	pString	"StartUpScreen"
HelloName:	pString	"System"
ScrapName:	pString	"Clipboard"
CntFCBs:	.short	10
CntEvts:	.short	20
Heap128K:	.long	0x00004300
Heap256K:	.long	0x00008000
SysHeapSize:	.long	0x00020000

	.align	2
start:
	move.l	#0x500fe060, %d1
	.long	0x4e7b1006		/* movec %d1,%dtt0 */
	nop

	movea.l	#SCC_CTL_A, %a1
	tst.b	(%a1)
	move.b	#0x05, (%a1)
	move.b	#0x68, (%a1)
	movea.l	#SCC_DAT_A, %a3
	lea	bbmsg(%pc), %a2
1:	move.b	(%a2)+, %d0
	beq.s	2f
	move.b	%d0, (%a3)
	bra.s	1b
2:
	/* SCSI-read the SPL image into the staging buffer */
	clr.w	-(%sp)
	SCSIDispatch _SCSIGet
	clr.w	-(%sp)
	move.w	%d6, -(%sp)
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
	move.l	#600, -(%sp)
	SCSIDispatch _SCSIComplete

	/* Build the Mac bootinfo at MAC_BOOTINFO while the ROM is still alive,
	 * so the SPL and U-Boot proper get the real model/RAM/framebuffer. */
	build_bootinfo

	/* mask interrupts + tear down MMU/caches -> flat physical */
	move.w	#0x2700, %sr
	moveq	#0, %d0
	movec	%d0, %tc
	movec	%d0, %itt0
	movec	%d0, %itt1
	movec	%d0, %dtt0
	movec	%d0, %dtt1
	.short	0xf478			/* cpusha bc */
	nop

	/* zero the RAM the SPL and U-Boot need clean (keep boot block + READBUF) */
	movea.l	#0, %a1
	move.l	#0x00500000/4, %d0
3:	clr.l	(%a1)+
	subq.l	#1, %d0
	bne.s	3b
	movea.l	#0x00700000, %a1
	move.l	#(0x01000000-0x00700000)/4, %d0
4:	clr.l	(%a1)+
	subq.l	#1, %d0
	bne.s	4b

	/* copy the SPL to its text base */
	movea.l	#READBUF, %a0
	movea.l	#SPL_LOAD, %a1
	move.l	#SPL_SIZE/4, %d0
5:	move.l	(%a0)+, (%a1)+
	subq.l	#1, %d0
	bne.s	5b
	.short	0xf478
	nop

	movea.l	#SPL_ENTRY, %a0
	jmp	(%a0)

	.align 2
CDB:	.byte	READ_10
	.byte	0
	.long	PAYLOAD_LBA
	.byte	0
	.short	PAYLOAD_BLOCKS
	.byte	0

.equ op_inc,  1
.equ op_stop, 7
	.align 2
TIB:	.short	op_inc
	.long	READBUF
	.long	PAYLOAD_BLOCKS * 2048
	.short	op_stop
	.long	0
	.long	0

stat:		.short	0
message:	.short	0
bbmsg:		.asciz	"\r\noldmac: loading SPL...\r\n"

	.org	1024, 0xda
end:
