/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SOC_MSTAR_RIU_H_
#define _SOC_MSTAR_RIU_H_

static inline u32 riu_readl(void* regs, unsigned reg)
{
	u32 l = readw(regs + (reg * 2));
	u32 h = readw(regs + ((reg * 2) + 4));
	return l | (h << 16);
}

static inline void riu_writel(void *regs, unsigned reg, u32 value){
        u16 l = value & 0xffff;
        u16 h = (value >> 16) & 0xffff;
        writew(l, regs + (reg * 2));
        writew(h, regs + ((reg * 2) + 4));
}

#endif
