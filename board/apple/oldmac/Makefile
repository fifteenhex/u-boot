# SPDX-License-Identifier: GPL-2.0-or-later

obj-y = oldmac.o

# Bonus peripherals are only for U-Boot proper; keep them (and their DMA
# buffers) out of the size-constrained SPL, which only needs serial + SCSI.
ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_CMD_NUBUS) += nubus.o nubus_buserr.o
obj-$(CONFIG_DM_KEYBOARD) += adb.o
obj-$(CONFIG_DM_ETH) += sonic.o
endif
