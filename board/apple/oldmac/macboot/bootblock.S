/*
 * oldmac boot block (Phase A/B) - u-boot loader with Mac bootinfo
 *
 * Loaded and entered by our driver (first stage) at bbEntry (offset 2) with
 * d6 = SCSI target id and the ROM MMU active.  We:
 *   1. SCSI-read u-boot.bin off the disk (ROM SCSI Manager) into a high buffer,
 *   2. while the ROM/Toolbox is still alive, query the machine for its model
 *      (Gestalt), RAM size (MemTop) and framebuffer (main GDevice) and build a
 *      Macintosh bootinfo record list in a scratch buffer,
 *   3. mask interrupts, tear down the 68040 MMU + caches (flat physical, as
 *      u-boot expects when loaded by -kernel),
 *   4. zero the RAM u-boot needs clean, copy u-boot to its link address 0, place
 *      the bootinfo right after u-boot (_end), and
 *   5. jump to u-boot _start.
 *
 * Build-specific addresses (UBOOT_SIZE = __image_copy_end, UBOOT_ENTRY = _start,
 * BOOTINFO_DEST = _end) are passed with --defsym from nm(u-boot); defaults below.
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
.equ _Gestalt,      0xA1AD
.equ READ_10,       0x28

.equ MACH_MAC,          3
.equ gestaltMachineType, 0x6d616368	/* 'mach' */
.equ MemTop,        0x0108		/* low-mem: top of RAM */
.equ MainDevice,    0x08a4		/* low-mem: main GDevice handle */

.equ UBOOT_LBA,     128
.ifndef UBOOT_BLOCKS
.equ UBOOT_BLOCKS,  96			/* CD blocks of u-boot.bin to read */
.endif
.equ READBUF,       0x00600000
.equ BOOTINFO_BUF,  0x00501000		/* scratch, in the preserved gap */

.ifndef UBOOT_SIZE
.equ UBOOT_SIZE,    0x00028218		/* __image_copy_end */
.endif
.ifndef UBOOT_ENTRY
.equ UBOOT_ENTRY,   0x00000400		/* _start */
.endif
.ifndef BOOTINFO_DEST
.equ BOOTINFO_DEST, 0x0002cb30		/* _end */
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

/* emit one bootinfo record: tag, size (bytes incl. header), then long data.
 * uses %a4 as the running write pointer. */
.macro BIREC tag, longval
	move.w	#\tag, (%a4)+
	move.w	#8, (%a4)+
	move.l	\longval, (%a4)+
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
	/* transparent-translate the IO region while the ROM MMU is on */
	move.l	#0x500fe060, %d1
	.long	0x4e7b1006		/* movec %d1,%dtt0 */
	nop

	/* announce ourselves */
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
	/* SCSI-read u-boot.bin into the staging buffer (ROM SCSI Manager) */
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

	/* ---- build a Mac bootinfo record list in BOOTINFO_BUF ----
	 * (ROM/Toolbox still alive; Gestalt preserves a3-a6/d3-d7). */
	movea.l	#BOOTINFO_BUF, %a4

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
	bne.s	3f
	move.l	%a0, %d3
3:	BIREC	0x8000, %d3		/* BI_MAC_MODEL */

	/* framebuffer: MainDevice -> GDevice -> PixMap */
	movea.l	MainDevice, %a0		/* GDHandle */
	move.l	%a0, %d0
	beq.s	4f
	movea.l	(%a0), %a0		/* GDevice* */
	movea.l	22(%a0), %a1		/* gdPMap (PixMapHandle) */
	move.l	%a1, %d0
	beq.s	4f
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
4:
	/* BI_LAST */
	move.w	#0x0000, (%a4)+
	move.w	#4, (%a4)+

	/* d5 = bootinfo byte count */
	move.l	%a4, %d5
	sub.l	#BOOTINFO_BUF, %d5

	/* Mask interrupts before tearing down the ROM environment. */
	move.w	#0x2700, %sr

	/* Tear down the ROM MMU + caches -> flat physical. */
	moveq	#0, %d0
	movec	%d0, %tc
	movec	%d0, %itt0
	movec	%d0, %itt1
	movec	%d0, %dtt0
	movec	%d0, %dtt1
	.short	0xf478			/* cpusha bc */
	nop

	/* zero BSS + init-stack + gd/boardf region */
	movea.l	#UBOOT_SIZE, %a1
	move.l	#(0x00500000-UBOOT_SIZE)/4, %d0
5:	clr.l	(%a1)+
	subq.l	#1, %d0
	bne.s	5b

	/* zero high relocation region */
	movea.l	#0x00700000, %a1
	move.l	#(0x01000000-0x00700000)/4, %d0
6:	clr.l	(%a1)+
	subq.l	#1, %d0
	bne.s	6b

	/* copy the u-boot image to its link address 0 */
	movea.l	#READBUF, %a0
	movea.l	#0, %a1
	move.l	#UBOOT_SIZE/4, %d0
7:	move.l	(%a0)+, (%a1)+
	subq.l	#1, %d0
	bne.s	7b

	/* place the bootinfo right after u-boot (_end) */
	movea.l	#BOOTINFO_BUF, %a0
	movea.l	#BOOTINFO_DEST, %a1
	add.l	#3, %d5
	lsr.l	#2, %d5			/* longs, rounded up */
8:	move.l	(%a0)+, (%a1)+
	subq.l	#1, %d5
	bne.s	8b

	.short	0xf478			/* flush caches after writing code */
	nop

	/* enter u-boot */
	movea.l	#UBOOT_ENTRY, %a0
	jmp	(%a0)

	.align 2
CDB:	.byte	READ_10
	.byte	0
	.long	UBOOT_LBA
	.byte	0
	.short	UBOOT_BLOCKS
	.byte	0

.equ op_inc,  1
.equ op_stop, 7
	.align 2
TIB:	.short	op_inc
	.long	READBUF
	.long	UBOOT_BLOCKS * 2048
	.short	op_stop
	.long	0
	.long	0

stat:		.short	0
message:	.short	0
bbmsg:		.asciz	"\r\noldmac: loading u-boot...\r\n"

	.org	1024, 0xda
end:
