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
	mstar_writereadback_l(0xA5A5A5A5, MSTAR_DRAM + 0x4);
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
	mstar_ddr_setrequestmasks(0xfffe, 0xffff, 0xffff,
			0xffff, 0xffff, 0xffff, 0xfffe);

	// drive cal software mode
	mstar_writew(1, MIU_ANA + MIU_ANA_F0);
	mdelay(100);

	mstar_writew(0x1000, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);

	mstar_writew(0, MIU_ANA + MIU_ANA_DDRAT_23_16);
	mdelay(100);

	//ddr clock freq
	mstar_writew(0x400, MIU_ANA + MIU_ANA_6C);

	// ddr clock freq
	mstar_writew(0x2004, MIU_ANA + MIU_ANA_68);

	// dunno :(
	mstar_writew(0x1, MIU_ANA + MIU_ANA_114);

	// clock gen freq set
	mstar_writew(0x8000, MIU_ANA + MIU_ANA_60);
	mstar_writew(0x29, MIU_ANA + MIU_ANA_64);
	mdelay(100);

	// dunno
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
	  //


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


	  // clock wave form
	  mstar_writew(0xaaaa, MIU_ANA + MIU_ANA_04);

	  // ???
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_08);

	  // rd phase timing, m5 is 0
	  mstar_writew(0x1100, MIU_ANA + MIU_ANA_14);

	 // more timing
	  mstar_writew(0x85, MIU_ANA + MIU_ANA_1c);

	  // reserved , m5 is 2222
	  mstar_writew(0x1112, MIU_ANA + MIU_ANA_5c);

	  // clock phase select
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
	  // phase select
	  mstar_writew(0x4040, MIU_ANA + MIU_ANA_74);
	  // rec trig
	  mstar_writew(0x202, MIU_ANA + MIU_ANA_E8);

	  // ???
	  mstar_writew(0x1517, MIU_ANA + MIU_ANA_128);
	  mstar_writew(0x14, MIU_ANA + MIU_ANA_140);
	  mstar_writew(0x2434, MIU_ANA + MIU_ANA_144);
	  mstar_writew(0x1132, MIU_ANA + MIU_ANA_148);
	  mstar_writew(0x4311, MIU_ANA + MIU_ANA_14C);

	  // phase delay select, m5 0707
	  mstar_writew(0x808, MIU_ANA + MIU_ANA_DC);
	  // reserved m5 0707
	  mstar_writew(0x808, MIU_ANA + MIU_ANA_D8);

	  // reserved - m5 0200
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_A4);
	  // reserved - m5 0
	  mstar_writew(0x1111, MIU_ANA + MIU_ANA_A0);


	  // undocumented
	  mstar_writew(0x33, MIU_ANA + MIU_ANA_9C);
	  mstar_writew(0x33, MIU_ANA + MIU_ANA_98);
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_94);
	  mstar_writew(0x77, MIU_ANA + MIU_ANA_90);


	  // skew - m5 0
	  mstar_writew(0x1011, MIU_ANA + MIU_ANA_7C);
	  // skew
	  mstar_writew(0x9133, MIU_ANA + MIU_ANA_78);

	  // ??
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

	  // ps cycle
	  mstar_writew(0x7f, MIU_ANA + MIU_ANA_C4);

	  // dll code
	  mstar_writew(0xf000, MIU_ANA + MIU_ANA_C8);

	  // dll crap
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

	  // not documented
	  mstar_writew(0x1f1f, MIU_ANA + MIU_ANA_B0);
	  // not documented
	  mstar_writew(0x0000, MIU_ANA + MIU_ANA_B4);

	  // drv
	  mstar_writew(0x0000, MIU_ANA + MIU_ANA_B8);

	  // drv
	  mstar_writew(0x0010, MIU_ANA + MIU_ANA_BC);

	  // ptn mode m5 8020
	  mstar_writew(0x8111, MIU_ANA + MIU_ANA_34);
	  // pattern data
	  mstar_writew(0x20, MIU_ANA + MIU_ANA_38);
#if 0
	  something_to_dowith_ddr();
#endif

	  // rx en
	  mstar_writew(0x3f, MIU_ANA + MIU_ANA_10);

	  mstar_writew(0x8c00, MIU_DIG + MIU_DIG_SW_RST);

	  // test register
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_30);

	  // reserved -
	  mstar_writew(0x0, MIU_ANA + MIU_ANA_F8);

	  // drv
	  mstar_writew(0x4000, MIU_ANA + MIU_ANA_A8);

	  // read crc
	  mstar_writew(0x5, MIU_ANA + MIU_ANA_3C);

	  // power up ana
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
#endif
	  mstar_writew(0x484, L3BRIDGE + L3BRIDGE_04);

#if 0
	  _DAT_1f206540 = 0x1eb9;
	  _DAT_1f206544 = 0x45;
	  return;
#endif
}

void mstar_ddr_unmask_setdone()
{
	uint16_t temp;

	mstar_ddr_setrequestmasks(0, 0, 0, 0, 0, 0, 0);

	/* if this is not cleared any access to the DDR locks the CPU */
	temp = readw(MIU_DIG + MIU_DIG_SW_RST);
	temp |= MIU_DIG_SW_RST_SW_INIT_DONE;
	mstar_writew(temp, MIU_DIG + MIU_DIG_SW_RST);
}

static void mstar_ddr_init_i3_ip(void)
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
	mstar_ddr_unmask_setdone();
}

struct ddr_config {

};

static int mstar_ddr_getconfig(int chiptype, struct ddr_config *config)
{
	uint16_t type;

	printf("tying to work out DDR config..\n");

	switch(chiptype){
	case CHIPTYPE_SSC8336:
		type = readw(GPIO + GPIO_18);
		printf("mystery gpio register is %02x\n", type);
		break;
	default:
		printf("Don't know how to find DRAM config for chiptype %i\n", chiptype);
		return -EINVAL;
	}

	return 0;
}

void mstar_ddr_init(int chiptype)
{
	struct ddr_config config;

	mstar_ddr_rst();

	mstar_dump_reg_block("miu_ana", MIU_ANA);
	mstar_dump_reg_block("miu_extra", MIU_EXTRA);
	mstar_dump_reg_block("miu_dig", MIU_DIG);

	if (mstar_ddr_getconfig(chiptype, &config))
		goto out;

	mstar_ddr2_init();

	mstar_dump_reg_block("miu_ana+", MIU_ANA);
	mstar_dump_reg_block("miu_extra+", MIU_EXTRA);
	mstar_dump_reg_block("miu_dig+", MIU_DIG);

	//mstar_ddr_test();

out:
	return;
}
