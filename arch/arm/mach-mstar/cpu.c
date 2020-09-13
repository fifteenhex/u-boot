// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 Daniel Palmer<daniel@thingy.jp>
 */

#include <cpu_func.h>

void enable_caches(void)
{
	icache_enable();
	dcache_enable();
}

int print_cpuinfo(void) {
	return 0;
}

void reset_cpu(void)
{
	volatile u16* resetreg = (u16*) 0x1f001cb8;
	*resetreg = 0x00FF;
	*resetreg = 0x0079;
	while(true){

	}
}
