# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2000-2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

PLATFORM_CPPFLAGS += -D__M68K__
ifeq ($(CONFIG_M680x0),y)
# On the 68040, U-Boot proper relocates itself to high RAM (see relocate_code),
# which needs position-independent code with dynamic relocations.  The SPL runs
# in place and does not apply relocations, so it must stay non-PIC.
ifdef CONFIG_SPL_BUILD
PLATFORM_CPPFLAGS += -fno-pic
else
PLATFORM_CPPFLAGS += -fPIC
endif
else
PLATFORM_CPPFLAGS += -fPIC
endif
KBUILD_LDFLAGS    += -n -pie
PLATFORM_RELFLAGS += -ffunction-sections -fdata-sections
PLATFORM_RELFLAGS += -ffixed-d7
ifneq ($(CONFIG_M680x0),y)
PLATFORM_RELFLAGS += -msep-data
endif
LDFLAGS_FINAL     += --gc-sections -pie
