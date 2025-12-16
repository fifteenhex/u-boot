#include <asm/global_data.h>
#include <fdt_support.h>
#include <linux/errno.h>
#include <stdlib.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long bootelf_exec(ulong (*entry)(int, char * const[]),
			   int argc, char *const argv[])
{
	unsigned long ret;

#ifdef CONFIG_MC68000
	size_t fdt_size = fdt_totalsize(gd->fdt_blob);

	/* We are going to nuke d7 so save the current value of gd.. */
	gd_t *old_gd = gd;

	void *fdt = malloc(fdt_size);

	if (!fdt) {
		printf("Failed to allocate new FDT, %u bytes\n", (unsigned int) fdt_size);
		return -ENOMEM;
	}
	printf("new fdt %p\n", fdt);
	memcpy(fdt, gd->fdt_blob, fdt_size);
	ret = fdt_chosen(fdt);
	if (ret) {
		printf("Failed to updated chosen node\n");
		return ret;
	}
	unsigned long fdtaddr = (unsigned long) fdt;
	__asm__ volatile("move.l %0, %%d7\n" : : "r" (fdtaddr));
#endif
	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	ret = entry(argc, argv);

#ifdef CONFIG_MC68000
	/* Put gd back */
	/* DOES NOT WORK!! */
	arch_setup_gd(old_gd);
	free(fdt);
#endif

	return ret;
}
