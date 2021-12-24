/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __UNITV2_CONFIG_H
#define __UNITV2_CONFIG_H

#include "mstarv7.h"

#define CONFIG_EXTRA_ENV_SETTINGS	"bootargs=console=ttyS0,115200 clk_ignore_unused "\
					"ubi.fm_autoconvert=1 ubi.mtd=2 ubi.block=0,rootfs root=/dev/ubiblock0_4\0"\
					"bootcmd_rescue=ubi readvol ${loadaddr} rescue; bootm ${loadaddr}\0"\
					"bootcmd=ubi readvol ${loadaddr} kernel; bootm ${loadaddr}\0"
#endif
