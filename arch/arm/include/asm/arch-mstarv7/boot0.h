/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Second stage (after bootrom) and Third stage (After IPL)
 * header for MStar/SigmaStar Arm v7 SoCs.
 *
 * Copyright (c) 2020 Daniel Palmer <daniel@thingy.jp>.
 */

/* The first 4 bytes should be an instruction */
	b reset

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

_start:
	ARM_VECTORS
