/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Configuration for classic 68k Apple Macintosh ("oldmac").
 */

#ifndef __OLDMAC_H
#define __OLDMAC_H

/* Memory Configuration (RAM starts at 0 on the Quadra 800) */
#define CFG_SYS_SDRAM_BASE	0x00000000

/*
 * Initial Stack Pointer: place at a 4MB offset to avoid overwriting
 * U-Boot code/data.
 */
#define CFG_SYS_INIT_SP_ADDR	(CFG_SYS_SDRAM_BASE + 0x400000)

/* Send console output to both the serial port and the Mac framebuffer. */
#define CFG_EXTRA_ENV_SETTINGS		\
	"stdin=serial\0"		\
	"stdout=serial,vidconsole\0"	\
	"stderr=serial,vidconsole\0"

#endif /* __OLDMAC_H */
