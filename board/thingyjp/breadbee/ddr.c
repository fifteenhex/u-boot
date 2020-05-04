/*
 * ddr.c
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"

static void mstar_ddr_rst(void)
{
	// what is the 0xc00 part?
	writew(MIU_DIG_SW_RST_MIU | 0xc00, MIU_DIG + MIU_DIG_SW_RST);
}

static void mstar_ddr_setrequestmasks(u16 g0, u16 g1, u16 g2, u16 g3,
		u32 g4, u32 g5, u32 g6)
{
	writew(g0, MIU_DIG + MIU_DIG_GROUP0_REQ_MASK);
	writew(g1, MIU_DIG + MIU_DIG_GROUP1_REQ_MASK);
	writew(g2, MIU_DIG + MIU_DIG_GROUP2_REQ_MASK);
	writew(g3, MIU_DIG + MIU_DIG_GROUP3_REQ_MASK);
	writew(g4, MIU_EXTRA + MIU_EXTRA_GROUP4_REQ_MASK);
	writew(g5, MIU_EXTRA + MIU_EXTRA_GROUP5_REQ_MASK);
	// I think this might be something else
	writew(g6, MIU_EXTRA + MIU_EXTRA_GROUP6_REQ_MASK);
}

static void mstar_ddr2_init(void)
{
	printf("doing DDR2 init\n");
	mstar_ddr_rst();
	mstar_ddr_setrequestmasks(0xfffe, 0xffff, 0xffff,
			0xffff, 0xffff, 0xffff, 0xfffe);
	writew(1, MIU_ANA + MIU_ANA_F0);
	mdelay(100);
	writew(0x1000, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);
	writew(0, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);

	//wtf is this
	writew(0x400, MIU_ANA + MIU_ANA_6C);
	writew(0x2004, MIU_ANA + MIU_ANA_68);
	writew(0x1, MIU_ANA + MIU_ANA_114);
	writew(0x8000, MIU_ANA + MIU_ANA_60);
	writew(0x29, MIU_ANA + MIU_ANA_64);
	mdelay(100);

	writew(0x4, MIU_ANA + MIU_ANA_DDRAT_15_0);
	writew(0x114, MIU_ANA + MIU_ANA_58);

	// 4 banks, 9 cols - 32MB?
#if 0
	  if (param_1 == 0) {
	                    /* ddr2, 16 bit 4 banks, 9 cols. data ratio 8x */
	    _miu_ddr_busconfig = 0x352;
	    _miu_ddr_timing0 = 0x2775;
	    _DAT_1f202684 = 0x51e;
	    _DAT_1f2025a4 = 0x5000;
	  }
#endif

	// 4 banks, 10 cols - 64MB?
	  writew(0x0392, MIU_DIG + MIU_DIG_CONFIG0);
	  writew(0x2777, MIU_DIG + MIU_DIG_TIMING1);
	  writew(0x6000, MIU_DIG + MIU_DIG_PROTECT2_START);
	  // mm?
	  writew(0x71e, 0x1f202684);
	  writew(0x0, 0x1f20267c);
	  writew(0x909, 0x1f202680);
	  writew(0x3, 0x1f202644);


	  writew(0x20, MIU_DIG + MIU_DIG_GROUP3_HPMASK);
	  writew(0xc000, MIU_DIG + MIU_DIG_MR3);
	  writew(0x8000, MIU_DIG + MIU_DIG_MR2);
	  writew(0x4004, MIU_DIG + MIU_DIG_MR1);
	  writew(0x3, MIU_DIG + MIU_DIG_MR0);
	  writew(0x4046, MIU_DIG + MIU_DIG_TIMING3);
	  writew(0x9598, MIU_DIG + MIU_DIG_TIMING2);
	  writew(0x1e99, MIU_DIG + MIU_DIG_TIMING0);

	  writew(0x1b28, MIU_DIG + MIU_DIG_CONFIG2);
	  writew(0xd, MIU_DIG + MIU_DIG_CONFIG1);

	  writew(0x2707, 0x1f202688);
	  writew(0x0908, 0x1f20268c);
	  writew(0x0905, 0x1f202690);
	  writew(0x0304, 0x1f202694);
	  writew(0x0528, 0x1f202698);
	  writew(0x0046, 0x1f20269c);
	  writew(0xe000, 0x1f2026a0);
	  writew(0x0000, 0x1f2026a4);
	  writew(0x0900, 0x1f2026a8);

	  writew(0x0000, 0x1f202700);
	  writew(0x0000, 0x1f20270c);
	  writew(0x0000, 0x1f2027fc);

	  writew(0x0000, MIU_EXTRA + MIU_EXTRA_C0);
	  writew(0x0000, MIU_EXTRA + MIU_EXTRA_C4);
	  writew(0x0000, MIU_EXTRA + MIU_EXTRA_C8);
	  writew(0x0030, MIU_EXTRA + MIU_EXTRA_CC);
	  writew(0x5000, MIU_EXTRA + MIU_EXTRA_D0);


	  writew(0xaaaa, MIU_ANA + MIU_ANA_04);
	  writew(0x0, MIU_ANA + MIU_ANA_08);
	  writew(0x1100, MIU_ANA + MIU_ANA_14);
	  writew(0x85, MIU_ANA + MIU_ANA_1c);
	  writew(0x1112, MIU_ANA + MIU_ANA_5c);
	  writew(0x77, MIU_ANA + MIU_ANA_70);

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

	  writew(0x808, MIU_ANA + MIU_ANA_DC);
	  writew(0x808, MIU_ANA + MIU_ANA_D8);
	  writew(0x0, MIU_ANA + MIU_ANA_A4);
	  writew(0x1111, MIU_ANA + MIU_ANA_A0);
	  writew(0x33, MIU_ANA + MIU_ANA_9C);
	  writew(0x33, MIU_ANA + MIU_ANA_98);
	  writew(0x0, MIU_ANA + MIU_ANA_94);
	  writew(0x77, MIU_ANA + MIU_ANA_90);
	  writew(0x1011, MIU_ANA + MIU_ANA_7C);
	  writew(0x9133, MIU_ANA + MIU_ANA_78);
	  writew(0x1111, MIU_ANA + MIU_ANA_150);
	  writew(0x1111, MIU_ANA + MIU_ANA_154);
	  writew(0x1111, MIU_ANA + MIU_ANA_158);
	  writew(0x1111, MIU_ANA + MIU_ANA_15C);
	  writew(0x0, MIU_ANA + MIU_ANA_16C);
	  writew(0x1111, MIU_ANA + MIU_ANA_170);
	  writew(0x111, MIU_ANA + MIU_ANA_174);
	  writew(0x111, MIU_ANA + MIU_ANA_178);
	  writew(0x111, MIU_ANA + MIU_ANA_17C);

	  writew(0x4444, MIU_ANA + MIU_ANA_1A0);
	  writew(0x4444, MIU_ANA + MIU_ANA_1A4);
	  writew(0x5555, MIU_ANA + MIU_ANA_1A8);
	  writew(0x5555, MIU_ANA + MIU_ANA_1AC);

	  writew(0x54, MIU_ANA + MIU_ANA_1B0);


	  writew(0x5555, MIU_ANA + MIU_ANA_1C0);
	  writew(0x5555, MIU_ANA + MIU_ANA_1C4);
	  writew(0x5555, MIU_ANA + MIU_ANA_1C8);
	  writew(0x5555, MIU_ANA + MIU_ANA_1CC);
	  writew(0x55, MIU_ANA + MIU_ANA_1D0);

	  writew(0x7f, MIU_ANA + MIU_ANA_C4);
	  writew(0xf000, MIU_ANA + MIU_ANA_C8);
	  writew(0x33c8, MIU_ANA + MIU_ANA_C0);

	  writew(0x0, MIU_ANA + MIU_ANA_130);
	  writew(0x0, MIU_ANA + MIU_ANA_134);
	  writew(0xf0f1, MIU_ANA + MIU_ANA_120);

	  writew(0x8021, MIU_DIG + MIU_DIG_ADDR_BAL_SEL);
	  writew(0x951a, MIU_DIG + MIU_DIG_PTN_DATA);

	  // deadlines
	  writew(0xffff, MIU_DIG + MIU_DIG_GROUP0_REQ_DEADLINE);
	  writew(0xffff, MIU_DIG + MIU_DIG_GROUP1_REQ_DEADLINE);
	  writew(0xffff, MIU_DIG + MIU_DIG_GROUP2_REQ_DEADLINE);
	  writew(0xffff, MIU_DIG + MIU_DIG_GROUP3_REQ_DEADLINE);
	  writew(0xffff, MIU_EXTRA + MIU_EXTRA_GROUP4_REQ_DEADLINE);
	  writew(0xffff, MIU_EXTRA + MIU_EXTRA_GROUP5_REQ_DEADLINE);
	  //

	  // request group control
	  writew(0x8015, MIU_DIG + MIU_DIG_GROUP0_CTRL);
	  writew(0x8015, MIU_DIG + MIU_DIG_GROUP1_CTRL);
	  writew(0x8015, MIU_DIG + MIU_DIG_GROUP2_CTRL);
	  writew(0x8015, MIU_DIG + MIU_DIG_GROUP3_CTRL);
	  writew(0x8015, MIU_EXTRA + MIU_EXTRA_GROUP4_CTRL);
	  writew(0x8015, MIU_EXTRA + MIU_EXTRA_GROUP5_CTRL);
	  //

	  writew(0x1, MIU_ANA + MIU_ANA_114);
	  writew(0x800, MIU_ANA + MIU_ANA_E0);


	  #if 0
	  if (param_1 == 8) {
	    _DAT_1f2020b4 = 0x7777;
	    _DAT_1f2020b8 = 0x7777;
	    _DAT_1f2020bc = 0x7777;
	  }
	  else {
	    _DAT_1f2020b4 = 0x5555;
	    _DAT_1f2020b8 = 0x7755;
	    _DAT_1f2020bc = 0x7755;
	  }
	#endif

	  writew(0x8111, MIU_ANA + MIU_ANA_34);
	  writew(0x20, MIU_ANA + MIU_ANA_38);
#if 0
	  something_to_dowith_ddr();
#endif
	  writew(0x3f, MIU_ANA + MIU_ANA_10);

	  writew(0x8c00, MIU_DIG + MIU_DIG_SW_RST);

	  writew(0x0, MIU_ANA + MIU_ANA_30);
	  writew(0x0, MIU_ANA + MIU_ANA_F8);
	  writew(0x4000, MIU_ANA + MIU_ANA_A8);
	  writew(0x5, MIU_ANA + MIU_ANA_3C);
	  writew(0x1, MIU_ANA + MIU_ANA_00);


	  writew(0x0, MIU_DIG + MIU_DIG_CNTRL0);
	  mdelay(100);
	  writew(0xc, MIU_DIG + MIU_DIG_CNTRL0);
	  mdelay(100);
	  writew(0xe, MIU_DIG + MIU_DIG_CNTRL0);
	  mdelay(100);
	  writew(0x1f, MIU_DIG + MIU_DIG_CNTRL0);
	  mdelay(1000);

	  writew(0x5, MIU_ANA + MIU_ANA_3C);

	  writew(0xfffa, MIU_EXTRA + MIU_EXTRA_GROUP6_REQ_MASK);
	  writew(0x7ffe, MIU_DIG + MIU_DIG_GROUP0_REQ_MASK);

	  writew(0x1f, MIU_DIG + MIU_DIG_MIUSEL0);
	  writew(0x80e1, MIU_DIG + MIU_DIG_R_READ_CRC);
}

static void mstar_ddr3_init(void)
{
	printf("doing DDR3 init\n");
}

void mstar_ddr_init()
{
	uint16_t pmlock, efuse_14;
	efuse_14 = readw_relaxed(EFUSE + EFUSE_14);
	pmlock = readw_relaxed(PMSLEEP + PMSLEEP_LOCK);

	printf("efuse: %04x\n", efuse_14);
	printf("pmlock: %04x\n", pmlock);
	printf("doing ddr setup, hold onto your pants...\n");

	mstar_ddr2_init();
}

