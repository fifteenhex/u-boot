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
#include "emac.h"
#include "utmi.h"
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

#ifdef CONFIG_SPL_BUILD
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

#ifndef CONFIG_MSTAR_IPL
static int cpupll_init(void)
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

static void check_ipl(void)
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

static void poweron_reason(void)
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

	check_ipl();
	poweron_reason();

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

int embedded_dtb_select(void)
{
	fdtdec_setup();
	return 0;
}

#ifndef CONFIG_SPL_BUILD
int eth_setup(void)
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
#endif

int board_late_init(void){
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

