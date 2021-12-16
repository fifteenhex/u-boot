# SPDX-License-Identifier: GPL-2.0

spinand-objs := core.o gigadevice.o longsys.o macronix.o micron.o toshiba.o winbond.o xincun.o
ifdef CONFIG_SPL_BUILD
spinand-objs += spl.o
endif

obj-$(CONFIG_MTD_SPI_NAND) += spinand.o
