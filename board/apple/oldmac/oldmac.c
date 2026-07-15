// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Board support for classic 68k Apple Macintosh ("oldmac").
 *
 * The first supported model is the Quadra 800 (Motorola 68040), which is also
 * what QEMU's `q800` machine emulates and is used as the development/test
 * vehicle; the true target is real hardware.
 */

#include <config.h>
#include <command.h>
#include <cpu_func.h>
#include <init.h>
#include <serial.h>
#include <serial_scc.h>
#include <time.h>
#include <asm/global_data.h>
#include <dm/platdata.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/* Quadra 800 Zilog 8530 SCC, channel A (modem port = QEMU -serial) */
#define OLDMAC_SCC_CTRL_A	0x5000c022
#define OLDMAC_SCC_DATA_A	0x5000c026

static struct scc_serial_plat oldmac_scc_plat = {
	.ctrl = OLDMAC_SCC_CTRL_A,
	.data = OLDMAC_SCC_DATA_A,
};

U_BOOT_DRVINFO(oldmac_scc) = {
	.name = "serial_scc",
	.plat = &oldmac_scc_plat,
};

int dram_init(void)
{
	/* Conservative default; refined per-model / from Mac bootinfo later. */
	gd->ram_size = SZ_16M;

	return 0;
}

int checkboard(void)
{
	puts("Board: Apple Macintosh (Quadra 800)\n");

	return 0;
}

/*
 * TODO(oldmac): bring-up stubs.  Replace with a real VIA/RTC timer driver and a
 * proper machine reset.  These exist only so u-boot proper links and boots far
 * enough to validate the SCC console; the crude monotonic counter is not a real
 * timebase.
 */
unsigned long notrace timer_read_counter(void)
{
	static unsigned long t;

	return t += 1000;
}

ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}

void reset_cpu(void)
{
	while (1)
		;
}

int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	reset_cpu();

	return 0;
}
