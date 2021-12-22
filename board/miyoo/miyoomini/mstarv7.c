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
	mstar_cpupll_init();
#endif

	mstar_bump_cpufreq();

	printf("mplldbg %x\n", mplldbg);
	for(int i = 0; i < 5; i++){
		printf("mpll: %x - %x\n", i * 4, mpllregs[i]);
	}

	//mstar_utmi_setfinetuning();
	//mstar_clockfixup();
}



#endif // spl

int embedded_dtb_select(void)
{
	fdtdec_setup();
	return 0;
}


int board_late_init(void){
	// hacks!
	// set mmc pinctrl
	*((u32*)0x1f203c20) = 0x100;
	*((u32*)0x1f226694) = 0x8; // set secondary sdio gate to clkgen
	*((u32*)0x1f207114) = 0x18; // degate sdio clock

	return 0;
}

#ifndef CONFIG_SPL_BUILD
int ft_board_setup(void *blob, struct bd_info *bd)
{
	return mstar_fill_macaddress();
}
#endif
