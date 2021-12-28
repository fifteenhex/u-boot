/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _SOC_MSTAR_RIU_H_
#define _SOC_MSTAR_RIU_H_

static inline u32 riu_readl(const void* regs, unsigned reg)
{
	u32 l = readw(regs + (reg * 2));
	u32 h = readw(regs + ((reg * 2) + 4));


	u32 ret =  l | (h << 16);

	printf("r - 0x%08x - 0x%02x: 0x%08x\n", regs, reg, ret);

	return ret;
}

static inline void riu_writel(void *regs, unsigned reg, u32 value){
        u16 l = value & 0xffff;
        u16 h = (value >> 16) & 0xffff;

        printf("w - 0x%08x - 0x%02x: 0x%08x\n", regs, reg, value);

        writew(l, regs + (reg * 2));
        writew(h, regs + ((reg * 2) + 4));
}

static inline void* riu_bank(void *regs)
{
	return (void*)(((unsigned) regs) & ~0x1ff);
}

static inline unsigned riu_reg(void *regs)
{
	return ((unsigned) regs) & 0x1ff;
}

#endif
