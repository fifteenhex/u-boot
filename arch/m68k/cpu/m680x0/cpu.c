// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * CPU specific code for m68040
 *
 * Copyright (C) 2025, Kuan-Wei Chiu <visitorckw@gmail.com>
 */

#include <config.h>
#include <cpu_func.h>
#include <elf.h>
#include <init.h>
#include <stdio.h>
#include <asm/global_data.h>
#include <asm/sections.h>
#include <linux/types.h>

DECLARE_GLOBAL_DATA_PTR;

void m68k_virt_init_reserve(ulong base)
{
	struct global_data *gd_ptr = (struct global_data *)base;
	char *p = (char *)gd_ptr;
	unsigned int i;

	/* FIXME: usage of memset() here caused a hang on QEMU m68k virt. */
	for (i = 0; i < sizeof(*gd_ptr); i++)
		p[i] = 0;

	gd = gd_ptr;

	gd->malloc_base = base + sizeof(*gd_ptr);
}

int print_cpuinfo(void)
{
	puts("CPU:   M68040 (QEMU Virt)\n");

	return 0;
}

int get_clocks(void)
{
	return 0;
}

int cpu_init_r(void)
{
	return 0;
}

/*
 * Relocation Stub
 * We skip actual relocation for this QEMU bring-up and jump directly
 * to board_init_r.
 */

#define R_68K_32	1
#define R_68K_JMP_SLOT	21
#define R_68K_RELATIVE	22

void relocate_code(ulong start_addr_sp, gd_t *new_gd, ulong relocaddr);
void relocate_code(ulong start_addr_sp, gd_t *new_gd, ulong relocaddr)
{
	void (*reloc_board_init_r)(struct global_data*,ulong) = board_init_r;
	void *_bss_start = (void *)__bss_start;
	void *_bss_end = (void *)__bss_end;
	Elf32_Rela *rel_start = (void *)__rel_dyn_start;
	Elf32_Rela *rel_end = (void *)__rel_dyn_end;

	/* Copy ourself to the new address */
	if (new_gd->reloc_off) {
		void *dst = (void *)new_gd->relocaddr;
		void *src = (void *)(new_gd->relocaddr - new_gd->reloc_off);
		size_t len = new_gd->mon_len;
		memcpy(dst, src, len);

		printf("copied from %p to %p, 0x%x bytes (reloc_off 0x%08x)\n",
		       src, dst, len, (unsigned int) new_gd->reloc_off);

		reloc_board_init_r += new_gd->reloc_off;

		_bss_start += new_gd->reloc_off;
		_bss_end += new_gd->reloc_off;
		rel_start = ((void*) rel_start) + new_gd->reloc_off;
		rel_end = ((void*) rel_end) + new_gd->reloc_off;

		printf("clearing new bss from %p to %p\n", _bss_start, _bss_end);
		memset(_bss_start, 0, _bss_end - _bss_start);
	}

	/* Do ELF relocation */
	printf("Doing relocation \n");
	for (Elf32_Rela *rel = rel_start; rel != rel_end; rel++) {
		u8 x = rel->r_info & 0xff;
		void *offset = (void *) rel->r_offset;
		u32 *sym32 = offset;
		u32 *newsym32 = offset + new_gd->reloc_off;
		u32 newval;

		debug("relocation, i: 0x%08x, off: 0x%08x, a: 0x%08x\n",
				rel->r_info, rel->r_offset, rel->r_addend);
		debug("relocation, sym32: %p, newsym32: %p\n",
				sym32, newsym32);
		switch(x) {
		case R_68K_32:
			*newsym32 = *sym32 + new_gd->reloc_off + rel->r_addend;
			break;
		case R_68K_RELATIVE:
			newval = new_gd->reloc_off + rel->r_addend;
			*newsym32 = newval;
			break;
		case R_68K_JMP_SLOT:
			// I don't think this should be in our final binary but
			// for some reason with 030 turned on it happens
			*newsym32 = *newsym32 + new_gd->reloc_off;
			break;
		default:
			panic("Relocation failed, don't know what to do with rela at 0x%p, r_info: 0x%x\n",
					rel, rel->r_info);
			break;
		}

		debug("newsym value is 0x%08x\n", (unsigned int) *newsym32);
	}

	printf("Relocation point of no return, new SP 0x%p, jump to 0x%p\n",
			(void*) new_gd->start_addr_sp, reloc_board_init_r);

	/* Fix the GOT pointer, set the new stack pointer, then jmp */
	__asm__ __volatile__("add.l %0, %%a5\n"
			     "move.l %1, %%sp\n"
			     "move.l #0, -(%%sp)\n"
			     "move.l %2, -(%%sp)\n"
			     "jsr (%3)\n"
			     :
			     : "d" (new_gd->reloc_off),
			       "a" (new_gd->start_addr_sp),
			       "a" (new_gd),
			       "a" (reloc_board_init_r));

	/* Will no reach here */
	while (1) {
	}
}

/* Stubs for Standard Facilities (Cache, Interrupts) */

int disable_interrupts(void) { return 0; }
void enable_interrupts(void) { return; }
int interrupt_init(void) { return 0; }

void icache_enable(void) {}
void icache_disable(void) {}
int icache_status(void) { return 0; }
void dcache_enable(void) {}
void dcache_disable(void) {}
int dcache_status(void) { return 0; }
void flush_cache(unsigned long start, unsigned long size) {}
void flush_dcache_range(unsigned long start, unsigned long stop) {}
