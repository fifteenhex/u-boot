#include <init.h>
#include <elf.h>
#include <asm/sections.h>

#define R_68K_32	1
#define R_68K_RELATIVE	22

void relocate_code(ulong start_addr_sp, gd_t *new_gd, ulong relocaddr)
{
	void (*reloc_board_init_r)(gd_t *gd, ulong dest) = board_init_r;
	void *_bss_start = (void *)__bss_start;
	void *_bss_end = (void *)__bss_end;
	Elf32_Rela *rel_start = (void *)__rel_dyn_start;
	Elf32_Rela *rel_end = (void *)__rel_dyn_end;

	/* Copy ourself to the new address */
	if (new_gd->reloc_off) {
		void *dst = (void *)new_gd->relocaddr;
		void *src = 0;//(void *)(new_gd->relocaddr - new_gd->reloc_off);
		size_t len = new_gd->mon_len;
		memcpy(dst, src, len);

		printf("copied from %p to %p, 0x%x bytes\n",
		       src, dst, len);

		reloc_board_init_r += new_gd->relocaddr;

		_bss_start += new_gd->relocaddr;
		_bss_end += new_gd->relocaddr;
		printf("clearing new bss from %p to %p\n", _bss_start, _bss_end);

		memset(_bss_start, 0, _bss_end - _bss_start);
	}

	/* Do ELF relocation */
	for (Elf32_Rela *rel = rel_start; rel != rel_end; rel++) {
		u8 x = rel->r_info & 0xff;
		void *offset = rel->r_offset;
		u32 *sym32 = offset;
		u32 *newsym32 = offset + new_gd->relocaddr;
		u32 newval;

		debug("relocation, i: 0x%08x, off: 0x%08x, a: 0x%08x\n",
				rel->r_info, rel->r_offset, rel->r_addend);
		switch(x) {
		case R_68K_32:
			debug("xx %p -> 0x%08x\n", offset, *sym32);
			*newsym32 = *sym32 + new_gd->relocaddr + rel->r_addend;
			break;
		case R_68K_RELATIVE:
			newval = new_gd->relocaddr + rel->r_addend;
			offset += new_gd->relocaddr;
			*((u32 *)offset) = newval;
			break;
		default:
			panic("Relocation failed, don't know what to do with rela at %p\n", rel);
			break;
		}
	}

	printf("Relocation point of no return, new SP %p\n", new_gd->start_addr_sp);

	/* Fix the GOT pointer */
	__asm__ __volatile__("add.l %0, %%a5" : : "m" (new_gd->relocaddr));

	/* Set the new stack pointer */
	__asm__ __volatile__("move.l %0, %%sp" : : "m" (new_gd->start_addr_sp));

	reloc_board_init_r(new_gd, 0x0);

	while (1) {

	};
}
