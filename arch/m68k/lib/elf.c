// SPDX-License-Identifier: GPL-2.0+

#include <asm/global_data.h>
#include <elf.h>
#include <stdio.h>

DECLARE_GLOBAL_DATA_PTR;

unsigned long bootelf_exec(ulong (*entry)(int, char * const[]),
			   ulong end, int argc, char *const argv[])
{

		printf("setting up bootinfo\n");

	memcpy((void *) end, gd->arch.saved_bootinfo, 4096);

	if (IS_ENABLED(M68K_HAVE_BOOTINFO)) {
		
	}

	return entry(argc, argv);
}
