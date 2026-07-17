/*
 * oldmac boot block - loads the U-Boot SPL off the CD
 *
 * Same first-stage contract as bootblock.S, but loads spl/u-boot-spl.bin to
 * CONFIG_SPL_TEXT_BASE and jumps to the SPL _start; the SPL then loads U-Boot
 * proper from a SCSI disk.  Build-specific addresses come from --defsym.
 */

	/* Assemble as 68030 so the 68030 MMU 'pmove' instructions are accepted;
	 * the 68040-only ops (cpusha, movec %tc/%ittN/%dttN) are hand-encoded
	 * below and are gated at runtime by the detected CPU. */
	.cpu 68030
	.text
	.org 0

/* Canonical classic-Mac SCC channel-A registers.  Every 68k Mac decodes its
 * I/O at 0x50F0xxxx; QEMU q800 also aliases 0x5000xxxx to the same registers,
 * but on real hardware 0x5000xxxx is undecoded and bus-errors, so use 0x50F0. */
.equ SCC_CTL_A,     0x50f0c022
.equ SCC_DAT_A,     0x50f0c026
.equ _SCSIDispatch, 0xA815
.equ _SCSIGet,      1
.equ _SCSISelect,   2
.equ _SCSICmd,      3
.equ _SCSIComplete, 4
.equ _SCSIRead,     5
.equ READ_10,       0x28

.include "macbootinfo.inc"

.equ PAYLOAD_LBA,   128
.ifndef PAYLOAD_BLOCKS
.equ PAYLOAD_BLOCKS,64			/* CD blocks of the SPL to read */
.endif
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

/* write SCC channel-A register \reg = \val (control port already in %a1) */
.macro sccw reg, val
	move.b	#\reg, (%a1)
	move.b	#\val, (%a1)
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
	/* Detect the CPU (Gestalt processor type: 68030 = 4, 68040 = 5).  d7
	 * carries it to the MMU/cache teardown below; nothing is live at entry. */
	moveq	#5, %d7
	move.l	#0x70726f63, %d0	/* 'proc' = gestaltProcessorType */
	dc.w	0xA1AD			/* _Gestalt -> d0 = err, a0 = response */
	tst.w	%d0
	bne.s	7f
	move.l	%a0, %d7
7:
	/* Transparent-translate the IO region (identity, cache-inhibited) and keep
	 * it through the MMU teardown so the SPL and U-Boot see cache-inhibited
	 * device registers.  68040: DTT0 via movec; 68030: TT0 via pmove. */
	cmp.w	#5, %d7
	bne.s	8f
	move.l	#0x500fe060, %d1
	.long	0x4e7b1006		/* movec %d1,%dtt0 */
	nop
	bra.s	9f
8:	lea	tt0val(%pc), %a0
	pmove	(%a0), %tt0		/* 68030 transparent translation reg 0 */
9:

	/* Fully initialise SCC channel A (modem port) for 9600 8N1 polled.  A
	 * real Z8530 has no baud clock until the BRG is programmed, so we must
	 * do this ourselves rather than rely on whatever the ROM left. */
	movea.l	0x1d8.w, %a1		/* SCCRd: ROM's per-model SCC base */
	addq.l	#2, %a1			/* -> channel A control (base+2) */
	tst.b	(%a1)			/* reset the register pointer */
	sccw	9, 0x80			/* channel A reset */
	moveq	#63, %d0
6:	tst.b	(%a1)			/* let the reset settle */
	dbra	%d0, 6b
	sccw	4, 0x44			/* x16 clock, 1 stop bit, no parity */
	sccw	3, 0xc0			/* Rx 8 bits (not yet enabled) */
	sccw	5, 0xe2			/* Tx 8 bits, DTR, RTS (not yet enabled) */
	sccw	11, 0x50		/* Rx and Tx clock from the BRG */
	sccw	12, 10			/* BRG time constant lo (9600, 3.6864MHz) */
	sccw	13, 0			/* BRG time constant hi */
	sccw	14, 1			/* BRG enable, source = RTxC (3.6864MHz baud clock) */
	sccw	3, 0xc1			/* enable Rx */
	sccw	5, 0xea			/* enable Tx */

	lea	bbmsg(%pc), %a2
1:	move.b	(%a2)+, %d0
	beq.s	2f
	bsr.w	bbputc
	bra.s	1b
2:
	/* DEBUG(temp): one line confirming CPU detection (Gestalt proc: 4=68030,
	 * 5=68040) and the ROM's per-model SCC base (SCCRd/SCCWr), e.g. on a IIsi:
	 * "bb cpu/sccrd/sccwr: 00000004 50f04000 50f04000". */
	lea	dbgmsg(%pc), %a2
14:	move.b	(%a2)+, %d0
	beq.s	15f
	bsr.w	bbputc
	bra.s	14b
15:	move.l	%d7, %d0		/* Gestalt processor type */
	bsr.w	bbputhex
	moveq	#' ', %d0
	bsr.w	bbputc
	move.l	0x1d8.w, %d0		/* SCCRd */
	bsr.w	bbputhex
	moveq	#' ', %d0
	bsr.w	bbputc
	move.l	0x1dc.w, %d0		/* SCCWr */
	bsr.w	bbputhex
	moveq	#13, %d0
	bsr.w	bbputc
	moveq	#10, %d0
	bsr.w	bbputc
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

	/*
	 * Mask interrupts and disable paging, keeping the IO region cache-inhibited
	 * (DTT0 on the 68040, TT0 on the 68030) so the SPL and U-Boot still see
	 * uncached device registers.  The 68040 movec %tc/%ittN/%dttN are
	 * hand-encoded because they cannot be named under .cpu 68030.
	 */
	move.w	#0x2700, %sr
	moveq	#'M', %d0		/* DEBUG(temp): entering MMU/cache teardown */
	bsr.w	bbputc
	cmp.w	#5, %d7
	bne.s	10f
	moveq	#0, %d0
	.long	0x4e7b0003		/* movec %d0,%tc   */
	.long	0x4e7b0004		/* movec %d0,%itt0 */
	.long	0x4e7b0005		/* movec %d0,%itt1 */
	.long	0x4e7b0007		/* movec %d0,%dtt1 */
	.short	0xf478			/* cpusha bc */
	nop
	bra.s	11f
10:	/* 68030 (Mac IIsi): the boot block runs MMU-translated with logical !=
	 * physical (confirmed: disabling paging is fatal, and EMILE translates all
	 * addresses via logical2physical).  So DON'T disable the MMU.  Dump the live
	 * MMU state for the record, then just clear the caches and leave the ROM's
	 * mapping + our TT0 (IO cache-inhibit) in place.  The SPL/U-Boot run in the
	 * ROM's logical space; IO stays identity+uncached through TT0. */
	moveq	#'t', %d0		/* DEBUG(temp): "t" then TC hex (bit31=MMU on) */
	bsr.w	bbputc
	lea	mmuscratch(%pc), %a0
	pmove	%tc, (%a0)
	move.l	(%a0), %d0
	bsr.w	bbputhex		/* TC */
	moveq	#13, %d0
	bsr.w	bbputc
	moveq	#10, %d0
	bsr.w	bbputc
	move.l	#0x0808, %d0
	movec	%d0, %cacr		/* 68030: clear I+D caches (keep MMU + TT0) */
11:
	moveq	#'D', %d0		/* DEBUG(temp): teardown survived (paging off) */
	bsr.w	bbputc
	moveq	#13, %d0
	bsr.w	bbputc
	moveq	#10, %d0
	bsr.w	bbputc

	/* zero the RAM the SPL and U-Boot need clean (keep boot block + READBUF).
	 * 68030 keeps the ROM MMU on, whose page tables live in RAM at unknown
	 * (logical) addresses, so skip the blanket zero there — U-Boot clears its
	 * own BSS. */
	cmp.w	#5, %d7
	bne.s	20f
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
20:

	/* copy the SPL to its text base */
	movea.l	#READBUF, %a0
	movea.l	#SPL_LOAD, %a1
	move.l	#SPL_SIZE/4, %d0
5:	move.l	(%a0)+, (%a1)+
	subq.l	#1, %d0
	bne.s	5b
	/* flush the freshly-copied SPL out of the caches before entering it */
	cmp.w	#5, %d7
	bne.s	12f
	.short	0xf478			/* cpusha bc (68040) */
	nop
	bra.s	13f
12:	move.l	#0x0808, %d0
	movec	%d0, %cacr		/* 68030: clear I+D caches */
13:
	/* DEBUG(temp): 'J' = SPL copied, jumping.  68030 only: the 68040 zeroed
	 * 0x1d8 (SCCRd) in the RAM-zero above, so bbputc would hang there. */
	cmp.w	#5, %d7
	beq.s	21f
	moveq	#'J', %d0
	bsr.w	bbputc
21:	movea.l	#SPL_ENTRY, %a0
	jmp	(%a0)

/* Poll TX-empty and send the char in %d0.b out SCC channel A; clobbers a1/d1.
 * Select RR0 (write 0 to the control port) each poll, since the ROM's SCSI
 * manager may leave the SCC register pointer dirty. */
bbputc:
	movea.l	0x1d8.w, %a1		/* SCCRd -> +2 = channel A control */
	addq.l	#2, %a1
7:	move.b	#0, (%a1)		/* point at RR0 */
	move.b	(%a1), %d1		/* read RR0 */
	btst	#2, %d1
	beq.s	7b
	move.b	%d0, 4(%a1)
	rts

/* DEBUG(temp): print d0.l as 8 hex digits; clobbers d0-d3/a1 (bbputc) */
bbputhex:
	move.l	%d0, %d3
	moveq	#7, %d2
7:	rol.l	#4, %d3
	move.l	%d3, %d0
	andi.l	#0x0f, %d0
	cmpi.b	#10, %d0
	blt.s	8f
	addi.b	#('a'-'0'-10), %d0
8:	addi.b	#'0', %d0
	bsr.w	bbputc
	dbra	%d2, 7b
	rts

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
dbgmsg:		.asciz	"bb[d51a0c] cpu/sccrd/sccwr: "	/* DEBUG(temp): bump token each build */
	.align 2
mmuscratch:	.space	4				/* DEBUG(temp): pmove read target */

	/* 68030 TT0: base 0x50, mask 0x0f, E, CI, ignore R/W and function code
	 * -> identity-map + cache-inhibit the whole 0x5xxxxxxx IO region. */
	.align 2
tt0val:		.long	0x500f8507
zeroval:	.long	0

	.org	1024, 0xda
end:
