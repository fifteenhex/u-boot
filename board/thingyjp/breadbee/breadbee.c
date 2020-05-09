#include <common.h>
#include <spl.h>
#include <environment.h>
#include <u-boot/crc.h>
#include <debug_uart.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "clk.h"
#include "ddr.h"
#include "emac.h"
#include "utmi.h"

DECLARE_GLOBAL_DATA_PTR;

static const uint8_t* deviceid = (uint8_t*) CHIPID;
static const void* efuse = (void*) EFUSE;

static int breadbee_chiptype(void){
	debug("deviceid is %02x\n", (unsigned) *deviceid);
	switch(*deviceid){
		case CHIPID_MSC313:
			return CHIPTYPE_MSC313;
		case CHIPID_MSC313ED:
			if(*(uint16_t*)(efuse + 0x14) == 0x440)
				return CHIPTYPE_MSC313DC;
			else
				return CHIPTYPE_MSC313E;
		case CHIPID_SSC8336:
			return CHIPTYPE_SSC8336;
		case CHIPID_SSC8336N:
			return CHIPTYPE_SSC8336N;
		default:
			return CHIPTYPE_UNKNOWN;
	}
}

int board_init(void)
{
	timer_init();

	// this is needed stop FIQ interrupts bypassing the GIC
	// mstar had this in their irqchip driver but I've moved
	// this here to keep the mess out of view.
	u32 *gicreg = (u32*)(0x16000000 + 0x2000);
	*gicreg = 0x1e0;
	return 0;
}

#ifdef CONFIG_SPL_BUILD
/*void board_boot_order(u32 *spl_boot_list)
{
}*/

void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_SPI;
	spl_boot_list[1] = BOOT_DEVICE_UART;
	spl_boot_list[2] = BOOT_DEVICE_NONE;
}

#define GETU16(b,r)		(*((u16*)(b + r)))
#define SETU16(b, r, v)	(*((u16*)(b + r)) = v)

static void emacclocks(void){
	SETU16(CLKGEN, 0x108, 0);
	SETU16(SCCLKGEN, 0x88, 0x04);
	SETU16(SCCLKGEN, 0x8c, 0x04);
}

static void emacpinctrl(void){
	SETU16(PINCTRL, 0x3c, GETU16(PINCTRL, 0x3c) | 1 << 2);
}

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

void board_init_f(ulong dummy)
{
	uint32_t cpuid;
	int chiptype = breadbee_chiptype();
	bool wakingup = false;

	void* reg;

#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
#endif

	mstar_early_clksetup();
	spl_early_init();
	preloader_console_init();

	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(cpuid));
	printf("\ncpuid: %x, mstar chipid: %x\n", (unsigned) cpuid, (unsigned)*deviceid);

	if(readw(PMSLEEP + PMSLEEP_LOCK) == PMSLEEP_LOCK_MAGIC){
		printf("woken from sleep\n");
		wakingup = true;
	}
	else {
		printf("normal power on\n");
	}


	/* *((u16*)(PMCLKGEN + 0xf4)) = 0;
	for(int i = 0; i < 1000; i++){
		u16 t = *((u16*)(TIMER0 + 0x10));
		printf("t %d\n", (int) t);
	}*/

	//mstar_dump_reg_block("pmsleep", PMSLEEP);
	//mstar_dump_reg_block("clkgen", CLKGEN);

	switch(chiptype){
		case CHIPTYPE_SSC8336:
			m5_misc();
			break;
	}

	mstar_ddr_init(chiptype);

	mstar_utmi_setfinetuning();

	switch(chiptype){
		case CHIPTYPE_MSC313:
			emacpinctrl();
			emacclocks();
			emac_patches();
			emacphypowerup_msc313();
			break;
		case CHIPTYPE_MSC313E:
		case CHIPTYPE_MSC313DC:
			emacpinctrl();
			emacclocks();
			emac_patches();
			emacphypowerup_msc313e();
			break;
		default:
			break;
	}
}

static struct image_header hdr;
struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return &hdr;
}

#endif // spl

#ifndef CONFIG_BOARD_LATE_INIT
#error "CONFIG_BOARD_LATE_INIT is required"
#endif


#define ENV_VAR_MSTAR_FAMILY "mstar_family"
#define COMPAT_I1 "infinity1"
#define COMPAT_I3 "infinity3"
#define COMPAT_I6 "infinity6"
#define COMPAT_M5 "mercury5"

int board_fit_config_name_match(const char *name)
{
	switch(breadbee_chiptype()){
		case CHIPTYPE_MSC313:
			if(!strcmp(name, COMPAT_I1))
				return 0;
			break;
		case CHIPTYPE_MSC313E:
		case CHIPTYPE_MSC313DC:
			if(!strcmp(name, COMPAT_I3)){
				return 0;
			}
			break;
		case CHIPTYPE_SSC325:
			if(!strcmp(name, COMPAT_I6))
				return 0;
			break;
		case CHIPTYPE_SSC8336:
		case CHIPTYPE_SSC8336N:
			if(!strcmp(name, COMPAT_M5))
				return 0;
			break;
	}

	return -1;
}

int board_late_init(void){
#ifndef CONFIG_SPL_BUILD
	const char* family = CHIPTYPE_UNKNOWN;

	switch(breadbee_chiptype()){
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

	switch(breadbee_chiptype()){
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
#endif
	return 0;
}

#ifndef CONFIG_SPL_BUILD

#ifndef CONFIG_OF_BOARD_SETUP
#error "OF_BOARD_SETUP is required"
#endif

#ifdef CONFIG_DTB_RESELECT
int embedded_dtb_select(void)
{
	fdtdec_setup();

	return 0;
}
#endif

int ft_board_setup(void *blob, bd_t *bd)
{
	int i,j;
	uint8_t* didreg = (uint8_t*) 0x1f007000;
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
		sprintf(ethaddr, "ethernet%d", i);
		if (!fdt_get_alias(blob, ethaddr))
			continue;

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
#endif
