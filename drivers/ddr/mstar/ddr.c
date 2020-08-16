/*
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <regmap.h>


#include "chenxingv7.h"
#include "miu.h"
#include "clk.h"
#include "ddr.h"

struct ddr_config {
	uint32_t size;
	enum mstar_dram_type type;

	void *group0, *group1, *group2, *group3, *group4, *group5, *group6,
			*group7;
	int group0_init_mask, group1_init_mask, group2_init_mask,
			group3_init_mask, group4_init_mask, group5_init_mask,
			group6_init_mask, group7_init_mask;
	int dig_config2;

	uint16_t mr0, mr1, mr2, mr3;
	uint16_t rd_phase_timing;
	uint16_t ptn_mode;
	uint16_t ana_5c, ana_d8, ana_dc, ana_ec;
	uint16_t ana_120, ana_128;
	uint16_t ana_148, ana_14c;
	uint16_t ana_150, ana_154, ana_158, ana_15c;
	uint16_t ana_170, ana_174, ana_178, ana_17c;
	uint16_t ana_1f0;
};

static void mstar_ddr_dig_rst(struct regmap *dig)
{
	// what is the 0xc00 part?
	regmap_write(dig, MIU_DIG_SW_RST, MIU_DIG_SW_RST_MIU | 0xc00);
}

static void mstar_ddr_dig_rst_release(struct regmap *dig)
{
	regmap_write(dig,  MIU_DIG_SW_RST, 0x8c00);
}

static void mstar_ddr_setdigconfig(const struct ddr_config *config,
		struct regmap *dig)
{
	// 4 banks, 10 cols - 64MB?
	mstar_writew(0x0392, MIU_DIG + MIU_DIG_CONFIG0);
	mstar_writew(0x000d, MIU_DIG + MIU_DIG_CONFIG1);
	mstar_writew(config->dig_config2, MIU_DIG + MIU_DIG_CONFIG2);

	// mr config
	mstar_writew(config->mr0, MIU_DIG + MIU_DIG_MR0);
	mstar_writew(config->mr1, MIU_DIG + MIU_DIG_MR1);
	mstar_writew(config->mr2, MIU_DIG + MIU_DIG_MR2);
	mstar_writew(config->mr3, MIU_DIG + MIU_DIG_MR3);

	// timing configuration
	regmap_write(dig, REG_CONFIG4, 0x1e99);
	regmap_write(dig, REG_CONFIG5, 0x2777);
	regmap_write(dig, MIU_DIG_TIMING2, 0x9598);
	regmap_write(dig, MIU_DIG_TIMING3, 0x4046);

	// 2450, only on m5?
	mstar_writew(0x0020, MIU_ANA + MIU_ANA_50);
	mstar_writew(0x6000, MIU_DIG + MIU_DIG_PROTECT2_START);

	// deleting this causing a lock up when reading.
	mstar_writew(0x3, 0x1f202644);
	mstar_writew(0x0, 0x1f20267c);
	mstar_writew(0x909, 0x1f202680);
	mstar_writew(0x71e, 0x1f202684);
	mstar_writew(0x2707, 0x1f202688);
	mstar_writew(0x0908, 0x1f20268c);
	mstar_writew(0x0905, 0x1f202690);
	mstar_writew(0x0304, 0x1f202694);
	mstar_writew(0x0528, 0x1f202698);
	mstar_writew(0x0046, 0x1f20269c);
	mstar_writew(0xe000, 0x1f2026a0);
	mstar_writew(0x0000, 0x1f2026a4);
	mstar_writew(0x0900, 0x1f2026a8);
	mstar_writew(0x0000, 0x1f202700);
	mstar_writew(0x0000, 0x1f20270c);
	mstar_writew(0x0000, 0x1f2027fc);
	//

	mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C0);
	mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C4);
	mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C8);
	mstar_writew(0x0030, MIU_EXTRA + MIU_EXTRA_CC);

	// clock wave form
	mstar_writew(0xaaaa, MIU_ANA + MIU_ANA_04);
	mstar_writew(0x0, MIU_ANA + MIU_ANA_08);
	// more timing
	mstar_writew(0x85, MIU_ANA + MIU_ANA_1c);
	// reserved , m5 is 2222
	mstar_writew(config->ana_5c, MIU_ANA + MIU_ANA_5c);

	mstar_writew(config->ana_128, MIU_ANA + MIU_ANA_128);

	mstar_writew(0x0304, 0x1f202b00);
	mstar_writew(0x0200, 0x1f202b04);
	mstar_writew(0x0404, 0x1f202b08);
	mstar_writew(0x0304, 0x1f202b0c);
	mstar_writew(0x0201, 0x1f202b10);
	mstar_writew(0x0101, 0x1f202b14);
	mstar_writew(0x0101, 0x1f202b18);
	mstar_writew(0x0303, 0x1f202b1c);

	mstar_writew(0x0, MIU_ANA + MIU_ANA_16C);

	mstar_writew(0x0002, MIU_ANA + MIU_ANA_1B8);
	mstar_writew(0x0011, MIU_ANA + MIU_ANA_1C0);
	mstar_writew(0x0000, MIU_ANA + MIU_ANA_1C4);
	mstar_writew(0x0010, MIU_ANA + MIU_ANA_1C8);
	mstar_writew(0x1111, MIU_ANA + MIU_ANA_1CC);
	mstar_writew(0x1111, MIU_ANA + MIU_ANA_1D0);
	mstar_writew(0x1111, MIU_ANA + MIU_ANA_1D4);
	mstar_writew(0x1111, MIU_ANA + MIU_ANA_1D8);
	mstar_writew(0x1111, MIU_ANA + MIU_ANA_1DC);
	mstar_writew(0x3333, MIU_ANA + MIU_ANA_1E0);
	mstar_writew(0x0033, MIU_ANA + MIU_ANA_1E4);

	mstar_writew(0x0000, 0x1f202a00);
	mstar_writew(0x0000, 0x1f202a08);
	mstar_writew(0x0000, 0x1f202a10);
	mstar_writew(0x0000, 0x1f202a14);
	mstar_writew(0x0000, 0x1f202a18);
	mstar_writew(0x0200, 0x1f202a20);
	mstar_writew(0x0000, 0x1f202a24);
	mstar_writew(0x0505, 0x1f202a28);
	mstar_writew(0x0000, 0x1f202a3c);
	mstar_writew(0x0505, 0x1f202a40);
	mstar_writew(0x0505, 0x1f202a44);
	mstar_writew(0x0505, 0x1f202a48);
	mstar_writew(0x0505, 0x1f202a4c);
	mstar_writew(0x0505, 0x1f202a50);
	mstar_writew(0x0505, 0x1f202a54);
	mstar_writew(0x0505, 0x1f202a58);
	mstar_writew(0x0505, 0x1f202a5c);
	mstar_writew(0x0202, 0x1f202ac0);
	mstar_writew(0x0000, 0x1f202ac4);
	mstar_writew(0x0808, 0x1f202ac8);
	mstar_writew(0x0808, 0x1f202acc);


#if 0


	mstar_writew(0x20, MIU_DIG + MIU_DIG_GROUP3_HPMASK);

	// deadlines
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP0_REQ_DEADLINE);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP1_REQ_DEADLINE);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP2_REQ_DEADLINE);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP3_REQ_DEADLINE);
	mstar_writew(0xffff, MIU_EXTRA + MIU_EXTRA_GROUP4_REQ_DEADLINE);
	mstar_writew(0xffff, MIU_EXTRA + MIU_EXTRA_GROUP5_REQ_DEADLINE);
	//

	// request group control
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP0_CTRL);
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP1_CTRL);
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP2_CTRL);
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP3_CTRL);
	mstar_writew(0x8015, MIU_EXTRA + MIU_EXTRA_GROUP4_CTRL);
	mstar_writew(0x8015, MIU_EXTRA + MIU_EXTRA_GROUP5_CTRL);
	//
#endif


}

static int mstar_ddr_doinitialcycle(struct regmap *dig)
{
	int loops;
	uint tmp;

	// clear
	tmp = 0;
	regmap_write(dig, MIU_DIG_CNTRL0, tmp);
	mstar_delay(100);

	// deassert rst
	tmp |= MIU_DIG_CNTRL0_RSTZ;
	regmap_write(dig, MIU_DIG_CNTRL0, tmp);
	mstar_delay(100);

	// assert cs
	tmp |= MIU_DIG_CNTRL0_CS;
	regmap_write(dig, MIU_DIG_CNTRL0, tmp);
	mstar_delay(100);

	// enable the clock
	tmp |= MIU_DIG_CNTRL0_CKE;
	regmap_write(dig, MIU_DIG_CNTRL0, 0xe);
	mstar_delay(100);

	// enable odt and trigger init cycle
	tmp |= MIU_DIG_CNTRL0_INIT_MIU | MIU_DIG_CNTRL0_ODT;
	regmap_write(dig, MIU_DIG_CNTRL0, tmp);
	printf("waiting for init to complete.. ");
	for(loops = 0; loops < 10; loops++){
		regmap_read(dig, MIU_DIG_CNTRL0, &tmp);
		if(tmp & MIU_DIG_CNTRL0_INITDONE){
			printf("done\n");
			return 0;
		}
		mdelay(100);
	}
	printf("failed\n");
	return -1;
}

static void mstar_ddr_test(void)
{
	printf("testing ddr..\n");
	mstar_writereadback_l(0xAAAA5555, MSTAR_DRAM);
	mstar_writereadback_l(0xA5A5A5A5, MSTAR_DRAM + 0x4);
}

static void mstar_ddr_setrequestmasks(const struct ddr_config *config, int g0, int g1, int g2, int g3,
		int g4, int g5, int g6, int g7)
{
	if (g0 >= 0 && config->group0 != NULL)
		mstar_writew(g0, (uint32_t) config->group0 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g1 >= 0 && config->group1 != NULL)
		mstar_writew(g1, (uint32_t) config->group1 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g2 >= 0 && config->group2 != NULL)
		mstar_writew(g2, (uint32_t) config->group2 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g3 >= 0 && config->group3 != NULL)
		mstar_writew(g3, (uint32_t) config->group3 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g4 >= 0 && config->group4 != NULL)
		mstar_writew(g4, (uint32_t) config->group4 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g5 >= 0 && config->group5 != NULL)
		mstar_writew(g5, (uint32_t) config->group5 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g6 >= 0 && config->group6 != NULL)
		mstar_writew(g6, (uint32_t) config->group6 + MIU_DIG_GROUP_REG_MASK_OFF);
	if (g7 >= 0 && config->group7 != NULL)
		mstar_writew(g7, (uint32_t) config->group7 + MIU_DIG_GROUP_REG_MASK_OFF);
}

static void mstar_ddr_setinitrequestmasks(const struct ddr_config *config)
{
	mstar_ddr_setrequestmasks(config, config->group0_init_mask,
			config->group1_init_mask, config->group2_init_mask,
			config->group3_init_mask, config->group4_init_mask,
			config->group5_init_mask, config->group6_init_mask,
			config->group7_init_mask);
}

static void mstar_ddr_setclkfreq(const struct ddr_config *config,
		struct regmap *ana, struct regmap *dig)
{
	uint16_t pllstatus = readw(MIU_DIG + MIU_M5_DIG_PLLSTATUS);

	//ddr clock freq
	mstar_writew(0x400, MIU_ANA + MIU_ANA_6C);
	// ddr clock freq
	mstar_writew(0x2004, MIU_ANA + MIU_ANA_68);

	// not setting this causes the initial cycle to not finish
	mstar_writew(0x1, MIU_ANA + MIU_ANA_114);

	// clock gen freq set
	mstar_writew(0x8000, MIU_ANA + MIU_ANA_60);
	mstar_writew(0x29, MIU_ANA + MIU_ANA_64);
	mstar_delay(100);

	mstar_writew(0x1, MIU_ANA + MIU_ANA_114);

	// only on m5?
	mstar_writew(0x0010, MIU_ANA + MIU_ANA_BC);

	mstar_writew(0x4, MIU_ANA + MIU_ANA_DDRAT_15_0);

	mstar_delay(100);

	mstar_writew(0x114, MIU_ANA + MIU_ANA_58);

	printf("pll status before: %04x", pllstatus);
	pllstatus = readw(MIU_DIG + MIU_M5_DIG_PLLSTATUS);
	printf(" after: %04x\n", pllstatus);
}

void garbage(){
	//mstar_writew(0x5000, MIU_EXTRA + MIU_EXTRA_D0);


	// rd phase timing, m5 is 0
	//mstar_writew(config->rd_phase_timing,
	//		MIU_ANA + MIU_ANA_RD_PHASE_TIMING);

	// added to match dumps
	//mstar_writew(0x0, MIU_ANA + MIU_ANA_20);
	//



	// clock phase select
	//mstar_writew(0x77, MIU_ANA + MIU_ANA_70);

#if 0
	{
	  if (param_1 == 8) {
	    _DAT_1f202074 = 0x7070;
	    _DAT_1f2020e8 = 0x606;
	    _DAT_1f202128 = 0x1818;
	    _DAT_1f202140 = 0x23;
	    _DAT_1f202144 = 0x2333;
	    _DAT_1f202148 = 0x1121;
	    _DAT_1f20214c = 0x3201;
	  }
	  else {
	    _DAT_1f202074 = 0x4040;
	    _DAT_1f2020e8 = 0x202;
	    _DAT_1f202128 = 0x1517;
	    _DAT_1f202140 = 0x14;
	    _DAT_1f202144 = 0x2434;
	    _DAT_1f202148 = 0x1132;
	    _DAT_1f20214c = 0x4311;
	  }
#endif

	// using this block because 20e8 matched
	// phase select
	//mstar_writew(0x4040, MIU_ANA + MIU_ANA_74);



	// ???
	//mstar_writew(0x14, MIU_ANA + MIU_ANA_140);
	//mstar_writew(0x2434, MIU_ANA + MIU_ANA_144);
	//mstar_writew(config->ana_148, MIU_ANA + MIU_ANA_148);
	//mstar_writew(config->ana_14c, MIU_ANA + MIU_ANA_14C);


	// reserved - m5 0200
	//mstar_writew(0x0, MIU_ANA + MIU_ANA_A4);
	// reserved - m5 0
	//mstar_writew(0x1111, MIU_ANA + MIU_ANA_A0);

	// undocumented
	//mstar_writew(0x33, MIU_ANA + MIU_ANA_9C);
	//mstar_writew(0x33, MIU_ANA + MIU_ANA_98);
	//mstar_writew(0x0, MIU_ANA + MIU_ANA_94);
	//mstar_writew(0x77, MIU_ANA + MIU_ANA_90);

	// skew - m5 0
	//mstar_writew(0x1011, MIU_ANA + MIU_ANA_7C);
	// skew
	//mstar_writew(0x9133, MIU_ANA + MIU_ANA_78);

	// ??
	//mstar_writew(config->ana_150, MIU_ANA + MIU_ANA_150);
	//mstar_writew(config->ana_154, MIU_ANA + MIU_ANA_154);
	//mstar_writew(config->ana_158, MIU_ANA + MIU_ANA_158);
	//mstar_writew(config->ana_15c, MIU_ANA + MIU_ANA_15C);



	//mstar_writew(config->ana_170, MIU_ANA + MIU_ANA_170);
	//mstar_writew(config->ana_174, MIU_ANA + MIU_ANA_174);
	//mstar_writew(config->ana_178, MIU_ANA + MIU_ANA_178);
	//mstar_writew(config->ana_17c, MIU_ANA + MIU_ANA_17C);

	//mstar_writew(0x4444, MIU_ANA + MIU_ANA_1A0);
	//mstar_writew(0x4444, MIU_ANA + MIU_ANA_1A4);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1A8);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1AC);
	//mstar_writew(0x54, MIU_ANA + MIU_ANA_1B0);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C0);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C4);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C8);
	//mstar_writew(0x5555, MIU_ANA + MIU_ANA_1CC);
	//mstar_writew(0x55, MIU_ANA + MIU_ANA_1D0);



	//mstar_writew(0x0, MIU_ANA + MIU_ANA_130);
	//mstar_writew(0x0, MIU_ANA + MIU_ANA_134);
	//mstar_writew(config->ana_120, MIU_ANA + MIU_ANA_120);

	//mstar_writew(0x8021, MIU_DIG + MIU_DIG_ADDR_BAL_SEL);
	//mstar_writew(0x951a, MIU_DIG + MIU_DIG_PTN_DATA);


	// hard coded version of the above
	// block and the "something to do with ddr" function

	// not documented
	//mstar_writew(0x1f1f, MIU_ANA + MIU_ANA_B0);
	// not documented
	//mstar_writew(0x0000, MIU_ANA + MIU_ANA_B4);

	// drv
	//mstar_writew(0x0000, MIU_ANA + MIU_ANA_B8);


	// pattern data
	// not setting this cases test to lock up

//#if 0
//	  something_to_dowith_ddr();
//#endif



	// test register
//	mstar_writew(0x0, MIU_ANA + MIU_ANA_30);

	// reserved -
//	mstar_writew(0x0, MIU_ANA + MIU_ANA_F8);

	// drv
//	mstar_writew(0x4000, MIU_ANA + MIU_ANA_A8);

	// read crc
//	mstar_writew(0x5, MIU_ANA + MIU_ANA_3C);

//	mstar_writew(0x5, MIU_ANA + MIU_ANA_3C);

//	mstar_writew(0xfffa, MIU_EXTRA + MIU_EXTRA_GROUP6_REQ_MASK);
//	mstar_writew(0x7ffe, MIU_DIG + MIU_DIG_GROUP0_REQ_MASK);


//	mstar_writew(0x1f, MIU_DIG + MIU_DIG_MIUSEL0);

// setting this changes the read back value
	mstar_writew(0x80e1, MIU_DIG + MIU_DIG_R_READ_CRC);

}

static void mstar_ddr_analogconfig(const struct ddr_config *config)
{
	mstar_writew(0x800, MIU_ANA + MIU_ANA_E0);

	// m5 only
	mstar_writew(0x0404, 0x1f202b40);
	mstar_writew(0x0404, 0x1f202b44);
	mstar_writew(0x0404, 0x1f202b48);
	mstar_writew(0x0404, 0x1f202b4c);
	mstar_writew(0x0a0a, 0x1f202b50);
	mstar_writew(0x0404, 0x1f202b54);
	mstar_writew(0x0404, 0x1f202b58);
	mstar_writew(0x0404, 0x1f202b5c);
	mstar_writew(0x0404, 0x1f202b60);
	mstar_writew(0x0a0a, 0x1f202b64);
	// reserved m5 0707
	mstar_writew(config->ana_d8, MIU_ANA + MIU_ANA_D8);
	// phase delay select, m5 0707
	mstar_writew(config->ana_dc, MIU_ANA + MIU_ANA_DC);

	// rec trig
	mstar_writew(0x202, MIU_ANA + MIU_ANA_E8);
	mstar_writew(config->ana_ec, MIU_ANA + MIU_ANA_EC);
	mstar_writew(0x20, MIU_ANA + MIU_ANA_38);

	// ptn mode m5 8020
	mstar_writew(config->ptn_mode, MIU_ANA + MIU_ANA_34);

	// rx en
	mstar_writew(0x3f, MIU_ANA + MIU_ANA_10);
}

static void mstar_ddr_powerupana(void)
{
	mstar_writew(0x2010, MIU_ANA + MIU_ANA_00);
	mstar_writew(0x0000, MIU_ANA + MIU_ANA_00);
	mstar_writew(0x0000, MIU_ANA + MIU_ANA_30);
	mstar_writew(0x0000, MIU_ANA + MIU_ANA_F8);
	mstar_writew(0x4000, MIU_ANA + MIU_ANA_A8);
	mstar_writew(0x0005, MIU_ANA + MIU_ANA_3C);
	mstar_writew(0x000f, MIU_ANA + MIU_ANA_3C);
	mstar_writew(0x0005, MIU_ANA + MIU_ANA_3C);
	// power up ana
	mstar_writew(0x1, MIU_ANA + MIU_ANA_00);
}

static void mstar_ddr3_init(void)
{
	printf("doing DDR3 init\n");
}

static void mstar_miu_init(void)
{
	// ana registers
	mstar_writew(0x0002, MIU_ANA + MIU_ANA_1C0);
	mstar_writew(0x001e, MIU_ANA + MIU_ANA_1C4);
	mstar_writew(0x0018, MIU_ANA + MIU_ANA_1D0);
	mstar_writew(0xffcd, MIU_ANA + MIU_ANA_1F0);

	// group 0 setup
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP0_CTRL);
	mstar_writew(0x2008, MIU_DIG + MIU_DIG_GROUP0_CONFIG0);
	mstar_writew(0x0400, MIU_DIG + MIU_DIG_GROUP0_TIMEOUT);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP0_HPMASK);
	mstar_writew(0x3210, MIU_DIG + MIU_DIG_GROUP0_REQ_PRIORITY0);
	mstar_writew(0x7654, MIU_DIG + MIU_DIG_GROUP0_REQ_PRIORITY1);
	mstar_writew(0x0000, MIU_DIG + MIU_DIG_GROUP0_REQ_DEADLINE);

	// group 1 setup
	mstar_writew(0x8000, MIU_DIG + MIU_DIG_GROUP1_CTRL);
	mstar_writew(0x3010, MIU_DIG + MIU_DIG_GROUP1_CONFIG0);
	mstar_writew(0x0400, MIU_DIG + MIU_DIG_GROUP1_TIMEOUT);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP1_HPMASK);
	mstar_writew(0x3210, MIU_DIG + MIU_DIG_GROUP1_REQ_PRIORITY0);
	mstar_writew(0x7654, MIU_DIG + MIU_DIG_GROUP1_REQ_PRIORITY1);
	mstar_writew(0xba98, MIU_DIG + MIU_DIG_GROUP1_REQ_PRIORITY2);
	mstar_writew(0xfedc, MIU_DIG + MIU_DIG_GROUP1_REQ_PRIORITY3);

	// group 2 setup
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP2_CTRL);
	mstar_writew(0x2008, MIU_DIG + MIU_DIG_GROUP2_CONFIG0);
	mstar_writew(0x0400, MIU_DIG + MIU_DIG_GROUP2_TIMEOUT);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP1_HPMASK);
	mstar_writew(0x3210, MIU_DIG + MIU_DIG_GROUP2_REQ_PRIORITY0);
	mstar_writew(0x7654, MIU_DIG + MIU_DIG_GROUP2_REQ_PRIORITY1);
	mstar_writew(0xba98, MIU_DIG + MIU_DIG_GROUP2_REQ_PRIORITY2);
	mstar_writew(0xfedc, MIU_DIG + MIU_DIG_GROUP2_REQ_PRIORITY3);

	// group 3 setup
	mstar_writew(0x8015, MIU_DIG + MIU_DIG_GROUP3_CTRL);
	mstar_writew(0x2008, MIU_DIG + MIU_DIG_GROUP3_CONFIG0);
	mstar_writew(0x0400, MIU_DIG + MIU_DIG_GROUP3_TIMEOUT);
	mstar_writew(0xffff, MIU_DIG + MIU_DIG_GROUP3_HPMASK);
	mstar_writew(0x3210, MIU_DIG + MIU_DIG_GROUP3_REQ_PRIORITY0);
	mstar_writew(0x7654, MIU_DIG + MIU_DIG_GROUP3_REQ_PRIORITY1);
	mstar_writew(0xba98, MIU_DIG + MIU_DIG_GROUP3_REQ_PRIORITY2);
	mstar_writew(0xfedc, MIU_DIG + MIU_DIG_GROUP3_REQ_PRIORITY3);

	// something else
	mstar_writew(0x81d2, MIU_DIG + MIU_DIG_R_READ_CRC);
	mstar_writew(0x0, MIU_EXTRA + MIU_EXTRA_1D0);
	mstar_writew(0x4008, MIU_EXTRA + MIU_EXTRA_1D4);
	mstar_writew(0x0202, MIU_EXTRA + MIU_EXTRA_1D8);

	// back to group 0
	mstar_writew(0xba98, MIU_DIG + MIU_DIG_GROUP0_REQ_PRIORITY2);
	mstar_writew(0xfedc, MIU_DIG + MIU_DIG_GROUP0_REQ_PRIORITY3);
	mstar_writew(0x0000, MIU_DIG + MIU_DIG_GROUP0_REQ_LIMITMASK);

	// back to group 1
	mstar_writew(0x001d, MIU_DIG + MIU_DIG_GROUP1_CTRL);
}

void mstar_the_return_of_miu(void)
{
#if 0
	  _DAT_1f202440 = 0xff;
	  DAT_1f20245c = 0x70;
	  DAT_1f20245d = 0xf;
	  DAT_1f202460 = 9;
	  DAT_1f202461 = 0x1f;
	  DAT_1f202464 = 0x1e;
	  DAT_1f202465 = 8;
	  DAT_1f202468 = 0x26;
	  DAT_1f202469 = 0xc;
	  _DAT_1f202580 = 0;
	  _DAT_1f202584 = 0xff;
	  _DAT_1f2025a4 = _DAT_1f2025a4 | 1;
	  _DAT_1f2025bc = _DAT_1f2025bc | 2;
#endif
}



static void mstar_ddr_unmask_bist(struct ddr_config *config)
{
	mstar_ddr_setrequestmasks(config, 0x7fff, -1, -1, -1, -1, -1, -1, 0xfffa);
}

static void mstar_ddr_setdone(struct ddr_config *config)
{
	uint16_t temp;
	/* if this is not cleared any access to the DDR locks the CPU */
	temp = readw(MIU_DIG + MIU_DIG_SW_RST);
	temp |= 0xc00;
	temp |= MIU_DIG_SW_RST_SW_INIT_DONE;
	mstar_writew(temp, MIU_DIG + MIU_DIG_SW_RST);
}

static void mstar_ddr_init_i3_ipl(void)
{
/*	uint16_t pmlock, efuse_14;
	efuse_14 = readw_relaxed(EFUSE + EFUSE_14);
	pmlock = readw_relaxed(PMSLEEP + PMSLEEP_LOCK);

	printf("efuse: %04x\n", efuse_14);
	printf("pmlock: %04x\n", pmlock);

	printf("doing ddr setup, hold onto your pants...\n");

	mstar_ddr2_init();
	mstar_miu_init();
	mstar_the_return_of_miu();
	cpu_clk_setup();
	mstar_ddr_unmask_setdone();*/
}

static int mstar_ddr_getconfig(int chiptype, struct ddr_config *config)
{
	uint16_t type;

	printf("trying to work out DDR config..\n");

	switch(chiptype){
	case CHIPTYPE_MSC313E:
		config->group0 = (void*) MIU_DIG + MIU_DIG_GROUP0_CTRL;
		config->group1 = (void*) MIU_DIG + MIU_DIG_GROUP1_CTRL;
		config->group2 = (void*) MIU_DIG + MIU_DIG_GROUP2_CTRL;
		config->group3 = (void*) MIU_DIG + MIU_DIG_GROUP3_CTRL;
		config->group4 = (void*) MIU_EXTRA + MIU_EXTRA_GROUP4_CTRL;
		config->group5 = (void*) MIU_EXTRA + MIU_EXTRA_GROUP5_CTRL;
		// I think this might be something else
		config->group6 = (void*) MIU_EXTRA + MIU_EXTRA_GROUP6_CTRL;
		config->group7 = NULL;

		config->group0_init_mask = 0xfffe;
		config->group1_init_mask = 0xffff;
		config->group2_init_mask = 0xffff;
		config->group3_init_mask = 0xffff;
		config->group4_init_mask = 0xffff;
		config->group5_init_mask = 0xffff;
		config->group6_init_mask = 0xfffe;

		config->dig_config2 = 0x1b28;
		break;

	case CHIPTYPE_SSC8336:
	case CHIPTYPE_SSC8336N:
		type = readw(GPIO + GPIO_18);
		printf("mystery gpio register is %02x\n", type);
		type &= GPIO_18_D9_DDRMASK;

		switch (chiptype) {
		case CHIPTYPE_SSC8336:
			switch (type) {
			case 0:
				config->size = 0x2000000; // 32MB
				break;
			case 1:
				config->size = 0x4000000; // 64MB
				config->type = MSTAR_DRAM_DDR2;
				break;
			case 2:
				config->size = 0x8000000; // 128MB
				break;
			case 4:
				config->size = 0x10000000; // 256MB
				break;
			default:
				printf("Don't know how to workout the DDR size for %02x\n",type);
				break;
			}
			break;
		case CHIPTYPE_SSC8336N:
			switch (type) {
			case 0xf:
				config->size = 0x4000000; // 64MB
				config->type = MSTAR_DRAM_DDR2;
				break;
			}
			break;
		}



		config->group0 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP0_CTRL;
		config->group1 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP1_CTRL;
		config->group2 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP2_CTRL;
		config->group3 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP3_CTRL;
		config->group4 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP4_CTRL;
		config->group5 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP5_CTRL;
		config->group6 = (void*) MIU_M5_GROUPS + MIU_M5_GROUPS_GROUP6_CTRL;
		config->group7 = (void*) MIU_EXTRA + MIU_EXTRA_GROUP6_CTRL;

		config->group0_init_mask = 0xfffe;
		config->group1_init_mask = 0xffff;
		config->group2_init_mask = 0xffff;
		config->group3_init_mask = 0xffff;
		config->group4_init_mask = 0xffff;
		config->group5_init_mask = 0xffff;
		config->group6_init_mask = 0xffff;
		config->group7_init_mask = 0xfffe;

		config->dig_config2 = 0x1828;

		config->mr0 = 0x3;
		config->mr1 = 0x4004;
		config->mr2 = 0x8000;
		config->mr3 = 0xc000;

		config->rd_phase_timing = 0x0;

		config->ptn_mode = 0x8020;

		config->ana_5c = 0x2222;

		config->ana_d8 = 0x707;
		config->ana_dc = 0x707;

		config->ana_ec = 0x707;

		config->ana_120 = 0xf0f3;
		config->ana_128 = 0x2829;

		config->ana_148 = 0;
		config->ana_14c = 0;

		config->ana_150 = 0;
		config->ana_154 = 0;
		config->ana_158 = 0;
		config->ana_15c = 0x202;

		config->ana_170 = 0;
		config->ana_174 = 0;
		config->ana_178 = 0;
		config->ana_17c = 0;
		break;
	default:
		printf("Don't know how to find DRAM config for chiptype %i\n", chiptype);
		return -EINVAL;
	}

	printf("Detected DRAM: %08x bytes\n", config->size);

	return 0;
}

void mstar_ddr_maybeanareset(struct regmap *ana)
{
	// drive cal software mode
	regmap_write(ana, MIU_ANA_F0, 1);
	mstar_delay(1000);
	regmap_write(ana, MIU_ANA_DDRAT_23_16, 0x1000);
	mstar_delay(1000);
	regmap_write(ana, MIU_ANA_DDRAT_23_16, 0);
	mstar_delay(1000);
}

void mstar_ddr_init(int chiptype, struct regmap *ana, struct regmap *dig)
{
	struct ddr_config config;

	if (mstar_ddr_getconfig(chiptype, &config))
		goto out;

	mstar_ddr_dig_rst(dig);
	mstar_ddr_setinitrequestmasks(&config);
	mstar_ddr_maybeanareset(ana);
	mstar_ddr_setclkfreq(&config, ana, dig);
	printf("-- 2 --\n");
	mstar_ddr_setdigconfig(&config, dig);
	printf("-- 3 -- \n");
	// ps cycle
	mstar_writew(0x7f, MIU_ANA + MIU_ANA_C4);
	// dll code
	mstar_writew(0xf000, MIU_ANA + MIU_ANA_C8);
	// m5 only?
	mstar_writew(0x00cb, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00cf, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00cb, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00c3, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00cb, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00c3, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00cb, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00c2, MIU_ANA + MIU_ANA_C0);
	mstar_writew(0x00c0, MIU_ANA + MIU_ANA_C0);
	// --
	mstar_writew(0x33c8, MIU_ANA + MIU_ANA_C0);

	mstar_writew(0x0000, 0x1f2020e0);
	mstar_writew(0x0000, 0x1f202130);
	mstar_writew(0x0000, 0x1f202134);
	mstar_writew(0xf0f3, 0x1f202120);
	mstar_writew(0x0800, 0x1f2020e0);
	mstar_writew(0x8000, 0x1f2027bc);
	mstar_writew(0x8221, 0x1f202458);
	mstar_writew(0x61a1, 0x1f2025f8);
	mstar_writew(0x0300, 0x1f202714);
	mstar_writew(0x80f0, 0x1f202700);
	mstar_writew(0xc01d, 0x1f202c60);
	mstar_writew(0xc01d, 0x1f202ce0);
	mstar_writew(0xc01d, 0x1f202d60);
	mstar_writew(0xc01d, 0x1f202de0);
	mstar_writew(0xc01d, 0x1f202e60);
	mstar_writew(0x001d, 0x1f202ee0);
	mstar_writew(0x001d, 0x1f202f60);


	printf("-- 4 --\n");
	mstar_ddr_analogconfig(&config);
	mstar_ddr_dig_rst_release(dig);
	printf("-- 5 --\n");
	mstar_ddr_powerupana();
	if(mstar_ddr_doinitialcycle(dig))
		goto out;

	mstar_ddr_setdone(&config);
	mstar_ddr_unmask_bist(&config);

	mstar_writew(0x8000, 0x1f2025fc);
	mstar_writew(0x200, 0x1f2025ac);
	mstar_writew(0x000, 0x1f2025e0);

	printf("-----\n");

	mstar_delay(1000);



	mstar_ddr_setrequestmasks(&config, 0, 0, 0, 0, 0, 0, 0, 0);

	// protection size?
	//mstar_writew(0x6000, 0x1f2025a4);

	//mstar_dump_reg_block("miu_dig", MIU_DIG);
	//mstar_dump_reg_block("miu_dig", MIU_DIG + 0x200);


	//mstar_miu_init();
	//mstar_the_return_of_miu();



	mstar_writew(0x8, CLKGEN + CLKGEN_MIU);
	mstar_writew(0x18, CLKGEN + CLKGEN_MIU);
	mstar_writew(0x4, CLKGEN + CLKGEN_MIU_BOOT);

	// This will lock up until the cpu clock is setup.
	//mstar_ddr_test();

out:
	return;
}
