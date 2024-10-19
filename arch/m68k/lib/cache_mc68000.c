// SPDX-License-Identifier: GPL-2.0+

#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/bitops.h>

#include <stdio.h>

#if defined(CONFIG_MC68030) || defined(CONFIG_MC68040) || defined(CONFIG_TARGET_QEMU)
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

void icache_invalid(void)
{
}

void dcache_invalid(void)
{
}
#endif

__weak void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}
__weak void flush_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}

void flush_cache(ulong start_addr, ulong size)
{
}
