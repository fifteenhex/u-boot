// SPDX-License-Identifier: GPL-2.0+
/*
 * m68k bootelf hand-off: Linux/m68k expects its bootinfo record list to sit
 * immediately after the kernel image (at the kernel's _end).  bootelf() loads
 * the kernel ELF and hands us the address just past it, so we rebuild the
 * bootinfo the board saved (from the ROM/firmware) there, splicing in the
 * kernel command line from the environment ("bootargs"), before entering the
 * kernel.
 */

#include <elf.h>
#include <env.h>
#include <bootm.h>
#include <vsprintf.h>
#include <asm/global_data.h>
#include <asm/bootinfo.h>
#include <linux/string.h>

DECLARE_GLOBAL_DATA_PTR;

static struct bi_record *bi_put(struct bi_record *rec, ushort tag,
				const void *data, int len)
{
	rec->tag = tag;
	rec->size = sizeof(*rec) + ALIGN(len, 4);
	memset(rec->data, 0, ALIGN(len, 4));	/* zero the tail padding too */
	if (len)
		memcpy(rec->data, data, len);
	return (struct bi_record *)((u8 *)rec + rec->size);
}

/* Boards with an FPU-less 68040 variant (e.g. the 68LC040 in the LC 475)
 * override this so Linux does not try to use a nonexistent FPU.  0 = no FPU. */
__weak u32 board_m68k_fputype(void)
{
	return FPU_68040;
}

/*
 * Boards whose ROM framebuffer address is unusable with the MMU off (e.g. the
 * LC 475's non-identity 0x51901000, which bus-errors) override this to relocate
 * Linux's early framebuffer console to a scratch area carved from the top of
 * RAM.  head.S's console_init()/console_putc() write to BI_MAC_VADDR very early,
 * before the serial console is up and with no guard, so a bad address hangs the
 * kernel silently.  Returns the scratch framebuffer physical address and lowers
 * *memsize_mb to exclude it, or 0 to keep the ROM's framebuffer as-is.
 */
__weak ulong board_m68k_video_scratch(u32 *memsize_mb)
{
	return 0;
}

unsigned long bootelf_exec(ulong (*entry)(int, char * const[]),
			   ulong end, int argc, char *const argv[])
{
	if (gd->arch.bootinfo_size) {
		struct bi_record *rec = (struct bi_record *)ALIGN(end, 4);
		const char *args = env_get("bootargs");
		const u32 cputype = CPU_68040;
		const u32 fputype = board_m68k_fputype();
		const u32 mmutype = MMU_68040;
		const u32 cpuid = CPUB_68040;
		u32 memsize = gd->ram_size >> 20;	/* in MiB */
		ulong fb = board_m68k_video_scratch(&memsize);
		struct bi_record *src;

		/*
		 * Copy the saved records (up to, but not including, BI_LAST).
		 * When the board relocated the early console framebuffer to
		 * scratch RAM, rewrite BI_MAC_VADDR to it and shrink the
		 * BI_MEMCHUNK size so Linux does not use that reserved region;
		 * copy every other record verbatim.
		 */
		src = (struct bi_record *)gd->arch.saved_bootinfo;
		for (; src->tag != BI_LAST;
		     src = (struct bi_record *)((u8 *)src + src->size)) {
			int dlen = src->size - sizeof(*src);

			if (fb && src->tag == BI_MAC_VADDR) {
				u32 v = fb;

				rec = bi_put(rec, BI_MAC_VADDR, &v, sizeof(v));
			} else if (fb && src->tag == BI_MEMCHUNK && dlen >= 8) {
				u32 mc[2];

				memcpy(mc, src->data, sizeof(mc));
				mc[1] = (u32)memsize << 20;	/* shrunk size */
				rec = bi_put(rec, BI_MEMCHUNK, mc, sizeof(mc));
			} else {
				rec = bi_put(rec, src->tag, src->data, dlen);
			}
		}

		/*
		 * The Mac bootinfo the ROM boot chain builds describes the
		 * machine but not the CPU, so Linux/m68k would read -1 for the
		 * CPU/FPU/MMU type and mis-configure paging.  Supply them (the
		 * m680x0 build is 68040-only).
		 */
		rec = bi_put(rec, BI_CPUTYPE, &cputype, sizeof(cputype));
		rec = bi_put(rec, BI_FPUTYPE, &fputype, sizeof(fputype));
		rec = bi_put(rec, BI_MMUTYPE, &mmutype, sizeof(mmutype));
		rec = bi_put(rec, BI_MAC_CPUID, &cpuid, sizeof(cpuid));
		rec = bi_put(rec, BI_MAC_MEMSIZE, &memsize, sizeof(memsize));

		/*
		 * If the boot script loaded a ramdisk/initramfs into memory and set
		 * initrd_start / initrd_size, pass it to Linux via BI_RAMDISK (the
		 * kernel reserves it and unpacks it as the initramfs).  Both are
		 * hex; the address is physical (U-Boot runs with the MMU off).
		 */
		{
			const char *rs = env_get("initrd_start");
			const char *rn = env_get("initrd_size");

			if (rs && rn) {
				struct { u32 addr; u32 size; } rd;

				rd.addr = simple_strtoul(rs, NULL, 16);
				rd.size = simple_strtoul(rn, NULL, 16);
				if (rd.addr && rd.size)
					rec = bi_put(rec, BI_RAMDISK, &rd,
						     sizeof(rd));
			}
		}

		if (args && *args)
			rec = bi_put(rec, BI_COMMAND_LINE, args, strlen(args) + 1);

		bi_put(rec, BI_LAST, NULL, 0);
	}

	/* Let the board return hardware (interrupt sources the ROM/U-Boot left
	 * active) to a quiescent state before we hand off to the OS. */
	board_quiesce_devices();

	/*
	 * Enter the kernel the way a classic-Mac bootstrap does: interrupts
	 * masked, caches flushed to RAM and disabled, and the MMU turned off (all
	 * translation cleared).  U-Boot itself never programs these, but on the
	 * real-ROM boot path the Apple ROM leaves the MMU and caches enabled;
	 * Linux/m68k head.S expects them off and double-faults otherwise.  RAM is
	 * identity-mapped here, so dropping translation keeps us executing at the
	 * same addresses.  The 68040 uses cpusha + movec %ttN; the 68030 uses CACR
	 * + pmove %tc/%tt0/%tt1.
	 */
	if (m68k_is_68040()) {
		__asm__ __volatile__(
			".chip 68040\n\t"
			"move.w #0x2700,%%sr\n\t"	/* mask interrupts */
			"nop\n\t"
			"cpusha %%bc\n\t"		/* flush+invalidate both caches */
			"nop\n\t"
			"moveq #0,%%d0\n\t"
			"movec %%d0,%%tc\n\t"		/* disable paging MMU */
			"movec %%d0,%%itt0\n\t"
			"movec %%d0,%%itt1\n\t"
			"movec %%d0,%%dtt0\n\t"
			"movec %%d0,%%dtt1\n\t"
			"movec %%d0,%%cacr\n\t"		/* disable caches */
			"nop\n\t"
			".chip 68k\n\t"
			: : : "d0", "memory");
	} else {
		u32 zero = 0;

		__asm__ __volatile__(
			".chip 68030\n\t"
			"move.w #0x2700,%%sr\n\t"	/* mask interrupts */
			"move.l #0x0808,%%d0\n\t"	/* CI | CD: clear+disable caches */
			"movec %%d0,%%cacr\n\t"
			"pmove %0,%%tc\n\t"		/* disable paging */
			"pmove %0,%%tt0\n\t"		/* clear transparent translation */
			"pmove %0,%%tt1\n\t"
			".chip 68k\n\t"
			: : "m" (zero) : "d0", "memory");
	}

	return entry(argc, argv);
}
