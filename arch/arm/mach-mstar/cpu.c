/*
 */

#include <cpu_func.h>

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

