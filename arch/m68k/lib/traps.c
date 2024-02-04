// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2003
 * Josef Baumgartner <josef.baumgartner@telex.de>
 *
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <config.h>
#include <cpu_func.h>
#include <init.h>
#include <watchdog.h>
#include <command.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#ifdef CONFIG_MC68000
#define NULLTRAP 32
extern void _int_sled_4(void);
extern void _int_sled_5(void);
extern void _int_sled_8(void);
extern void _int_sled_32(void);
#else
extern void _exc_handler(void);
extern void _int_handler(void);
#endif

static void show_frame(unsigned int vector_num, unsigned int format, unsigned int status,
		struct pt_regs *fp)
{
	const char *name = "unknown";

	switch (vector_num) {
	case 4:
		name = "illegal instruction";
		break;
	case 5:
		name = "divide by zero";
		break;
	case 8:
		name = "privilege violation";
		break;
	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:
	case 38:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
		name = "TRAP";
		break;
	}

	printf ("Vector Number: %d (%s)  Format: %02x  Fault Status: %01x\n\n",
			vector_num, name, format, status);
	printf ("PC: %08lx    SR: %08lx    SP: %08lx\n", fp->pc, (long) fp->sr, (long) fp);
	printf ("D0: %08lx    D1: %08lx    D2: %08lx    D3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printf ("D4: %08lx    D5: %08lx    D6: %08lx    D7: %08lx\n",
		fp->d4, fp->d5, fp->d6, fp->d7);
	printf ("A0: %08lx    A1: %08lx    A2: %08lx    A3: %08lx\n",
		fp->a0, fp->a1, fp->a2, fp->a3);
	printf ("A4: %08lx    A5: %08lx    A6: %08lx\n",
		fp->a4, fp->a5, fp->a6);
}

#ifdef CONFIG_MC68000
static unsigned long int_handlers[256] = {
		[4] = (unsigned long)_int_sled_4,
		[5] = (unsigned long)_int_sled_5,
		[8] = (unsigned long)_int_sled_8,
};

void exc_handler(int vec, int group, struct pt_regs *fp)
{
	printf("vec %d\n", vec);
	show_frame(vec, group, 0, fp);

	if(vec == NULLTRAP) {
		while(1){ }
	}
}
#else
static unsigned long int_handlers[256] = {
		[2 ... 24 ] = (unsigned long)_exc_handler,
		[25 ... 31] = (unsigned long)_int_handler,
		[32 ... 63] = (unsigned long)_exc_handler,
		[64 ... 255] = (unsigned long)_int_handler,
};
void exc_handler(struct pt_regs *fp) {
	printf("\n\n*** Unexpected exception ***\n");
	show_frame (((fp->vector & 0x3fc) >> 2), fp->format, (fp->vector & 0x3) | ((fp->vector & 0xc00) >> 8), fp);
	printf("\n*** Please Reset Board! ***\n");
	for(;;);
}
#endif
static void trap_init(ulong value) {
	unsigned long *vec = (ulong *)value;
	int i;

	for(i = 0; i < ARRAY_SIZE(int_handlers); i++) {
		if (int_handlers[i])
			vec[i] = int_handlers[i];
	}

	setvbr(value);		/* set vector base register to new table */
}

int arch_initr_trap(void)
{
#ifdef CONFIG_MC68000
	trap_init(0);
#else
	trap_init(CFG_SYS_SDRAM_BASE);
#endif
	return 0;
}

void reset_cpu(void)
{
	/* TODO: Refactor all the do_reset calls to be reset_cpu() instead */
	do_reset(NULL, 0, 0, NULL);
}
