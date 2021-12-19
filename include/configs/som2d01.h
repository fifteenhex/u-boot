/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __DONGSHANPIONE_CONFIG_H
#define __DONGSHANPIONE_CONFIG_H

#include "mstarv7.h"

#define CONFIG_EXTRA_ENV_SETTINGS	"carrier_board=ssd201-som2d01\0"\
					"bootargs=console=ttyS0,115200 clk_ignore_unused "\
					"ubi.fm_autoconvert=1 ubi.mtd=1 ubi.block=0,rootfs root=/dev/ubiblock0_4\0"\
					"bootcmd_rescue=ubi readvol ${loadaddr} rescue; bootm ${loadaddr}#${carrier_board}\0"\
					"bootcmd=ubi readvol ${loadaddr} kernel; bootm ${loadaddr}#${carrier_board}\0"

#endif
