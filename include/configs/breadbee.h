/* SPDX-License-Identifier: GPL-2.0+ */
/*
 */

#ifndef __BREADBEE_CONFIG_H
#define __BREADBEE_CONFIG_H

#include "mstarv7.h"

#define CONFIG_EXTRA_ENV_SETTINGS "loadaddr=0x22000000\0"\
				  "bb_noroff_fit=0x80000\0"\
				  "bb_norsz_fit=0x300000\0"\
				  "bb_noroff_rescue=0xd00000\0"\
				  "bb_norsz_rescue=0x300000\0"\
				  "bootcmd=sf probe; sf read ${loadaddr} ${bb_noroff_fit} ${bb_norsz_fit}; bootm ${loadaddr}#${bb_boardtype}${bb_config}\0"\
				  "bb_boot_failsafe=sf probe; if sf read ${loadaddr} ${bb_noroff_fit} ${bb_norsz_fit}; then bootm ${loadaddr}#${bb_boardtype}; fi\0"\
				  "bb_boot_rescue=sf probe; if sf read ${loadaddr} ${bb_noroff_rescue} ${bb_norsz_rescue}; then bootm ${loadaddr}#${bb_boardtype}; fi\0"

#endif
