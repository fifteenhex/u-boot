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

/*
 * Physical addresses of a NuBus card's register + shared-RAM windows, resolved
 * by the SPL (oldmac_mmu_xlate) via the ROM's page tables while its MMU was
 * still on, and left at a fixed low-RAM address for U-Boot proper.  Proper runs
 * with the MMU off on the 040 Macs and so cannot reach the ROM's non-identity
 * slot mapping; mac8390 uses these translated addresses instead of the logical
 * ones when the SPL left a valid record for the card's slot.
 *
 * The address sits in the boot block's preserved low-RAM gap, just past the Mac
 * bootinfo at MAC_BOOTINFO (0x00501000), so it survives the SPL -> proper
 * hand-off the same way the bootinfo does.
 */
#define OLDMAC_ETH_XLATE_ADDR	0x00501800UL
#define OLDMAC_ETH_XLATE_MAGIC	0x384e5542	/* "8NUB" */

struct oldmac_eth_xlate {
	u32	magic;
	u32	slot;		/* NuBus slot this translation is for */
	u32	reg_phys;	/* DP8390 register window, physical (0 = unresolved) */
	u32	mem_phys;	/* shared-RAM window, physical (0 = unresolved) */
};

/*
 * SPL-only 68040 MMU helpers (mmu040.S).  oldmac_mmu_xlate() returns the
 * physical address the ROM's page tables map @logical to, or 0 if not resident;
 * it must run while the MMU is still on.  oldmac_mmu_disable() tears the ROM MMU
 * down (keeping DTT0's IO cache-inhibit) so U-Boot proper runs with the MMU off.
 * Both use 68040-only instructions and must only be called on 040-class Macs.
 */
ulong oldmac_mmu_xlate(ulong logical);
void oldmac_mmu_disable(void);

#endif /* __OLDMAC_ETH_H */
