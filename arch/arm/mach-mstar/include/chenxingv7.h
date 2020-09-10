/*
 * chenxingv7.h
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <log.h>

#include "mstarv7.h"

#ifndef BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_
#define BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_

#define ENV_VAR_MSTAR_FAMILY	"mstar_family"
#define COMPAT_I1		"infinity"
#define COMPAT_I1_MSC313	"infinity-msc313"
#define COMPAT_I3		"infinity3"
#define COMPAT_I3_MSC313E	"infinity3-msc313e"
#define COMPAT_I6		"infinity6"
#define COMPAT_M5		"mercury5"
#define COMPAT_GENERIC		"mstar-v7"

static inline uint16_t mstar_writew(uint16_t value, uint32_t addr)
{
	uint16_t pre = readw(addr);
	uint16_t post;
	writew(value, addr);
	post = readw(addr);
	printf("wrote %08x <- %04x was %04x, readback %04x\n", addr,value, pre, post);
	return post;
}

static inline uint32_t mstar_writereadback_l(uint32_t value, uint32_t addr)
{
	uint32_t pre = readw(addr);
	uint32_t post;
	writel(value, addr);
	post = readl(addr);
	printf("wrote %08x to %08x, was %08x, readback %08x\n", value, addr, pre, post);
	return post;
}

static inline void mstar_dump_reg_block(const char* what, uint32_t start){
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

static inline void mstar_delay(unsigned long msec)
{
	printf("delaying for %lu\n", msec);
	mdelay(msec);
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
#define CHIPID_SSC328			0xed
#define CHIPID_SSC325			0xef
#define CHIPID_SSD202			0xf0
#define CHIPID_SSC337DE			0xf2

#define PMSLEEP				0x1f001c00
#define PMSLEEP_LOCK			0x48
#define PMSLEEP_LOCK_MAGIC		0xbabe
#define PMSLEEP_C0			0xc0
#define PMSLEEP_C0_BIT4			BIT(4)
#define PMSLEEP_F4			0xf4

#define PMCLKGEN			0x1f001c00

#define CHIPID				0x1f003c00
#define CHIPIDCOPY			0x1f003d98


#define EFUSE				0x1f004000
#define EFUSE_14			0x14
#define EFUSE_20			0x20

#define PINCTRL				0x1f203c00

#define L3BRIDGE			0x1f204400
#define L3BRIDGE_2C			0x2c
#define L3BRIDGE_14			0x14
#define L3BRIDGE_40			0x40
#define L3BRIDGE_FLUSH_TRIGGER		BIT(0)
#define L3BRIDGE_STATUS_DONE		BIT(12)

#define CLKGEN				0x1f207000
#define CLKGEN_MIU			0x5c
#define CLKGEN_MIU_BOOT			0x80
#define CLKGEN_BDMA			0x180

#define CLKGEN_PM			0x1f001c00
#define CLKGEN_SPI_MCU_PM		0x80

#define DID				0x1f007000
#define DID_BOOTSOURCE			0x1c0
#define DID_BOOTSOURCE_M5_SD		BIT(14)

#define GPIO				0x1f207800
#define GPIO_18				0x18
#define GPIO_18_D9_DDRMASK		0xf

#define SCCLKGEN			0x1f226600

#define UTMI0				0x1f284200
#define UTMI0_50			0x50
#define UTMI1				0x1f285200
#define UTMI1_50			0x50



static const uint8_t* deviceid = (uint8_t*) CHIPID;
static const void* efuse = (void*) EFUSE;

static inline int mstar_chiptype(void){
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

void mstar_bump_cpufreq(void);
void mstar_clockfixup(void);

#endif /* BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_ */
