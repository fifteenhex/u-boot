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
#include <chenxingv7.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPL_BUILD
void board_boot_order(u32 *spl_boot_list)
{
	int index = 0;

	uint16_t bootsource = readw(DID + DID_BOOTSOURCE);
	switch(mstar_chiptype()){
		case CHIPTYPE_SSC8336:
		case CHIPTYPE_SSC8336N:
			if(bootsource & DID_BOOTSOURCE_M5_SD)
				printk("will try sd first\n");
			break;
	}

#ifdef CONFIG_SPL_MMC_SUPPORT
	spl_boot_list[index++] = BOOT_DEVICE_MMC1;
#endif

#ifdef CONFIG_SPL_SPI_FLASH_SUPPORT
	spl_boot_list[index++] = BOOT_DEVICE_SPI;
#endif

#ifdef CONFIG_SPL_YMODEM_SUPPORT
	spl_boot_list[index++] = BOOT_DEVICE_UART;
#endif

	spl_boot_list[index++] = BOOT_DEVICE_NONE;
}
#endif

int board_fit_config_name_match(const char *name)
{
	switch(mstar_chiptype()){
#ifdef CONFIG_MSTAR_INFINITY1
		case CHIPTYPE_MSC313:
			if(!strcmp(name, COMPAT_I1_MSC313))
				return 0;
			break;
#endif
#ifdef CONFIG_MSTAR_INFINITY3
		case CHIPTYPE_MSC313E:
			if(!strcmp(name, COMPAT_I3_MSC313E)){
				return 0;
			}
			break;
		case CHIPTYPE_MSC313DC:
			if(!strcmp(name, COMPAT_I3)){
				return 0;
			}
			break;
#endif
		case CHIPTYPE_SSC325:
			if(!strcmp(name, COMPAT_I6))
				return 0;
			break;
#ifdef CONFIG_MSTAR_MERCURY5
		case CHIPTYPE_SSC8336:
		case CHIPTYPE_SSC8336N:
			if(!strcmp(name, COMPAT_M5))
				return 0;
			break;
#endif
	}

	//if(!strcmp(name, COMPAT_GENERIC))
	//	return 0;

	return -1;
}

int board_init(void)
{
	mstar_bump_cpufreq();

	// this is needed stop FIQ interrupts bypassing the GIC
	// mstar had this in their irqchip driver but I've moved
	// this here to keep the mess out of view.
	u32 *gicreg = (u32*)(0x16000000 + 0x2000);
	*gicreg = 0x1e0;
	return 0;
}

/*
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "clk.h"

void mstar_clockfixup()
{
	// once the DDR is running the deglitch clock doesn't work anymore.
	writew_relaxed(0x10, CLKGEN + CLKGEN_BDMA);
}

