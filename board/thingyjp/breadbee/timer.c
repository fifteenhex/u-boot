/*
 * timer.c
 *
 *  Created on: 8 May 2020
 *      Author: daniel
 */

#include <common.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#include "chenxingv7.h"

static uint32_t mstar_timer_get_count(void)
{
	uint32_t count;
	count = readw(TIMER0 + TIMER0_COUNTER_H);
	count = count << 16;
	count |= readw(TIMER0 + TIMER0_COUNTER_L);
	return count;
}

void __udelay(unsigned long usec)
{
	const int granularity = 100;
	unsigned long ticks = usec * 12;
	uint16_t ctrl = readw(TIMER0 + TIMER0_CTRL);
	while(usec > 0){
		writew(TIMER0_CTRL_CLR | TIMER0_CTRL_EN, TIMER0 + TIMER0_CTRL);
		while(mstar_timer_get_count() < 12 * granularity){
		}
		writew(0, TIMER0 + TIMER0_CTRL);
		if(usec < granularity)
			usec = 0;
		else
			usec -= granularity;
	}
	writew(0, TIMER0 + TIMER0_CTRL);
}
