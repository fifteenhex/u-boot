/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2000 - 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 ********************************************************************
 * NOTE: This header file defines an interface to U-Boot. Including
 * this (unmodified) header file in another file is considered normal
 * use of U-Boot, and does *not* fall under the heading of "derived
 * work".
 ********************************************************************
 */

#ifndef __U_BOOT_H__
#define __U_BOOT_H__

/* Use the generic board which requires a unified bd_info */
#include <asm-generic/u-boot.h>

/* For image.h:image_check_target_arch() */
#define IH_ARCH_DEFAULT IH_ARCH_M68K

/* Optional board hook called from board_init_r() when CONFIG_BOARD_INIT=y */
int board_init(void);

/* Runtime CPU class: 1 = 68040 (default), 0 = 68030.  Boards with a 68030
 * variant (e.g. the Mac IIsi) override this so the cache/MMU paths pick the
 * right instructions.  Overridden weak default lives in cpu/m680x0/cpu.c. */
int m68k_is_68040(void);

/* FPU type for the Linux/m68k bootinfo hand-off; boards with an FPU-less part
 * (68LC040) override it.  Default FPU_68040 lives in lib/elf.c. */
u32 board_m68k_fputype(void);

#endif				/* __U_BOOT_H__ */
