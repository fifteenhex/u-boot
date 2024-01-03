// SPDX-License-Identifier: GPL-2.0+

#include <linux/types.h>
#include <asm/processor.h>

void enable_interrupts(void)
{
}

int disable_interrupts(void)
{
	return 0;
}

void int_handler (int vec, struct pt_regs *fp)
{
	while (1)
	{

	}
}
