// SPDX-License-Identifier: GPL-2.0+

#include <cpu_func.h>
#include <asm/cache.h>

void flush_cache(ulong start_addr, ulong size)
{
}

int icache_status(void)
{
	return 0;
}

int dcache_status(void)
{
	return 0;
}

void icache_enable(void)
{
}

void icache_disable(void)
{
}

void icache_invalid(void)
{
}

void dcache_enable(void)
{
}

void dcache_disable(void)
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
