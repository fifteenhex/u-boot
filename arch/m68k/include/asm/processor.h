#ifndef __ASM_M68K_PROCESSOR_H
#define __ASM_M68K_PROCESSOR_H

#include <asm/ptrace.h>
#include <asm/types.h>

#define _GLOBAL(n)\
	.globl n;\
n:

/* Macros for setting and retrieving special purpose registers */
#ifdef CONFIG_MC68000
#define setvbr(v)
#else
#define setvbr(v)	asm volatile("movec %0,%%VBR" : : "r" (v))
#endif

#endif /* __ASM_M68K_PROCESSOR_H */
