/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2002 - 2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#ifndef	__ASM_GBL_DATA_H
#define __ASM_GBL_DATA_H

#include <asm/u-boot.h>

/* Architecture-specific global data */
struct arch_global_data {
#ifdef CONFIG_SYS_I2C_FSL
	unsigned long	i2c1_clk;
	unsigned long	i2c2_clk;
#endif
#ifdef CONFIG_EXTRA_CLOCK
	unsigned long inp_clk;
	unsigned long vco_clk;
	unsigned long flb_clk;
#endif
#ifdef CONFIG_MCF5441x
	unsigned long sdhc_clk;
#endif
#if defined(CONFIG_FSL_ESDHC)
	unsigned long sdhc_per_clk;
#endif
	/** @pci_clk: PCI clock rate in Hz */
	unsigned long pci_clk;
};

#include <asm-generic/global_data.h>

#if defined(LTO_ENABLE)
/* If LTO is enabled we have to hide d7 to avoid multiple symbol declarations */
#define DECLARE_GLOBAL_DATA_PTR
#define gd	get_gd()

static inline gd_t *get_gd(void)
{
	gd_t *gd_ptr;

	__asm__ volatile("move.l %%d7, %0\n" : "=r" (gd_ptr));

	return gd_ptr;
}
#else
#define DECLARE_GLOBAL_DATA_PTR     register gd_t *gd asm ("d7")
#endif
static inline void arch_setup_gd(gd_t *new_gd)
{
	__asm__ volatile("move.l %0, %%d7\n" : : "r" (new_gd));
}

#endif /* __ASM_GBL_DATA_H */
