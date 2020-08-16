/*
 */

#include <common.h>

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

