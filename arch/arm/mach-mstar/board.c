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
