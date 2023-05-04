/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __PREDITA_CONFIG_H
#define __PREDITA_CONFIG_H
#include "mstarv7.h"

#define CONFIG_EXTRA_ENV_SETTINGS "bootargs=" MSTARV7_CMDLINEOPTS "\0" \
				  "dfu_alt_info=all raw 0x0 0x8000000;gcis raw 0x0 0x20000;spl0 raw 0x20000 0x20000;spl1 raw 0x40000 0x20000;ubi raw 0x60000 0x7FA0000\0"

#endif
