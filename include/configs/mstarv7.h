/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2020 Daniel Palmer<daniel@thingy.jp>
 */

#ifndef __MSTARV7_CONFIG_H
#define __MSTARV7_CONFIG_H

#include "../../arch/arm/mach-mstar/include/mstarv7.h"

#ifdef CONFIG_MSTAR_INFINITY2M
/*
 * For some reason they put only 64KB into
 * the i2m. So if i2m is enabled only use
 * 64KB of SRAM
 */
#define MSTAR_SRAM_SZ			0x10000
#elif CONFIG_MSTAR_INFINITY1
/*
 * If i1 is selected but i2m isn't we can
 * used the 88KB that seems to be in the i1
 * chips.
 */
#define MSTAR_SRAM_SZ			0x16000
#else
/*
 * otherwise use the 128KB that seems to be
 * present in i3 and m5
 */
#define MSTAR_SRAM_SZ			0x20000
#endif

#define CONFIG_SYS_INIT_SP_ADDR		(MSTAR_SRAM + MSTAR_SRAM_SZ)
#define CONFIG_SPL_STACK		CONFIG_SYS_INIT_SP_ADDR

#define CONFIG_SYS_UBOOT_BASE		0 // for spl_nor

//#define CONFIG_SYS_SPI_U_BOOT_OFFS	0x20000

#define CONFIG_SYS_SDRAM_BASE		0x20000000



#define CONFIG_SPL_MAX_SIZE		CONFIG_SPL_SIZE_LIMIT



#define CONFIG_SPL_BSS_START_ADDR	(MSTAR_SRAM + CONFIG_SPL_MAX_SIZE)
#define CONFIG_SPL_BSS_MAX_SIZE		(MSTAR_SRAM - (CONFIG_SPL_MAX_SIZE + CONFIG_SPL_SYS_MALLOC_F_LEN))

#define CONFIG_SYS_HZ_CLOCK 6000000

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot.img"

#endif
