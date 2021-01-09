# SPDX-License-Identifier: GPL-2.0+

nandcore-objs := core.o bbt.o
obj-$(CONFIG_MTD_NAND_CORE) += nandcore.o
ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)
obj-$(CONFIG_MTD_RAW_NAND) += raw/
obj-$(CONFIG_MTD_SPI_NAND) += spi/
else
obj-$(CONFIG_$(SPL_TPL_)NAND_SUPPORT) += raw/
obj-$(CONFIG_$(SPL_TPL_)SPI_NAND_SUPPORT) += spi/
endif
