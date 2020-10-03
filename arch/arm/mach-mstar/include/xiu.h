/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SOC_MSTAR_XIU_H_
#define _SOC_MSTAR_XIU_H_

static inline u32 xiu_readl(void* regs, unsigned reg)
{
	return readl(regs + (reg * 2));
}

static inline void xiu_writel(void *regs, unsigned reg, u32 value){
        writel(value, regs + (reg * 2));
}

#endif
