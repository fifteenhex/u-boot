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
	mstar_writew(MIU_DIG_SW_RST_MIU | 0xc00, MIU_DIG + MIU_DIG_SW_RST);
}

static void mstar_ddr_doinitialcycle(void)
{
	uint16_t temp;

	// clear
	temp = 0;
	mstar_writew(temp, MIU_DIG + MIU_DIG_CNTRL0);
	mdelay(100);

	// assert cs, deassert rst
	temp |= MIU_DIG_CNTRL0_CS | MIU_DIG_CNTRL0_RSTZ;
	mstar_writew(temp, MIU_DIG + MIU_DIG_CNTRL0);
	mdelay(100);

	// enable the clock
	temp |= MIU_DIG_CNTRL0_CKE;
	mstar_writew(0xe, MIU_DIG + MIU_DIG_CNTRL0);
	mdelay(100);

	// enable odt and trigger init cycle
	temp |= MIU_DIG_CNTRL0_INIT_MIU | MIU_DIG_CNTRL0_ODT;
	mstar_writew(temp, MIU_DIG + MIU_DIG_CNTRL0);
	mdelay(1000);

	temp = readw(MIU_DIG + MIU_DIG_CNTRL0);
	printf("cntrl %04x\n", temp);
}

static void mstar_ddr_test(void)
{
	printf("testing ddr..\n");
	mstar_writereadback_l(0xAAAA5555, MSTAR_DRAM);
}

static void mstar_ddr_setrequestmasks(u16 g0, u16 g1, u16 g2, u16 g3,
		u32 g4, u32 g5, u32 g6)
{
	mstar_writew(g0, MIU_DIG + MIU_DIG_GROUP0_REQ_MASK);
	mstar_writew(g1, MIU_DIG + MIU_DIG_GROUP1_REQ_MASK);
	mstar_writew(g2, MIU_DIG + MIU_DIG_GROUP2_REQ_MASK);
	mstar_writew(g3, MIU_DIG + MIU_DIG_GROUP3_REQ_MASK);
	mstar_writew(g4, MIU_EXTRA + MIU_EXTRA_GROUP4_REQ_MASK);
	mstar_writew(g5, MIU_EXTRA + MIU_EXTRA_GROUP5_REQ_MASK);
	// I think this might be something else
	mstar_writew(g6, MIU_EXTRA + MIU_EXTRA_GROUP6_REQ_MASK);
}

static void mstar_ddr2_init(void)
{
	printf("doing DDR2 init\n");
	mstar_ddr_rst();
	mstar_ddr_setrequestmasks(0xfffe, 0xffff, 0xffff,
			0xffff, 0xffff, 0xffff, 0xfffe);
	mstar_writew(1, MIU_ANA + MIU_ANA_F0);
	mdelay(100);
	mstar_writew(0x1000, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);
	mstar_writew(0, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);

	//wtf is this
	mstar_writew(0x400, MIU_ANA + MIU_ANA_6C);
	mstar_writew(0x2004, MIU_ANA + MIU_ANA_68);
	mstar_writew(0x1, MIU_ANA + MIU_ANA_114);
	mstar_writew(0x8000, MIU_ANA + MIU_ANA_60);
	mstar_writew(0x29, MIU_ANA + MIU_ANA_64);
	mdelay(100);

	mstar_writew(0x4, MIU_ANA + MIU_ANA_DDRAT_15_0);
	mstar_writew(0x114, MIU_ANA + MIU_ANA_58);

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
	  mstar_writew(0x0392, MIU_DIG + MIU_DIG_CONFIG0);
	  mstar_writew(0x2777, MIU_DIG + MIU_DIG_TIMING1);
	  mstar_writew(0x6000, MIU_DIG + MIU_DIG_PROTECT2_START);
	  // mm?
	  mstar_writew(0x71e, 0x1f202684);
	  mstar_writew(0x0, 0x1f20267c);
	  mstar_writew(0x909, 0x1f202680);
	  mstar_writew(0x3, 0x1f202644);


	  mstar_writew(0x20, MIU_DIG + MIU_DIG_GROUP3_HPMASK);
	  mstar_writew(0xc000, MIU_DIG + MIU_DIG_MR3);
	  mstar_writew(0x8000, MIU_DIG + MIU_DIG_MR2);
	  mstar_writew(0x4004, MIU_DIG + MIU_DIG_MR1);
	  mstar_writew(0x3, MIU_DIG + MIU_DIG_MR0);
	  mstar_writew(0x4046, MIU_DIG + MIU_DIG_TIMING3);
	  mstar_writew(0x9598, MIU_DIG + MIU_DIG_TIMING2);
	  mstar_writew(0x1e99, MIU_DIG + MIU_DIG_TIMING0);

	  mstar_writew(0x1b28, MIU_DIG + MIU_DIG_CONFIG2);
	  mstar_writew(0xd, MIU_DIG + MIU_DIG_CONFIG1);

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

	  mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C0);
	  mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C4);
	  mstar_writew(0x0000, MIU_EXTRA + MIU_EXTRA_C8);
	  mstar_writew(0x0030, MIU_EXTRA + MIU_EXTRA_CC);
	  mstar_writew(0x5000, MIU_EXTRA + MIU_EXTRA_D0);


	  mstar_writew(0xaaaa, MIU_ANA + MIU_ANA_04);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_08);
	  mstar_writew(0x1100, MIU_ANA + MIU_ANA_14);
	  mstar_writew(0x85, MIU_ANA + MIU_ANA_1c);
	  mstar_writew(0x1112, MIU_ANA + MIU_ANA_5c);
	  mstar_writew(0x77, MIU_ANA + MIU_ANA_70);

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
	  mstar_writew(0x4040, MIU_ANA + MIU_ANA_74);
	  mstar_writew(0x202, MIU_ANA + MIU_ANA_E8);
	  mstar_writew(0x1517, MIU_ANA + MIU_ANA_128);
	  mstar_writew(0x14, MIU_ANA + MIU_ANA_140);
	  mstar_writew(0x2434, MIU_ANA + MIU_ANA_144);
	  mstar_writew(0x1132, MIU_ANA + MIU_ANA_148);
	  mstar_writew(0x4311, MIU_ANA + MIU_ANA_14C);

	  mstar_writew(0x808, MIU_ANA + MIU_ANA_DC);
	  mstar_writew(0x808, MIU_ANA + MIU_ANA_D8);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_A4);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_A0);
	  mstar_writew(0x33, MIU_ANA + MIU_ANA_9C);
	  mstar_writew(0x33, MIU_ANA + MIU_ANA_98);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_94);
	  mstar_writew(0x77, MIU_ANA + MIU_ANA_90);
	  mstar_writew(0x1011, MIU_ANA + MIU_ANA_7C);
	  mstar_writew(0x9133, MIU_ANA + MIU_ANA_78);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_150);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_154);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_158);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_15C);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_16C);
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_170);
	  mstar_writew(0x111, MIU_ANA + MIU_ANA_174);
	  mstar_writew(0x111, MIU_ANA + MIU_ANA_178);
	  mstar_writew(0x111, MIU_ANA + MIU_ANA_17C);

	  mstar_writew(0x4444, MIU_ANA + MIU_ANA_1A0);
	  mstar_writew(0x4444, MIU_ANA + MIU_ANA_1A4);
	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1A8);
	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1AC);

	  mstar_writew(0x54, MIU_ANA + MIU_ANA_1B0);


	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C0);
	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C4);
	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1C8);
	  mstar_writew(0x5555, MIU_ANA + MIU_ANA_1CC);
	  mstar_writew(0x55, MIU_ANA + MIU_ANA_1D0);

	  mstar_writew(0x7f, MIU_ANA + MIU_ANA_C4);
	  mstar_writew(0xf000, MIU_ANA + MIU_ANA_C8);
	  mstar_writew(0x33c8, MIU_ANA + MIU_ANA_C0);

	  mstar_writew(0x0, MIU_ANA + MIU_ANA_130);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_134);
	  mstar_writew(0xf0f1, MIU_ANA + MIU_ANA_120);

	  mstar_writew(0x8021, MIU_DIG + MIU_DIG_ADDR_BAL_SEL);
	  mstar_writew(0x951a, MIU_DIG + MIU_DIG_PTN_DATA);

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

	  mstar_writew(0x1, MIU_ANA + MIU_ANA_114);
	  mstar_writew(0x800, MIU_ANA + MIU_ANA_E0);


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

	  // hard coded version of the above
	  // block and the "something to do with ddr" function
	  mstar_writew(0x1f1f, MIU_ANA + MIU_ANA_B0);
	  mstar_writew(0x0000, MIU_ANA + MIU_ANA_B4);
	  mstar_writew(0x0000, MIU_ANA + MIU_ANA_B8);
	  mstar_writew(0x0010, MIU_ANA + MIU_ANA_BC);

	  mstar_writew(0x8111, MIU_ANA + MIU_ANA_34);
	  mstar_writew(0x20, MIU_ANA + MIU_ANA_38);
#if 0
	  something_to_dowith_ddr();
#endif
	  mstar_writew(0x3f, MIU_ANA + MIU_ANA_10);

	  mstar_writew(0x8c00, MIU_DIG + MIU_DIG_SW_RST);

	  mstar_writew(0x0, MIU_ANA + MIU_ANA_30);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_F8);
	  mstar_writew(0x4000, MIU_ANA + MIU_ANA_A8);
	  mstar_writew(0x5, MIU_ANA + MIU_ANA_3C);
	  mstar_writew(0x1, MIU_ANA + MIU_ANA_00);


	  mstar_ddr_doinitialcycle();

	  mstar_writew(0x5, MIU_ANA + MIU_ANA_3C);

	  mstar_writew(0xfffa, MIU_EXTRA + MIU_EXTRA_GROUP6_REQ_MASK);
	  mstar_writew(0x7ffe, MIU_DIG + MIU_DIG_GROUP0_REQ_MASK);

	  mstar_writew(0x1f, MIU_DIG + MIU_DIG_MIUSEL0);
	  mstar_writew(0x80e1, MIU_DIG + MIU_DIG_R_READ_CRC);
}

static void mstar_ddr3_init(void)
{
	printf("doing DDR3 init\n");
}

static void mstar_miu_init(void)
{
#if 0
	  DAT_1f2023c0 = 2;
	  DAT_1f2023c1 = 0;
	  DAT_1f2023c4 = 0x1e;
	  DAT_1f2023c5 = 0;
	  DAT_1f2023d0 = 0x18;
	  DAT_1f2023f0 = 0xcd;
	  DAT_1f2023f1 = 0xff;
	  DAT_1f202480 = 0x15;
	  DAT_1f202481 = 0x80;
	  DAT_1f202484 = 8;
	  DAT_1f202485 = 0x20;
	  DAT_1f202488 = 0;
	  DAT_1f202489 = 4;
	  DAT_1f202490 = 0xff;
	  DAT_1f202491 = 0xff;
	  DAT_1f202494 = 0x10;
	  DAT_1f202495 = 0x32;
	  DAT_1f202498 = 0x54;
	  DAT_1f202499 = 0x76;
	  DAT_1f2024a4 = 0;
	  DAT_1f2024a5 = 0;
	  DAT_1f2024c1 = 0x80;
	  DAT_1f2024c4 = 0x10;
	  DAT_1f2024c5 = 0x30;
	  DAT_1f2024c8 = 0;
	  DAT_1f2024c9 = 4;
	  DAT_1f2024d0 = 0xff;
	  DAT_1f2024d1 = 0xff;
	  DAT_1f2024d4 = 0x10;
	  DAT_1f2024d5 = 0x32;
	  DAT_1f2024d8 = 0x54;
	  DAT_1f2024d9 = 0x76;
	  DAT_1f2024dc = 0x98;
	  DAT_1f2024dd = 0xba;
	  DAT_1f2024e0 = 0xdc;
	  DAT_1f2024e1 = 0xfe;
	  DAT_1f202500 = 0x15;
	  DAT_1f202501 = 0x80;
	  DAT_1f202504 = 8;
	  DAT_1f202505 = 0x20;
	  DAT_1f202508 = 0;
	  DAT_1f202509 = 4;
	  DAT_1f202510 = 0xff;
	  DAT_1f202511 = 0xff;
	  DAT_1f202514 = 0x10;
	  DAT_1f202515 = 0x32;
	  DAT_1f202518 = 0x54;
	  DAT_1f202519 = 0x76;
	  DAT_1f20251c = 0x98;
	  DAT_1f20251d = 0xba;
	  DAT_1f202520 = 0xdc;
	  DAT_1f202521 = 0xfe;
	  DAT_1f202540 = 0x15;
	  DAT_1f202541 = 0x80;
	  DAT_1f202544 = 8;
	  DAT_1f202545 = 0x20;
	  DAT_1f202548 = 0;
	  DAT_1f202549 = 4;
	  DAT_1f202550 = 0xff;
	  DAT_1f202551 = 0xff;
	  DAT_1f202554 = 0x10;
	  DAT_1f202555 = 0x32;
	  DAT_1f202558 = 0x54;
	  DAT_1f202559 = 0x76;
	  DAT_1f20255c = 0x98;
	  DAT_1f20255d = 0xba;
	  DAT_1f202560 = 0xdc;
	  DAT_1f202561 = 0xfe;
	  DAT_1f2025fc = 0xd2;
	  DAT_1f2025fd = 0x81;
	  DAT_1f2023d1 = 0;
	  DAT_1f2023d4 = 8;
	  DAT_1f2023d5 = 0x40;
	  DAT_1f2023d8 = 2;
	  DAT_1f2023d9 = 2;
	  DAT_1f20249c = 0x98;
	  DAT_1f20249d = 0xba;
	  DAT_1f2024a0 = 0xdc;
	  DAT_1f2024a1 = 0xfe;
	  DAT_1f2024b9 = 0;
	  DAT_1f2024c0 = 0x1d;
#endif
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

void cpu_clk_setup(void)
{
#if 0
	  DAT_1f206005 = 0;
	                    /* this is inserting the current frequency */
	  DAT_1f206580 = 0xb9;
	  DAT_1f206581 = 0x1e;
	  DAT_1f206584 = 0x45;
	  DAT_1f206588 = 1;
	  DAT_1f206589 = 0;
	  DAT_1f206445 = 0;
	  DAT_1f206448 = 0x88;
	  delay?(0x4b0);
	  DAT_1f2041f0 = 1;
	  DAT_1f204404 = 0x84;
	  DAT_1f204405 = 4;
	  _DAT_1f206540 = 0x1eb9;
	  _DAT_1f206544 = 0x45;
	  return;
#endif
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
	mstar_miu_init();
	mstar_the_return_of_miu();
	cpu_clk_setup();
	mstar_ddr_test();
}

