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
		const u32 memsize = gd->ram_size >> 20;	/* in MiB */

		/* copy the saved records except the terminating BI_LAST */
		memcpy(rec, gd->arch.saved_bootinfo, gd->arch.bootinfo_size - 4);
		rec = (struct bi_record *)((u8 *)rec +
					   gd->arch.bootinfo_size - 4);

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
