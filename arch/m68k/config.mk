# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

PLATFORM_CPPFLAGS += -D__M68K__ -fPIC
KBUILD_LDFLAGS    += -n -pie
PLATFORM_RELFLAGS += -ffixed-d7 -msep-data
LDFLAGS_FINAL     += -pie
PLATFORM_ELFFLAGS += -B m68k -O elf32-m68k

ifneq ($(LTO_ENABLE)$(CONFIG_USE_PRIVATE_LIBGCC),yy)
LDFLAGS_FINAL += --gc-sections
endif

ifneq ($(LTO_ENABLE),y)
PLATFORM_RELFLAGS += -ffunction-sections -fdata-sections
endif
