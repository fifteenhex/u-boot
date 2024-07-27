// SPDX-License-Identifier: GPL-2.0+

#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/bitops.h>

#include <stdio.h>

#if defined(CONFIG_MC68030)
#define MC68030_CACR_EI	BIT(0)
#define MC68030_CACR_ED	BIT(8)

static inline u32 mc68030_get_cacr(void)
{
	u32 cacr;

	asm volatile ("movec %%cacr,%0" : "=r" (cacr));

	return cacr;
}

static inline void mc68030_set_cacr(u32 cacr)
{
	asm volatile ("movec %0,%%cacr" : : "r" (cacr));
}

void icache_enable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_EI);
}

void icache_disable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_EI);
}

int icache_status(void)
{
	return (mc68030_get_cacr() & MC68030_CACR_EI) ? 1 : 0;
}

void dcache_enable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_ED);
}

void dcache_disable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_ED);
}

int dcache_status(void)
{
	u32 vbr;


	asm volatile ("movec %%vbr,%0" : "=r" (vbr));

	printf("vbr: 0x%08x\n", vbr);

	return (mc68030_get_cacr() & MC68030_CACR_ED) ? 1 : 0;
}


#else
void icache_enable(void)
{
}

void icache_disable(void)
{
}

int icache_status(void)
{
	return 0;
}

void dcache_enable(void)
{
}

void dcache_disable(void)
{
}


int dcache_status(void)
{
	return 0;
}
#endif

void flush_cache(ulong start_addr, ulong size)
{
}

void icache_invalid(void)
{
}

void dcache_invalid(void)
{
}

__weak void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}
__weak void flush_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}
