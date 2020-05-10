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
	.2byte	0xa000

	// this is the CID
	.byte	0x0

	// this is something to do with auth
	.byte	0x0

	// this is a checksum of some sort
	.long	0x0000

__ipl_init:
	// output a bang on the console so we know we're alive
	ldr	r0, =0x1f221000
	ldr	r1, =0x21
	strb	r1, [r0]

	// this enables JTAG on spi0 before doing anything
	// so we can debug the SPL
	//ldr	r0, =0x1f203c3c
	//ldr	r1, =0x2
	//str	r1, [r0]

	// this can be used to stop the cpu before uboot crashes
	//bkpt

	ldr	r0, =0x1f2078c4
	ldr	r1, =0x10
	str	r1, [r0]
	b	reset
	.balign 256
_start:
	ARM_VECTORS
