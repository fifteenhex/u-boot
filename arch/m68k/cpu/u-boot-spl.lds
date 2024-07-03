/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 */

#include <config.h>

MEMORY { .ram(wx) : ORIGIN = IMAGE_TEXT_BASE, \
					LENGTH = IMAGE_MAX_SIZE }
OUTPUT_ARCH(m68k)
ENTRY(_start)

SECTIONS
{
	.text :
	{
		__image_copy_start = .;
		KEEP(CPUDIR/start.o (.text*))
		*(.text*)
		_etext = .;
		PROVIDE (etext = .);
	} > .ram

	.rodata :
	{
		. = ALIGN(4);
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	} > .ram

	.reloc :
	{
		. = ALIGN(4);
		__got_start = .;
		KEEP(*(.got))
		__got_end = .;
		_GOT2_TABLE_ = .;
		KEEP(*(.got2))
		_FIXUP_TABLE_ = .;
		KEEP(*(.fixup))
		__got2_entries = (_FIXUP_TABLE_ - _GOT2_TABLE_) >>2;
		__fixup_entries = (. - _FIXUP_TABLE_)>>2;
	} > .ram

	.data :
	{
		. = ALIGN(4);
		*(.data*)
		*(.sdata*)
	} > .ram

	__u_boot_list : {
		. = ALIGN(4);
		KEEP(*(SORT(__u_boot_list*)));
	} > .ram
	
	.rela.dyn : {
		. = ALIGN(4);
		__rel_dyn_start = .;
		*(.rela.dyn)
		. = ALIGN(4);
		__rel_dyn_end = .;
	} > .ram

	.dynsym : {
		. = ALIGN(4);
		__dyn_sym_start = .;
		*(.dynsym)
		. = ALIGN(4);
		__dyn_sym_end = .;
	} > .ram

	_end = .;

	__image_copy_end = .;
	_image_binary_end = .;

	.bss (NOLOAD)       :
	{
		. = ALIGN(4);
		__bss_start = .;
		_sbss = .;
		*(.bss*)
		*(.sbss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
		__bss_end = . ;
	} > .ram
	PROVIDE (end = .);
}

#if defined(IMAGE_MAX_SIZE)
ASSERT(__image_copy_end - __image_copy_start <= (IMAGE_MAX_SIZE), \
	"SPL image too big");
#endif

#if defined(CONFIG_SPL_BSS_MAX_SIZE)
ASSERT(__bss_end - __bss_start <= (CONFIG_SPL_BSS_MAX_SIZE), \
	"SPL image BSS too big");
#endif
