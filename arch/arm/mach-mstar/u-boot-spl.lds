/* SPDX-License-Identifier: GPL-2.0+ */

MEMORY { .sram : ORIGIN = IMAGE_TEXT_BASE,\
		LENGTH = IMAGE_MAX_SIZE }
MEMORY { .sram_bss : ORIGIN = CONFIG_SPL_BSS_START_ADDR, \
		LENGTH = CONFIG_SPL_BSS_MAX_SIZE }

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	.text :
	{
		__image_copy_start = .;
		*(.vectors)
		CPUDIR/start.o (.text*)
		*(.text*)
		*(.glue*)
	} > .sram

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(.rodata*)) } >.sram

	. = ALIGN(4);
	.data : { *(SORT_BY_ALIGNMENT(.data*)) } >.sram

	. = ALIGN(4);
	.u_boot_list : {
		KEEP(*(SORT(.u_boot_list*)));
	} > .sram
	
	. = ALIGN(4);
	.binman_sym_table : {
		__binman_sym_start = .;
		KEEP(*(SORT(.binman_sym*)));
		__binman_sym_end = .;
	} > .sram

	__image_copy_end = .;
	.rel.dyn : {
		__rel_dyn_start = .;
		*(.rel*)
		__rel_dyn_end = .;
	} > .sram

	.end :
	{
		*(.__end)
	} > .sram

	_image_binary_end = .;

	.bss :
	{
		. = ALIGN(4);
		__bss_start = .;
		*(.bss*)
		. = ALIGN(4);
		__bss_end = .;
	} > .sram_bss
}

#if defined(IMAGE_MAX_SIZE)
ASSERT(__image_copy_end - __image_copy_start <= (IMAGE_MAX_SIZE), \
	"SPL image too big");
#endif

#if defined(CONFIG_SPL_BSS_MAX_SIZE)
ASSERT(__bss_end - __bss_start <= (CONFIG_SPL_BSS_MAX_SIZE), \
	"SPL image BSS too big");
#endif