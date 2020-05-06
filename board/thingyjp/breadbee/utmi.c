/*
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "utmi.h"

void mstar_utmi_setfinetuning()
{
	uint16_t efuse_20 = readw(EFUSE + EFUSE_20);
	uint16_t utmi0_50 = readw(UTMI0 + UTMI0_50);
	uint16_t utmi1_50 = readw(UTMI1 + UTMI1_50);

	printf("efuse_20: %04x, utmi0_50 %04x, utmi1_50 %04x\n",
			efuse_20, utmi0_50, utmi1_50);

}

