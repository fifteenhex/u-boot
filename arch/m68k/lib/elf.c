// SPDX-License-Identifier: GPL-2.0+

#include <elf.h>
#include <stdio.h>

unsigned long bootelf_exec(ulong (*entry)(int, char * const[]),
			   ulong end, int argc, char *const argv[])
{

	if (IS_ENABLED(HAVE_BOOTINFO)) {
		printf("setting up bootinfo\n");

	}

	return entry(argc, argv);
}
