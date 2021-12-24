// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020 Daniel Palmer<daniel@thingy.jp>
 */

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

#include <mstar/board.h>

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

#ifdef CONFIG_SPL_SPI_NAND_SUPPORT
	spl_boot_list[index++] = BOOT_DEVICE_SPINAND;
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
#ifdef CONFIG_MSTAR_INFINITY2M
		case CHIPTYPE_SSD201:
			if(!strcmp(name, COMPAT_I2M_SSD201)){
				return 0;
			}
			break;
		case CHIPTYPE_SSD202D:
			if(!strcmp(name, COMPAT_I2M_SSD202D)){
				return 0;
			}
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
#ifdef CONFIG_MSTAR_INFINITY6
		case CHIPTYPE_SSC325:
			if(!strcmp(name, COMPAT_I6))
				return 0;
			break;
#endif
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

int mstar_fill_macaddress(void)
{
	int i,j;
	uint8_t* didreg = (uint8_t*) DID;
	uint8_t mac_addr[6];
	uint8_t did[6];
	uint32_t didcrc32;
	char ethaddr[16];

	for(i = 0; i < 3; i++){
		for(j = 0; j < 2; j++){
			did[(i * 2) + j] = *(didreg + ((i * 4) + j));
		}
	}

	didcrc32 = crc32(0, did, sizeof(did));

	// stolen from sunxi
	for (i = 0; i < 4; i++) {
//		sprintf(ethaddr, "ethernet%d", i);
//		if (!fdt_get_alias(blob, ethaddr))
//			continue;

		if (i == 0)
			strcpy(ethaddr, "ethaddr");
		else
			sprintf(ethaddr, "eth%daddr", i);

		if (env_get(ethaddr))
			continue;
		mac_addr[0] = 0xbe;
		mac_addr[1] = 0xe0 | ((didcrc32 >> 28) & 0xf);
		mac_addr[2] = ((didcrc32 >> 20) & 0xff);
		mac_addr[3] = ((didcrc32 >> 12) & 0xff);
		mac_addr[4] = ((didcrc32 >> 4) & 0xff);
		mac_addr[5] = ((didcrc32 << 4) & 0xf0) | i;

		eth_env_set_enetaddr(ethaddr, mac_addr);
	}
	return 0;
}

void mstar_check_ipl(void)
{
	struct mstar_ipl *ipl = (struct mstar_ipl*) MSTAR_SRAM;
	u32 *data = (void*)(ipl + 1), chksum = 0;
	char ipl_hdr[sizeof(ipl->header) + 1] = {};
	int i;

	for(i = sizeof(*ipl); i < ipl->size; i += sizeof(u32))
		chksum += *data++;

	memcpy(ipl_hdr, &ipl->header, 4);
	debug("IPL: header %s, size %d, chksum 0x%08x(0x%08x)\n", ipl_hdr,
			ipl->size, ipl->chksum, chksum);

	if(ipl->chksum != chksum)
	{
		printf("IPL image is broken\n");
	}
}

void mstar_poweron_reason(void)
{
	bool wakingup = false;

	if(readw(PMSLEEP + PMSLEEP_LOCK) == PMSLEEP_LOCK_MAGIC){
		printf("woken from sleep\n");
		wakingup = true;
	}
	else {
		printf("normal power on\n");
	}
}

int mstar_cpupll_init(void)
{
	struct udevice *dev;
	int rv;
	struct clk clk;

	rv = uclass_get_device_by_name(UCLASS_CLK, "cpupll@206400",
			&dev);
	if (rv)
		debug("CPUPLL init failed: %d\n", rv);

	clk.dev = dev;
	clk_enable(&clk);

	return rv;
}

int mstar_miu_init(void)
{
	struct udevice *dev;
	int rv;

	rv = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (rv)
		debug("DRAM init failed: %d\n", rv);

	return rv;
}

struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return CONFIG_SYS_TEXT_BASE + offset;
}

int mstar_board_late_init(void)
{
#ifndef CONFIG_SPL_BUILD
	return mstar_fill_macaddress();
#else
	return 0;
#endif
}

__weak int board_late_init(void)
{
	return mstar_board_late_init();
}

__weak int embedded_dtb_select(void)
{
	fdtdec_setup();
	return 0;
}

__weak int ft_board_setup(void *blob, struct bd_info *bd)
{
	return 0;
}

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

void mstar_board_init_f(ulong dummy)
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

__weak void board_init_f(ulong dummy)
{
	mstar_board_init_f(dummy);
}

#endif // spl
