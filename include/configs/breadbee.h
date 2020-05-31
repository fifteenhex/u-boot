/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_SYS_LOAD_ADDR 0x22000000
#define CONFIG_SYS_MALLOC_LEN (1024 * 128)
#define CONFIG_SYS_INIT_SP_ADDR 0xA000FFFC
#define CONFIG_SYS_UBOOT_BASE 0 // for spl_nor

#define CONFIG_SYS_SPI_U_BOOT_OFFS 0x20000

#define CONFIG_SYS_SDRAM_BASE	0x20000000

#define MSTAR_SRAM		0xa0000000

#ifdef CONFIG_MSTAR_INFINITY1
#define MSTAR_SRAM_SZ			0x16000 // If i1 is selected there is only 88KB
#else
#define MSTAR_SRAM_SZ			0x20000 // i3+ seem to have 128KB
#endif

#define CONFIG_SPL_MAX_SIZE		0xa000	// 40KB seems to be the biggest we can load
#define CONFIG_SPL_STACK		(MSTAR_SRAM + MSTAR_SRAM_SZ)

#define CONFIG_SYS_HZ_CLOCK 6000000
#define CONFIG_EXTRA_ENV_SETTINGS "loadaddr=0x22000000\0"\
				  "bb_noroff_fit=0x80000\0"\
				  "bb_norsz_fit=0x300000\0"\
				  "bb_noroff_rescue=0xd00000\0"\
				  "bb_norsz_rescue=0x300000\0"\
				  "bootcmd=sf probe; sf read ${loadaddr} ${bb_noroff_fit} ${bb_norsz_fit}; bootm ${loadaddr}#${bb_boardtype}${bb_config}\0"\
				  "bb_boot_failsafe=sf probe; if sf read ${loadaddr} ${bb_noroff_fit} ${bb_norsz_fit}; then bootm ${loadaddr}#${bb_boardtype}; fi\0"\
				  "bb_boot_rescue=sf probe; if sf read ${loadaddr} ${bb_noroff_rescue} ${bb_norsz_rescue}; then bootm ${loadaddr}#${bb_boardtype}; fi\0"

#define CONFIG_ENV_SIZE			0x2000
#define CONFIG_ENV_OFFSET		0x2000 // 8KB into the rom partition, just in case we need to put something at the start
#define CONFIG_ENV_SECT_SIZE	4096

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot.img"

// reduce the max cluster size so we can alloc the bits needed for FAT in the SPL
#define CONFIG_FS_FAT_MAX_CLUSTSIZE 4096

#endif
