/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Linker script for the m68040 SPL.
 */

#include <config.h>

OUTPUT_ARCH(m68k)
ENTRY(_start)

SECTIONS
{
	. = CONFIG_SPL_TEXT_BASE;
	__text_start = .;

	.text :
	{
		arch/m68k/cpu/m680x0/start.o (.text*)
		*(.text*)
	}

	. = ALIGN(16);
	.rodata : { *(.rodata*) }

	. = ALIGN(16);
	.data : { *(.data*) }

	. = ALIGN(4);
	__u_boot_list : {
		KEEP(*(SORT(__u_boot_list*)));
	}

	. = ALIGN(4);
	__image_copy_end = .;
	_image_binary_end = .;
	__init_end = .;

	. = ALIGN(16);
	__bss_start = .;
	.bss (NOLOAD) :
	{
		*(.bss*)
		. = ALIGN(16);
	}
	__bss_end = .;

	_end = .;
}
