/*
 * chenxingv7.h
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#ifndef BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_
#define BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_

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

#define PMSLEEP			0x1f001c00
#define PMSLEEP_LOCK		0x48
#define PMSLEEP_LOCK_MAGIC	0xbabe
#define PMSLEEP_C0		0xc0
#define PMSLEEP_C0_BIT4		BIT(4)
#define PMSLEEP_F4		0xf4

#define PMCLKGEN	0x1f001c00
#define TIMER0		0x1f006040
#define EFUSE		0x1f004000
#define EFUSE_14	0x14
#define PINCTRL		0x1f203c00

#define CLKGEN				0x1f207000
#define CLKGEN_UART			0xc4
#define CLKGEN_UART_UART0MUX_MASK	(BIT(3) | BIT(2))

#define MAYBEPLL	0x1f206000
#define MAYBEPLL_04	0x04

#define SCCLKGEN	0x1f226600

#endif /* BOARD_THINGYJP_BREADBEE_CHENXINGV7_H_ */
