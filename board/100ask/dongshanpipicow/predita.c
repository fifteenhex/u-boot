/*
 * Mstar Armv7 generic board
 */

#include <init.h>
#include <fdtdec.h>

#ifdef CONFIG_DEBUG_UART
#include <debug_uart.h>
#endif

#include <common.h>
#include <asm/u-boot.h>
#include <spl.h>
#include <env.h>
#include <u-boot/crc.h>
#include <asm/io.h>
#include <dm.h>
#include <clk.h>
#include <ipl.h>
#include <image.h>

#include <mstar/board.h>

#include "chenxingv7.h"

/* check that some required config options are selected */

#ifndef CONFIG_BOARD_LATE_INIT
#error "BOARD_LATE_INIT is required"
#endif

#ifndef CONFIG_DTB_RESELECT
#error "DTB_RESELECT is required"
#endif

#ifndef CONFIG_OF_BOARD_SETUP
#error "OF_BOARD_SETUP is required"
#endif

#ifndef CONFIG_MULTI_DTB_FIT
#error "MULTI_DTB_FIT is required"
#endif

//#ifdef CONFIG_SPL_BUILD
//#ifndef CONFIG_SPL_LOAD_FIT
//#error "CONFIG_SPL_LOAD_FIT is required"
//#endif
//#endif


DECLARE_GLOBAL_DATA_PTR;

int board_late_init(void)
{
	int ret;

	ret = mstar_board_late_init();
	if(ret)
		return ret;

	return 0;
}
