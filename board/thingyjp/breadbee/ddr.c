/*
 * ddr.c
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"

static void mstar_ddr2_init(void)
{
	printf("doing DDR2 init\n");
}

static void mstar_ddr3_init(void)
{
	printf("doing DDR3 init\n");
}

void mstar_ddr_init()
{
	uint32_t efuse_14;
	printf("doing ddr setup, hold onto your pants...\n");
	efuse_14 = readw_relaxed(EFUSE + EFUSE_14);
	printf("efuse: %04x\n", efuse_14);

	mstar_ddr2_init();
}

