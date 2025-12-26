// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025, Kuan-Wei Chiu <visitorckw@gmail.com>
 */

#include <asm-generic/sections.h>
#include <asm/bootinfo.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <config.h>
#include <dm/platdata.h>
#include <goldfish_tty.h>
#include <init.h>
#include <linux/errno.h>
#include <serial.h>

DECLARE_GLOBAL_DATA_PTR;

/* QEMU Virt Machine Hardware Map */
#define VIRT_GF_TTY_MMIO_BASE  0xff008000
#define VIRT_CTRL_MMIO_BASE    0xff009000
#define VIRT_CTRL_RESET        0x01

int board_early_init_f(void)
{
	return 0;
}

int checkboard(void)
{
	puts("Board: QEMU m68k virt\n");
	return 0;
}

int dram_init(void)
{
	struct bi_record *record;
	ulong addr;

	/* Default: 16MB */
	gd->ram_size = 0x01000000;

	/* QEMU places bootinfo after _end, aligned to 2 bytes */
	addr = (ulong)&_end;
	if (addr & 1)
		addr++;

	record = (struct bi_record *)addr;

	if (record->tag != BI_MACHTYPE)
		return 0;

	while (record->tag != BI_LAST) {
		if (record->tag == BI_MEMCHUNK) {
			gd->ram_size = record->data[1];
			break;
		}
		record = (struct bi_record *)((ulong)record + record->size);
	}

	return 0;
}

void reset_cpu(unsigned long addr)
{
	writel(VIRT_CTRL_RESET, VIRT_CTRL_MMIO_BASE);
	while (1)
		;
}

int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	reset_cpu(0);
	return 0;
}

static const struct goldfish_tty_plat serial_plat = {
	.base = (void __iomem *)VIRT_GF_TTY_MMIO_BASE,
};

U_BOOT_DRVINFO(goldfish_serial) = {
	.name = "serial_goldfish",
	.plat = &serial_plat,
};
