// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 Daniel Palmer<daniel@thingy.jp>
 */

#include <init.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

void s_init(void)
{
	/* fix up the aux control register, we need smp mode on to use the caches*/
	asm volatile(
		"mrc p15, 0, r0, c1, c0, 1\n"
		"orr r0, r0, #1 << 6\n"
		"mcr p15, 0, r0, c1, c0, 1\n"
		::: "r0");
}

int dram_init(void) {
	gd->ram_size = 0x4000000;
	return 0;
}