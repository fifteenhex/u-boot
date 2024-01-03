#include <init.h>
#include <elf.h>
#include <asm/sections.h>
#include <asm/global_data.h>

#define R_68K_32		1
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

	/* Fix the GOT pointer */
	__asm__ __volatile__("add.l %0, %%a5" : : "m" (new_gd->reloc_off));

	/* Set the new stack pointer */
	__asm__ __volatile__("move.l %0, %%sp" : : "m" (new_gd->start_addr_sp));

	reloc_board_init_r(new_gd, 0x0);

	while (1) {

	};
}
