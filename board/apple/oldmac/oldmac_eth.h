/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Shared description of a NuBus DP8390-based Ethernet card discovered by the
 * declaration-ROM scanner (nubus.c) and consumed by the mac8390 driver, which
 * the board binds manually (no device tree) when a card is present.
 */
#ifndef __OLDMAC_ETH_H
#define __OLDMAC_ETH_H

#include <linux/types.h>

struct oldmac_eth_info {
	int	found;
	int	slot;		/* NuBus slot number (0x9..0xE) */
	ulong	slot_addr;	/* 0xF0000000 | (slot << 24) */
	ulong	minor_base;	/* MinorBaseOS: reg/RAM base offset from base_addr */
	u16	dr_sw;		/* functional-resource DrSW (selects Linux variant) */
	u16	dr_hw;		/* functional-resource DrHW */
	u8	enetaddr[6];	/* station address from the MAC_ADDRESS resource */
};

/* Scan NuBus slots for a Network/Ethernet functional sResource; returns 0 and
 * fills @info on success, -ENODEV if none.  Result is cached after the first
 * call. */
int nubus_find_eth(struct oldmac_eth_info *info);

#endif /* __OLDMAC_ETH_H */
