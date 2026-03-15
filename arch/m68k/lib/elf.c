// SPDX-License-Identifier: GPL-2.0+

#include <elf.h>

unsigned long bootelf_exec(ulong (*entry)(int, char * const[]),
			   int argc, char *const argv[])
{

	if (IS_ENABLED(HAVE_BOOTINFO)) {

	}

	return entry(argc, argv);
}
