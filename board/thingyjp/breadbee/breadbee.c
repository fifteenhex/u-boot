#include <asm/u-boot.h>
#include <common.h>
#include <linux/libfdt.h>
#include <spl.h>
#include <env.h>
#include <u-boot/crc.h>
#include <debug_uart.h>
#include <asm/io.h>
#include <dm.h>
#include <clk.h>
#include <init.h>
#include <ipl.h>
#include <image.h>
#include "clk.h"
#include "utmi.h"
#include "chenxingv7.h"
#include <mstar/board.h>

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

u32 mpllregs[5];
uint mplldbg;

#ifdef CONFIG_SPL_BUILD
static void m5_misc(void)
{
	// the m5 ipl does this before DRAM setup
	// zero'ing these registers while running
	// doesn't seem to break anything though.

	mstar_writew(0x2201, 0x1f206700);
	mstar_writew(0x0420, 0x1f206704);
	mstar_writew(0x0041, 0x1f206708);
	mstar_writew(0x0000, 0x1f20670c);
	mstar_writew(0xdd2f, 0x1f206720);
	mstar_writew(0x0024, 0x1f206724);
	mstar_writew(0x0000, 0x1f20672c);
	mstar_writew(0x0001, 0x1f206728);
}

#ifndef CONFIG_MSTAR_IPL
static int miu_init(void)
{
	struct udevice *dev;
	int rv;

	rv = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (rv)
		debug("DRAM init failed: %d\n", rv);

	return rv;
}
#endif

void board_init_f(ulong dummy)
{
	uint32_t cpuid;
	int chiptype = mstar_chiptype();
	void* reg;

#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
#endif

	spl_early_init();
	preloader_console_init();

	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(cpuid));
	printf("\ncpuid: %x, mstar chipid: %x\n",
			(unsigned) cpuid,
			(unsigned)*deviceid);

	mstar_check_ipl();
	mstar_poweron_reason();

	switch(chiptype){
		case CHIPTYPE_SSC8336:
			m5_misc();
			break;
	}

#ifndef CONFIG_MSTAR_IPL
	miu_init();
	cpupll_init();
#endif

	mstar_bump_cpufreq();

	mstar_utmi_setfinetuning();

	mstar_clockfixup();
}

#endif // spl

int embedded_dtb_select(void)
{
	fdtdec_setup();
	return 0;
}

#ifndef CONFIG_SPL_BUILD
int eth_setup(void)
{
	return mstar_fill_macaddress();
}
#endif

int board_late_init(void)
{
#ifndef CONFIG_SPL_BUILD
	const char* family = CHIPTYPE_UNKNOWN;
	int chiptype = mstar_chiptype();

	switch(chiptype){
		case CHIPTYPE_MSC313:
			family = COMPAT_I1;
			break;
		case CHIPTYPE_MSC313E:
		case CHIPTYPE_MSC313DC:
			family = COMPAT_I3;
			break;
		case CHIPTYPE_SSC325:
			family = COMPAT_I6;
			break;
		case CHIPTYPE_SSC8336:
		case CHIPTYPE_SSC8336N:
			family = COMPAT_M5;
			break;
		default:
			break;
	}

	env_set(ENV_VAR_MSTAR_FAMILY, family);

	switch(chiptype){
		case CHIPTYPE_MSC313:
			env_set("bb_boardtype", "breadbee_crust");
			break;
		case CHIPTYPE_MSC313E:
			env_set("bb_boardtype", "breadbee");
			break;
		case CHIPTYPE_MSC313DC:
			env_set("bb_boardtype", "breadbee_super");
			break;
		default:
			env_set("bb_boardtype", "unknown");
			break;
	}

	eth_setup();

#endif
	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

