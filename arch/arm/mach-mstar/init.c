// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 Daniel Palmer<daniel@thingy.jp>
 */

#include <init.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <linux/err.h>

DECLARE_GLOBAL_DATA_PTR;

void s_init(void)
{
	/*
	 * Fix up the aux control register;
	 * We need smp mode on to use the caches.
	 */
	asm volatile(
		"mrc p15, 0, r0, c1, c0, 1\n"
		"orr r0, r0, #1 << 6\n"
		"mcr p15, 0, r0, c1, c0, 1\n"
		::: "r0");
}

static const void *get_memory_reg_prop(const void *fdt, int *lenp)
{
	int offset;

	offset = fdt_path_offset(fdt, "/memory");
	if (offset < 0)
		return NULL;

	return fdt_getprop(fdt, offset, "reg", lenp);
}

int dram_init(void)
{
	/* copied from mach-uniphier/dram_init.c */
	const void *fdt = gd->fdt_blob;
	const fdt32_t *val;
	int ac, sc, len;

	ac = fdt_address_cells(fdt, 0);
	sc = fdt_size_cells(fdt, 0);
	if (ac < 0 || sc < 1 || sc > 2) {
		printf("invalid address/size cells\n");
		return -EINVAL;
	}

	val = get_memory_reg_prop(fdt, &len);
	if (len / sizeof(*val) < ac + sc)
		return -EINVAL;

	val += ac;

	gd->ram_size = fdtdec_get_number(val, sc);

	debug("DRAM size = %08lx\n", (unsigned long)gd->ram_size);

	return 0;
}
