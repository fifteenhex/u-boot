/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Second stage (after bootrom) and Third stage (After IPL)
 * header for MStar/SigmaStar Arm v7 SoCs.
 *
 * Copyright (c) 2020 Daniel Palmer <daniel@thingy.jp>.
 */

/* The first 4 bytes should be an instruction */
	b __ipl_init

#ifdef CONFIG_MSTAR_IPL
	/* this is needed for the IPL to jump into our image */
	.ascii	"IPLC"
#else
	/* this is needed for the bootrom to jump into our image */
	.ascii	"IPL_"
#endif
	/* this is the size of the image to load */
	.2byte	0x0000

	/* this seems to be the chip id */
	.byte	0x0

	/* this is something to do with authentication */
	.byte	0x0

	/* this is a checksum, doesn't always need to be right */
	.long	0x0000

__ipl_init:
#ifdef CONFIG_MSTAR_BANG_BOOT0
	// output a bang on the console so we know we're alive
	ldr	r0, =0x1f221000
	ldr	r1, =0x21
	strb	r1, [r0]
#endif

#ifdef CONFIG_MSTAR_ENABLE_JTAG_BOOT0
#ifdef CONFIG_MSTAR_PIONEER3
	// this enables JTAG on sr_io{0..3} before doing anything
	// so we can debug the SPL
	ldr	r0, =0x1f207980
	ldr	r1, =0x200
	str	r1, [r0]
#else
	// this enables JTAG on spi0 before doing anything
	// so we can debug the SPL
	ldr	r0, =0x1f203c3c
	ldr	r1, =0x2
	str	r1, [r0]
#endif
#endif

#ifdef CONFIG_MSTAR_BREAK_BOOT0
	// this can be used to stop the cpu before uboot crashes
	bkpt
#endif

// what does this do??
	ldr	r0, =0x1f2078c4
	ldr	r1, =0x10
	str	r1, [r0]
	b	reset

_start:
	ARM_VECTORS
