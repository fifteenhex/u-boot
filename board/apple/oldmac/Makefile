# SPDX-License-Identifier: GPL-2.0-or-later

obj-y = oldmac.o

# VIA1 timebase - needed in both the SPL (driver timeouts) and U-Boot proper.
obj-$(CONFIG_$(PHASE_)TIMER) += via_timer.o

# NuBus declaration-ROM scanner + bus-error-safe probe helper, built for both
# phases.  The SPL uses nubus_find_eth() to locate a card and translate its
# non-identity register window to a physical address while the ROM MMU is still
# on; U-Boot proper reuses the same scanner and adds the interactive `nubus`
# command and slot listing (both compiled out of the size-limited SPL).
obj-y += nubus.o nubus_buserr.o

# SPL-only 68040 MMU helpers: translate NuBus/video windows to physical while
# the ROM MMU is on, then tear it down for U-Boot proper.
ifdef CONFIG_SPL_BUILD
obj-y += mmu040.o
endif

# Bonus peripherals are only for U-Boot proper; keep them (and their DMA
# buffers) out of the size-constrained SPL, which only needs serial + SCSI.
ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_DM_KEYBOARD) += adb.o
obj-$(CONFIG_DM_ETH) += sonic.o mac8390.o
endif
