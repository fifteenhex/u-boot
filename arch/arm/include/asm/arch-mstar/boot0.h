/* SPDX-License-Identifier: GPL-2.0+ */
	b	__ipl_init
__ipl_hdr:
#ifdef CONFIG_MSTAR_IPL
// this is needed for the IPL to jump into our image
	.ascii	"IPLC"
#else
	// this is needed for the bootrom to jump into our image
	.ascii	"IPL_"
#endif
	// this is the size of the image to load
	.long	0xa000
__ipl_init:
	// this enables JTAG on spi0 before doing anything
	// so we can debug the SPL
	ldr	r0, =0x1f203c3c
	ldr	r1, =0x2
	str	r1, [r0]

	// this can be used to stop the cpu before uboot crashes
	//bkpt

	ldr	r0, =0x1f2078c4
	ldr	r1, =0x10
	str	r1, [r0]
	b	reset
	.balign 256
_start:
	ARM_VECTORS
