# SPDX-License-Identifier: GPL-2.0-or-later

obj-y = oldmac.o

# VIA1 timebase - needed in both the SPL (driver timeouts) and U-Boot proper.
obj-$(CONFIG_$(PHASE_)TIMER) += via_timer.o

# Bonus peripherals are only for U-Boot proper; keep them (and their DMA
# buffers) out of the size-constrained SPL, which only needs serial + SCSI.
ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_CMD_NUBUS) += nubus.o nubus_buserr.o
obj-$(CONFIG_DM_KEYBOARD) += adb.o
obj-$(CONFIG_DM_ETH) += sonic.o mac8390.o
endif
