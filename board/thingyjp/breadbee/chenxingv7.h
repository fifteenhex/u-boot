/*
 * chenxingv7.h
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#ifndef BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_
#define BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_


static uint16_t mstar_writew(uint16_t value, uint32_t addr)
{
	uint16_t pre = readw(addr);
	uint16_t post;
	writew(value, addr);
	post = readw(addr);
	printf("wrote %04x to %08x, was %04x, readback %04x\n", value, addr, pre, post);
	return post;
}

static uint32_t mstar_writereadback_l(uint32_t value, uint32_t addr)
{
	uint32_t pre = readw(addr);
	uint32_t post;
	writel(value, addr);
	post = readl(addr);
	printf("wrote %08x to %08x, was %08x, readback %08x\n", value, addr, pre, post);
	return post;
}

static void mstar_dump_reg_block(const char* what, uint32_t start){
	uint32_t val;
	void* reg;
	int i, j;

	printf("dump of %s regblock from %08x\n", what, start);

	reg = (void*) start;
	for(i = 0; i < 0x200; i += (4 * 4)){
		printf("%08x: ", start + i);
		for(j = 0; j < 4; j++){
			val = *(uint32_t*)(reg + i + (j * 4));
			printf("%04x 0000 ", val & 0xffff);
		}
		printf("\n");
	}
}

#define CHIPTYPE_UNKNOWN		0
#define CHIPTYPE_MSC313			1
#define CHIPTYPE_MSC313E		2
#define CHIPTYPE_MSC313DC		3
#define CHIPTYPE_SSC8336		4
#define CHIPTYPE_SSC8336N		5
#define CHIPTYPE_SSC325			6

#define CHIPID_MSC313			0xae
#define CHIPID_MSC313ED			0xc2 // this is the same for E and D
#define CHIPID_SSC8336			0xd9
#define CHIPID_SSC8336N			0xee
#define CHIPID_SSC325			0xef

#define PMSLEEP				0x1f001c00
#define PMSLEEP_LOCK			0x48
#define PMSLEEP_LOCK_MAGIC		0xbabe
#define PMSLEEP_C0			0xc0
#define PMSLEEP_C0_BIT4			BIT(4)
#define PMSLEEP_F4			0xf4

#define PMCLKGEN			0x1f001c00
#define TIMER0				0x1f006040

#define EFUSE				0x1f004000
#define EFUSE_14			0x14

#define PINCTRL				0x1f203c00

#define MIU_ANA				0x1f202000
#define MIU_ANA_00			0x00
#define MIU_ANA_04			0x04
#define MIU_ANA_08			0x08
#define MIU_ANA_10			0x10
#define MIU_ANA_14			0x14
#define MIU_ANA_1c			0x1c
#define MIU_ANA_30			0x30
#define MIU_ANA_34			0x34
#define MIU_ANA_38			0x38
#define MIU_ANA_3C			0x3C
#define MIU_ANA_DDRAT_15_0		0x44
#define MIU_ANA_DDRAT_23_16		0x48
#define MIU_ANA_DDRAT_31_24		0x4c
#define MIU_ANA_58			0x58
#define MIU_ANA_5c			0x5c
#define MIU_ANA_60			0x60
#define MIU_ANA_64			0x64
#define MIU_ANA_68			0x68
#define MIU_ANA_6C			0x6c
#define MIU_ANA_70			0x70
#define MIU_ANA_74			0x74
#define MIU_ANA_78			0x78
#define MIU_ANA_7C			0x7c
#define MIU_ANA_90			0x90
#define MIU_ANA_94			0x94
#define MIU_ANA_98			0x98
#define MIU_ANA_9C			0x9c
#define MIU_ANA_A0			0xa0
#define MIU_ANA_A4			0xa4
#define MIU_ANA_A8			0xa8
#define MIU_ANA_B0			0xb0
#define MIU_ANA_B4			0xb4
#define MIU_ANA_B8			0xb8
#define MIU_ANA_BC			0xbc
#define MIU_ANA_C0			0xc0
#define MIU_ANA_C4			0xc4
#define MIU_ANA_C8			0xc8
#define MIU_ANA_D8			0xd8
#define MIU_ANA_DC			0xdc
#define MIU_ANA_E0			0xe0
#define MIU_ANA_E8			0xe8
#define MIU_ANA_F0			0xf0
#define MIU_ANA_F8			0xf8
#define MIU_ANA_114			0x114
#define MIU_ANA_120			0x120
#define MIU_ANA_128			0x128
#define MIU_ANA_130			0x130
#define MIU_ANA_134			0x134
#define MIU_ANA_140			0x140
#define MIU_ANA_144			0x144
#define MIU_ANA_148			0x148
#define MIU_ANA_14C			0x14C
#define MIU_ANA_150			0x150
#define MIU_ANA_154			0x154
#define MIU_ANA_158			0x158
#define MIU_ANA_15C			0x15c
#define MIU_ANA_16C			0x16c
#define MIU_ANA_170			0x170
#define MIU_ANA_174			0x174
#define MIU_ANA_178			0x178
#define MIU_ANA_17C			0x17c
#define MIU_ANA_1A0			0x1a0
#define MIU_ANA_1A4			0x1a4
#define MIU_ANA_1A8			0x1a8
#define MIU_ANA_1AC			0x1ac
#define MIU_ANA_1B0			0x1b0
#define MIU_ANA_1C0			0x1c0
#define MIU_ANA_1C4			0x1c4
#define MIU_ANA_1C8			0x1c8
#define MIU_ANA_1CC			0x1cc
#define MIU_ANA_1D0			0x1d0
#define MIU_ANA_1F0			0x1f0


#define MIU_EXTRA			0x1f202200
#define MIU_EXTRA_GROUP4_CTRL		0x00
#define MIU_EXTRA_GROUP4_REQ_MASK	0x0c
#define MIU_EXTRA_GROUP4_REQ_DEADLINE	0x24
#define MIU_EXTRA_GROUP5_CTRL		0x40
#define MIU_EXTRA_GROUP5_REQ_MASK	0x4c
#define MIU_EXTRA_GROUP5_REQ_DEADLINE	0x6c
#define MIU_EXTRA_C0			0xc0
#define MIU_EXTRA_C4			0xc4
#define MIU_EXTRA_C8			0xc8
#define MIU_EXTRA_CC			0xcc
#define MIU_EXTRA_D0			0xd0
#define MIU_EXTRA_1D0			0x1d0
#define MIU_EXTRA_1D4			0x1d4
#define MIU_EXTRA_1D8			0x1d8

#define MIU_EXTRA_GROUP6_REQ_MASK	0x1cc

#define MIU_DIG				0x1f202400
#define MIU_DIG_CNTRL0			0x00
#define MIU_DIG_CNTRL0_INIT_MIU		BIT(0)
#define MIU_DIG_CNTRL0_CKE		BIT(1)
#define MIU_DIG_CNTRL0_CS		BIT(2)
#define MIU_DIG_CNTRL0_RSTZ		BIT(3)
#define MIU_DIG_CNTRL0_ODT		BIT(4)
#define MIU_DIG_CONFIG0			0x04
#define MIU_DIG_CONFIG1			0x08
#define MIU_DIG_CONFIG2			0x0c
#define MIU_DIG_TIMING0			0x10
#define MIU_DIG_TIMING1			0x14
#define MIU_DIG_TIMING2			0x18
#define MIU_DIG_TIMING3			0x1c
#define MIU_DIG_MR0			0x20
#define MIU_DIG_MR1			0x24
#define MIU_DIG_MR2			0x28
#define MIU_DIG_MR3			0x2c
#define MIU_DIG_SW_RST			0x3c
#define MIU_DIG_SW_RST_MIU		BIT(0)
#define MIU_DIG_SW_RST_SW_INIT_DONE	BIT(3)
#define MIU_DIG_SW_RST_G0		BIT(4)
#define MIU_DIG_SW_RST_G1		BIT(5)
#define MIU_DIG_SW_RST_G2		BIT(6)
#define MIU_DIG_SW_RST_G3		BIT(7)
#define MIU_DIG_ADDR_BAL_SEL		0x58
#define MIU_DIG_GROUP0_CTRL		0x80
#define MIU_DIG_GROUP0_CONFIG0		0x84
#define MIU_DIG_GROUP0_TIMEOUT		0x88
#define MIU_DIG_GROUP0_REQ_MASK		0x8c
#define MIU_DIG_GROUP0_HPMASK		0x90
#define MIU_DIG_GROUP0_REQ_PRIORITY0	0x94
#define MIU_DIG_GROUP0_REQ_PRIORITY1	0x98
#define MIU_DIG_GROUP0_REQ_PRIORITY2	0x9c
#define MIU_DIG_GROUP0_REQ_PRIORITY3	0xa0
#define MIU_DIG_GROUP0_REQ_DEADLINE	0xa4
#define MIU_DIG_GROUP0_REQ_LIMITMASK	0xb8
#define MIU_DIG_GROUP1_CTRL		0xc0
#define MIU_DIG_GROUP1_CONFIG0		0xc4
#define MIU_DIG_GROUP1_TIMEOUT		0xc8
#define MIU_DIG_GROUP1_REQ_MASK		0xcc
#define MIU_DIG_GROUP1_HPMASK		0xd0
#define MIU_DIG_GROUP1_REQ_PRIORITY0	0xd4
#define MIU_DIG_GROUP1_REQ_PRIORITY1	0xd8
#define MIU_DIG_GROUP1_REQ_PRIORITY2	0xdc
#define MIU_DIG_GROUP1_REQ_PRIORITY3	0xe0
#define MIU_DIG_GROUP1_REQ_DEADLINE	0xe4
#define MIU_DIG_GROUP2_CTRL		0x100
#define MIU_DIG_GROUP2_CONFIG0		0x104
#define MIU_DIG_GROUP2_TIMEOUT		0x108
#define MIU_DIG_GROUP2_REQ_MASK		0x10c
#define MIU_DIG_GROUP2_HPMASK		0x110
#define MIU_DIG_GROUP2_REQ_PRIORITY0	0x114
#define MIU_DIG_GROUP2_REQ_PRIORITY1	0x118
#define MIU_DIG_GROUP2_REQ_PRIORITY2	0x11c
#define MIU_DIG_GROUP2_REQ_PRIORITY3	0x120
#define MIU_DIG_GROUP2_REQ_DEADLINE	0x124
#define MIU_DIG_GROUP3_CTRL		0x140
#define MIU_DIG_GROUP3_CONFIG0		0x144
#define MIU_DIG_GROUP3_TIMEOUT		0x148
#define MIU_DIG_GROUP3_REQ_MASK		0x14c
#define MIU_DIG_GROUP3_HPMASK		0x150
#define MIU_DIG_GROUP3_REQ_PRIORITY0	0x154
#define MIU_DIG_GROUP3_REQ_PRIORITY1	0x158
#define MIU_DIG_GROUP3_REQ_PRIORITY2	0x15c
#define MIU_DIG_GROUP3_REQ_PRIORITY3	0x160
#define MIU_DIG_GROUP3_REQ_DEADLINE	0x164
#define MIU_DIG_PROTECT2_START		0x1a4
#define MIU_DIG_MIUSEL0			0x1e0
#define MIU_DIG_PTN_DATA		0x1f8
#define MIU_DIG_R_READ_CRC		0x1fc

#define L3BRIDGE			0x1f204400
#define L3BRIDGE_04			0x04

#define CLKGEN				0x1f207000
#define CLKGEN_UART			0xc4
#define CLKGEN_UART_UART0MUX_MASK	(BIT(3) | BIT(2))

#define MAYBEPLL			0x1f206000
#define MAYBEPLL_04			0x04

#define MAYBEPLL1			0x1f206200
#define MAYBEPLL1_04			0x4
#define MAYBEPLL1_0C			0xc

#define GPIO				0x1f207800
#define GPIO_18				0x18

#define SCCLKGEN			0x1f226600

#define MSTAR_DRAM			0x20000000

#endif /* BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_ */
