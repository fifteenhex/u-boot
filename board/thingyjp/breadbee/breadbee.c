#include <common.h>
#include <spl.h>
#include <environment.h>
#include <u-boot/crc.h>
#include <debug_uart.h>

DECLARE_GLOBAL_DATA_PTR;

#define CHIPTYPE_UNKNOWN	0
#define CHIPTYPE_MSC313		1
#define CHIPTYPE_MSC313E	2
#define CHIPTYPE_MSC313DC	3
#define CHIPTYPE_SSC8336	4
#define CHIPTYPE_SSC8336N	5
#define CHIPTYPE_SSC325		6

#define CHIPID_MSC313		0xae
#define CHIPID_MSC313ED		0xc2 // this is the same for E and D
#define CHIPID_SSC8336		0xd9
#define CHIPID_SSC8336N		0xee
#define CHIPID_SSC325		0xef

#define EFUSE 0x1f004000

static const uint8_t* deviceid = (uint8_t*) 0x1f003c00;
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

#define TIMER0		0x1f006040
#define PMCLKGEN	0x1f001c00

#define CLKGEN		0x1f207000
#define SCCLKGEN	0x1f226600

#define MUI			0x1f202400
#define PINCTRL		0x1f203c00

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

#define EMAC_RIU_REG_BASE           (0x1F000000)
#define REG_BANK_ALBANY0                    0x0031
#define REG_BANK_ALBANY1                    0x0032
#define REG_BANK_ALBANY2                    0x0033
void MHal_EMAC_WritReg8( u32 bank, u32 reg, u8 val )
{
    u32 address = EMAC_RIU_REG_BASE + bank*0x100*2;
    address = address + (reg << 1) - (reg & 1);

    *( ( volatile u8* ) address ) = val;
}

static void emacphypowerup_msc313(void){
	printf("emac power up, msc313\n");

	//gain shift
	MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb4, 0x02);

	    //det max
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x4f, 0x02);

	    //det min
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x51, 0x01);

	    //snr len (emc noise)
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x77, 0x18);

	    //lpbk_enable set to 0
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x72, 0xa0);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xfc, 0x00);   // Power-on LDO
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xfd, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb7, 0x17);   // Power-on ADC**
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcb, 0x11);   // Power-on BGAP
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcc, 0x20);   // Power-on ADCPL
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcd, 0xd0);   // Power-on ADCPL
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xd4, 0x00);   // Power-on LPF_OP
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb9, 0x40);   // Power-on LPF
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xbb, 0x05);   // Power-on REF
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x3a, 0x03);   // PD_TX_IDAC, PD_TX_LD = 0
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x3b, 0x00);


	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x3b, 0x01);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xa1, 0xc0);// PD_SADC, EN_SAR_LOGIC**
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x8a, 0x01);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xc4, 0x44);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x80, 0x30);

	    //100 gat
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xc5, 0x00);

	    //200 gat
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x30, 0x43);

	    //en_100t_phase
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x39, 0x41);   // en_100t_phase;  [6] save2x_tx

	    // Prevent packet drop by inverted waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x79, 0xd0);   // prevent packet drop by inverted waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x77, 0x5a);

	    //disable eee
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2d, 0x7c);   // disable eee

	    //10T waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x06);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2b, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2b, 0x00);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x06);   // shadow_ctrl
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaa, 0x1c);   // tin17_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xac, 0x1c);   // tin18_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xad, 0x1c);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xae, 0x1c);   // tin19_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaf, 0x1c);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaa, 0x1c);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xab, 0x28);

	    //speed up timing recovery
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xf5, 0x02);

	    // Signal_det k
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x0f, 0xc9);

	    // snr_h
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x89, 0x50);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x8b, 0x80);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x8e, 0x0e);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x90, 0x04);

	    //set CLKsource to hv
	       MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xC7, 0x80);
}

static void emacphypowerup_msc313e(void){
	 printf("emac power up, msc313e\n");
	     // gain shift
	     *(int8_t *)0x1f006568 = 0x2;

	     // det max
	     *(int8_t *)0x1f00649d = 0x2;

	     // det min
	     *(int8_t *)0x1f0064a1 = 0x1;

	     //snr len(emc noise)
	     *(int8_t *)0x1f0064ed = 0x18;

	     // lpbk_enable set to 0
	     *(int8_t *)0x1f0062e4 = 0xa0;

	     // power on ldo
	     *(int8_t *)0x1f0065f8 = 0x0;
	     *(int8_t *)0x1f0065f9 = 0x0;
	     // power on adc
	     *(int8_t *)0x1f006741 = 0x80;
	     // power on bgap
	     *(int8_t *)0x1f006598 = 0x40;
	     // power on adcpl
	     *(int8_t *)0x1f006575 = 0x4;
	     *(int8_t *)0x1f006674 = 0x0;
	     // power on lpf_op
	     *(int8_t *)0x1f0067e1 = 0x0;

	     // lpf
	     *(int8_t *)0x1f006714 = 0x1;


	     *(int8_t *)0x1f006475 = 0x1;
	     *(int8_t *)0x1f006588 = 0x44;
	     *(int8_t *)0x1f006700 = 0x30;
	     *(int8_t *)0x1f006789 = 0x0;
	     *(int8_t *)0x1f006660 = 0x43;

	     // 100 gat
	     *(int8_t *)0x1f006671 = 0x41;

	     // 200 gat
	     *(int8_t *)0x1f0067e4 = 0xf5;

	     // en_100t_phase
	     *(int8_t *)0x1f0067e5 = 0xd;

	     // prevent packet drop by inverted waveform
	     *(int8_t *)0x1f0062f1 = 0xd0;
	     *(int8_t *)0x1f0062ed = 0x5a;

	     // disable eee
	     *(int8_t *)0x1f006259 = 0x7c;

	     // 10T waveform
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006255 = 0x0;
	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006255 = 0x0;

	     // shadow_ctrl
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006358 = 0x1c;
	     *(int8_t *)0x1f006359 = 0x1c;
	     *(int8_t *)0x1f00635c = 0x1c;
	     *(int8_t *)0x1f00635d = 0x1c;


	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006355 = 0x28;

	     // speed up timing recovery
	     *(int8_t *)0x1f0065e9 = 0x2;

	     // signal_det ket
	     *(int8_t *)0x1f00641d = 0xc9;

	     // snr h
	     *(int8_t *)0x1f006511 = 0x50;
	     *(int8_t *)0x1f006515 = 0x80;
	     *(int8_t *)0x1f00651c = 0xe;
	     *(int8_t *)0x1f006520 = 0x4;

	     // pinctrl
	     *(int8_t *)0x1f203d41 = *(int8_t *)0x1f203d41 & 0x7f;



	     *(int8_t *)0x1f001ca0 = *(int8_t *)0x1f001ca0 & 0xcf | 0x10;
}

static void emac_patches(void){
	printf("emac patches\n");

	// this is "switch rx descriptor format to mode 1"
	*(int8_t *)0x1f2a2274 = 0x0;
	*(int8_t *)0x1f2a2275 = 0x1;

	// RX shift patch
	*(int8_t *)0x1f2a2200 = *(int8_t *)0x1f2a2200 | 0x10;

	// TX underrun patch
	*(int8_t *)0x1f2a2271 = *(int8_t *)0x1f2a2271 | 0x1;

	// clkgen setup
	*(int8_t *)0x1f207108 = 0x0;
	*(int8_t *)0x1f226688 = 0x0;
	*(int8_t *)0x1f22668c = 0x0;

	*(u16*)(0x1f2a2000 + 0x200) = 0xF051; // mstar call this julian100, magic number, seems to be related to the phy
	*(u16*)(0x1f2a2000 + 0x204) = 0x0000;
	*(u16*)(0x1f2a2000 + 0x208) = 0x0001; // mstar call this julian104, this enables software descriptors apparently
	*(u16*)(0x1f2a2000 + 0x20c) = 0x0000;
}

void board_init_f(ulong dummy)
{
	uint32_t cpuid;

#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
#endif

	spl_early_init();
	preloader_console_init();

	asm volatile("mrc p15, 0, %0, c0, c0, 0" : "=r"(cpuid));
	printf("\ncpuid: %x, mstar chipid: %x\n", (unsigned) cpuid, (unsigned)*deviceid);

// leave everything as is if we're using the mstar ipl to do the setup
#if 0
#ifndef CONFIG_MSTAR_IPL
	u16 *gpioreg = (u8*) (0x1f207800 + 0xc8);
	*gpioreg = 0x10;

	*((u16*)(PMCLKGEN + 0xf4)) = 0;
	for(int i = 0; i < 1000; i++){
		u16 t = *((u16*)(TIMER0 + 0x10));
		printf("t %d\n", (int) t);
	}



	/*printf("pmclkgen\n");
	for(int pmclk = 0; pmclk < 0x100; pmclk += 4)
		printf("%02x - %08x\n", pmclk, *((u16*)(PMCLKGEN + pmclk)));*/

	/*printf("clkgen\n");
	for(int clk = 0; clk < 0x200; clk += 4)
		printf("%02x - %08x\n", clk, *((u16*)(CLKGEN + clk)));*/

	// this seems to be the settings for the pinmux for the internal DDR
	*((u16*)0x1f203ce0) = 0xffff;
	*((u16*)0x1f203ce4) = 0x3;
	*((u16*)0x1f203ce8) = 0xffff;
	*((u16*)0x1f203cec) = 0x3;
	*((u16*)0x1f203cf0) = 0;
	*((u16*)0x1f203cf4) = 0;

	// this seems to be resetting some MUI registers before setting up the DDR
	*((u16*)0x1f20248c) = 0;
	*((u16*)0x1f2024cc) = 0;
	*((u16*)0x1f20250c) = 0;
	*((u16*)0x1f20254c) = 0;
	*((u16*)0x1f20220c) = 0;
	*((u16*)0x1f20224c) = 0;
	*((u16*)0x1f2023cc) = 0;
	*((u16*)0x1f20243c) = 0x8c08;

	int16_t first = *((int16_t*) 0x1f004020);
	printf("first, %x\n", (unsigned) first);
	if((first << 0x1b) < 0){
		printf("-- f\n");
		int16_t* reg0 = (int16_t *)0x1f284250;
		int16_t* reg1 = (int16_t *)0x1f285250;
		printf("%x, %x\n", (unsigned) *reg0, (unsigned) *reg1);
		unsigned value = (first & 0xf) << 5;
		*reg0 = value;
		*reg1 = value;
		//*(int16_t *)0x1f284250 = (*(int16_t *)0x1f284250 & !0x1e0) << 0x10 >> 0x10 | (r0 & 0xf) << 0x5;
		//*(int16_t *)0x1f285250 = (*(int16_t *)0x1f285250 & !0x1e0) << 0x10 >> 0x10 | (r0 & 0xf) << 0x5;*/
	}

	int16_t second = *((int16_t*) 0x1f004024);
	printf("second, %x\n", (unsigned) second);
	if((second << 0x15) < 0x0){
		printf("-- s\n");
        /*int16_t r3 = *(int16_t *)0x1f004024;
        int16_t r2 = 0x1f001d90;
        int16_t r1 = *(int16_t *)0x1f004020;
        int16_t r3 = *(int16_t *)r2;
        asm { ubfx       r1, r1, #0x8, #0x6 };
        *(int16_t *)r2 = (r3 & !0x7e) << 0x10 >> 0x10 | r1 * 0x2;*/

	}

	int16_t third = *((int16_t*) 0x1f00402c);
	printf("third, %x\n", (unsigned) third);
	if((third << 0x1b) < 0x0){
		printf("-- t\n");
	}

	int16_t forth = *((int16_t*) 0x1f004020);
	printf("forth, %x\n", (unsigned) forth);
	if((forth << 0x18) < 0x0){
		printf("-- f\n");
	}
#endif // MStar IPL
#endif

	switch(breadbee_chiptype()){
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
