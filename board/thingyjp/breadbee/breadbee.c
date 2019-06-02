#include <common.h>
#include <spl.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	timer_init();
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

/*
 *
 *     //power down eth
    //wriu  -w  0x0032fc  0x0102   // Power-down LDO
    //wriu      0x0032b7  0x17     // Power-down ADC
    //wriu      0x0032cb  0x13     // Power-down BGAP
    //wriu      0x0032cc  0x30     // Power-down ADCPL
    //wriu      0x0032cd  0xd8     // Power-down ADCPL
    //wriu      0x0032d4  0x20     // Power-down LPF_OP
    //wriu      0x0032b9  0x41     // Power-down LPF
    //wriu      0x0032bb  0x84     // Power-down REF
    //wriu  -w  0x00333a  0x03f3   // PD_TX_IDAC, PD_TX_LD
    //wriu      0x0033a1  0x20     // PD_SADC, EN_SAR_LOGIC**
    //wriu      0x0033c5  0x40     // 100gat
    //wriu      0x003330  0x53     // 200gat
    OUTREG16(0x1F0065F8, 0x0102);
    OUTREG8 (0x1F00656D, 0x17);
    OUTREG8 (0x1F006595, 0x13);
    OUTREG8 (0x1F006598, 0x30);
    OUTREG8 (0x1F006599, 0xd8);
    OUTREG8 (0x1F0065A8, 0x20);
    OUTREG8 (0x1F006571, 0x41);
    OUTREG8 (0x1F006575, 0x84);
    OUTREG16(0x1F006674, 0x03f3);
    OUTREG8 (0x1F006741, 0x20);
    OUTREG8 (0x1F006789, 0x40);
    OUTREG8 (0x1F006660, 0x53);
 */

/*static void emacphypowerup(void){
	printf("emac power up\n");

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
}*/

static void emacphypowerup(){
	 printf("emac power up\n");
	     *(int8_t *)0x1f2a2274 = 0x0;
	     *(int8_t *)0x1f2a2275 = 0x1;
	     *(int8_t *)0x1f2a2200 = *(int8_t *)0x1f2a2200 | 0x10;
	     *(int8_t *)0x1f2a2271 = *(int8_t *)0x1f2a2271 | 0x1;
	     *(int8_t *)0x1f207108 = 0x0;
	     *(int8_t *)0x1f226688 = 0x0;
	     *(int8_t *)0x1f22668c = 0x0;
	     *(int8_t *)0x1f006568 = 0x2;
	     *(int8_t *)0x1f00649d = 0x2;
	     *(int8_t *)0x1f0064a1 = 0x1;
	     *(int8_t *)0x1f0064ed = 0x18;
	     *(int8_t *)0x1f0062e4 = 0x5f ^ 0xffffffff;
	     *(int8_t *)0x1f0065f8 = 0x0;
	     *(int8_t *)0x1f0065f9 = 0x0;
	     *(int8_t *)0x1f006741 = 0x7f ^ 0xffffffff;
	     *(int8_t *)0x1f006598 = 0x40;
	     *(int8_t *)0x1f006575 = 0x4;
	     *(int8_t *)0x1f006674 = 0x0;
	     *(int8_t *)0x1f0067e1 = 0x0;
	     *(int8_t *)0x1f006714 = 0x1;
	     *(int8_t *)0x1f006475 = 0x1;
	     *(int8_t *)0x1f006588 = 0x44;
	     *(int8_t *)0x1f006700 = 0x30;
	     *(int8_t *)0x1f006789 = 0x0;
	     *(int8_t *)0x1f006660 = 0x43;
	     *(int8_t *)0x1f006671 = 0x41;
	     *(int8_t *)0x1f0067e4 = 0xa ^ 0xffffffff;
	     *(int8_t *)0x1f0067e5 = 0xd;
	     *(int8_t *)0x1f0062f1 = 0x2f ^ 0xffffffff;
	     *(int8_t *)0x1f0062ed = 0x5a;
	     *(int8_t *)0x1f006259 = 0x7c;
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006255 = 0x0;
	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006255 = 0x0;
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006358 = 0x1c;
	     *(int8_t *)0x1f006359 = 0x1c;
	     *(int8_t *)0x1f00635c = 0x1c;
	     *(int8_t *)0x1f00635d = 0x1c;
	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006355 = 0x28;
	     *(int8_t *)0x1f0065e9 = 0x2;
	     *(int8_t *)0x1f00641d = 0x36 ^ 0xffffffff;
	     *(int8_t *)0x1f006511 = 0x50;
	     *(int8_t *)0x1f006515 = 0x7f ^ 0xffffffff;
	     *(int8_t *)0x1f00651c = 0xe;
	     *(int8_t *)0x1f006520 = 0x4;
	     *(int8_t *)0x1f203d41 = *(int8_t *)0x1f203d41 & 0x7f;
	     *(int8_t *)0x1f001ca0 = *(int8_t *)0x1f001ca0 & 0xcf | 0x10;
}

static void emac_patches(void){
	printf("emac patches\n");
	*(u16*)(0x1f2a2000 + 0x200) = 0xF051; // mstar call this julian100, magic number, seems to be related to the phy
	*(u16*)(0x1f2a2000 + 0x204) = 0x0000;
	*(u16*)(0x1f2a2000 + 0x208) = 0x0001; // mstar call this julian104, this enables software descriptors apparently
	*(u16*)(0x1f2a2000 + 0x20c) = 0x0000;
}

void board_init_f(ulong dummy)
{
	timer_init();

// leave everything as is if we're using the mstar ipl to do the setup
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
#endif
	emacpinctrl();
	emacclocks();
	emacphypowerup();
	emac_patches();
}

static struct image_header hdr;
struct image_header *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return &hdr;
}

#endif
