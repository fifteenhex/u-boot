/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm-offsets.h>
#include <config.h>
#include <asm/dragonball/mc68ez328.h>
#include <asm/mvme147/mvme147.h>
#include <asm/e17/e17.h>

#define DEBUG

.macro __putch val
#ifdef DEBUG
#ifdef CONFIG_MC68EZ328
	move.b	#\val,(MC68EZ328_UART + MC68EZ328_UART_TXDATA)
1:
	move.w	(MC68EZ328_UART + MC68EZ328_UART_TXFLAGS),%d7
	and.w	#0x2000,%d7
	beq		1b
#elif defined(CONFIG_TARGET_MVME147)
	move.b #\val,(MVME147_SERIAL0_RXTX)
1:
	move.b #0, (MVME147_SERIAL0_REG)
	move.b (MVME147_SERIAL0_REG), %d7
	and.b   #0x4,%d7
	beq	1b
#elif defined(CONFIG_TARGET_E17)
10:
	//btst.b #1, (E17_SERIAL_TISR)
	//beq 10b
	//move.b #\val, (E17_SERIAL_TDR)
	//move.b #0, (E17_SERIAL_TEOIR)
#endif
#endif
.endm

.macro __printch ch
#ifdef DEBUG
	move.l	%d7, -(%sp)
	__putch \ch
	__putch '\n'
	__putch '\r'
	move.l (%sp)+, %d7
#endif
.endm

#if defined(CONFIG_TARGET_E17)
_module_sp:
	.long (0x400000 + 0x100000)
_module_pc:
	.long _start
#endif

#if !defined(CONFIG_SPL_BUILD) && !defined(CONFIG_TARGET_MVME147)
.text
_vectors:
_initial_sp:
	/*
	 * For real hardware we need the bottom of the image to have
	 * space for the vectors, on QEMU we enter the ELF at the entry
	 * address and the initial stackpointer and PC are not used.
	 * For real hardware we don't use the initial stack pointer.
	 *
	 * So we can do a little hack here. If we encode a trap instruction
	 * here a null function pointer or otherwise jumping to 0 can be detected.
	 *
	 * With interrupts masked this will cause a dead lock which is better
	 * than trying to execute whatever random garbage is in memory.
	 */
	// trap #1					/* Initial stack pointer */
	 jmp	0x400
	.balign 0x4

_initial_pc:
#ifdef CONFIG_MC68EZ328
	.long (CFG_SYS_FLASH_BASE + _start)	/* Start of program Code */
#elif defined(CONFIG_TARGET_QEMU)
	.long _start				/* Start of program Code */
#endif

. = 0xc
_address_exception:
	.long __addressexception

#ifdef CONFIG_MC68EZ328
. = 0x100
intc_vect_0:
	.long	dummy_intc_handler
intc_vect_1:
	.long	dummy_intc_handler
intc_vect_2:
	.long	dummy_intc_handler
intc_vect_3:
	.long	dummy_intc_handler
intc_vect_4:
	.long	dummy_intc_handler
intc_vect_5:
	.long	dummy_intc_handler
intc_vect_6:
	.long	dummy_intc_handler
intc_vect_7:
	.long	dummy_intc_handler
#endif

.balign 0x400
#endif

.text
.globl _start
_start:
	/* Make sure interrupts are disabled, and MSP is selected */
	//move.w	#0x3700,%sr

/*
 * Very early setup we do while running from flash
 * with no access to SDRAM.
 */

#ifdef CONFIG_MC68EZ328
_mc68ez328_startup:
	/* Disable the WDT to avoid getting reset */
	move.l #MC68EZ328_RTC_WDT, %a0
	clr.w (%a0)
_mc68ez328_startup_scr:
	/* Configure SCR to good state */
	move.l #MC68EZ328_SCR, (%a0)
	move.w #(MC68EZ328_SCR_BET + MC68EZ328_SCR_SO), (%a0)

	/*
	 * Clear port b and e to enable all dedicated functions
	 * so we get cas/ras for sdram, serial, dwe for sdram.
	 */
	move.l #MC68EZ328_PORTS, %a0
	clr.b %a0@(MC68EZ328_PORTS_PBSEL)
	clr.b %a0@(MC68EZ328_PORTS_PESEL)

	/* Really early UART init for debugging this code */
	move.w #0x126, (MC68EZ328_UART + MC68EZ328_UART_UBAUD)
	move.w #(MC68EZ328_UART_USTCNT_87 + MC68EZ328_UART_USTCNT_TXEN + MC68EZ328_UART_USTCNT_UEN), (MC68EZ328_UART + MC68EZ328_UART_USTCNT)
	__putch '.'

	/* Map flash */
	move.l #MC68EZ328_GROUPBASEADDR, %a0
	move.w #(CFG_SYS_FLASH_BASE >> 13), %a0@(MC68EZ328_CHIPSELECT_A)
	move.w #(MC68EZ328_CHIPSELECT_FLASH + MC68EZ328_CHIPSELECT_WS_1 + MC68EZ328_CHIPSELECT_SZ_8M + MC68EZ328_CHIPSELECT_BSW + MC68EZ328_CHIPSELECT_EN), %d0
	move.l #MC68EZ328_CHIPSELECT, %a1
	move.w %d0, %a1@(MC68EZ328_CHIPSELECT_A)

	/* SDRAM config */
	move.l #MC68EZ328_DRAMC, %a2
	move.w #0x8f00, %a2@(MC68EZ328_DRAMC_DRAMMC)
	move.w #0x9607, %a2@(MC68EZ328_DRAMC_DRAMC)
	/* Set base address of SDRAM to 0 */
	clr.w %a0@(MC68EZ328_GROUPBASEADDR_C)
	/* Configure chipselect D */
	move.w #(0x060e + MC68EZ328_CHIPSELECT_BSW + MC68EZ328_CHIPSELECT_WS_1 + MC68EZ328_CHIPSELECT_EN), %a1@(MC68EZ328_CHIPSELECT_D)
	__putch '_'
#elif defined(CONFIG_TARGET_QEMU)
#elif defined(CONFIG_TARGET_MVME147)
___xxx__:
	__putch 'm'
	__putch 'v'
	__putch 'm'
	__putch 'e'
	__putch '1'
	__putch '4'
	__putch '7'
#elif defined(CONFIG_TARGET_E17)
	//move.b #0, (E17_SERIAL_CAR)
	//move.b (E17_SERIAL_CMR), (0x400008)

	# Enable the transmitter
//1:
	//tst.b (E17_SERIAL_CCR)
	//bne 1b
	//move.b #CD2401_CCR_ENBXMTR, (E17_SERIAL_CCR)

	# The transmit interrupt must be enabled to transmit
	//move.b #CD2401_IER_TXEMPTY, (E17_SERIAL_IER)
	__putch 'e'
	__putch '1'
	__putch '7'
	__putch '\r'
	__putch '\n'
#else
#error "Make sure your target is handled properly in start.S"
#endif

#if defined(CONFIG_MC68EZ328) && !defined(CONFIG_SPL_BUILD) && 1
	/* Copy ourself from flash to SDRAM */
	move.l #0, %a0
	move.l #CFG_SYS_FLASH_BASE, %a1
	move.l %a1, %a2
	add.l  #_img_end, %a2
_copy_from_flash_next:
	cmp.l %a1, %a2
	beq _copy_from_flash_done
	move.l (%a1)+, (%a0)+
	bra _copy_from_flash_next
_copy_from_flash_done:

	move.l	#_from_sdram, %a1
	jmp (%a1)
_from_sdram:
#endif


	/* put relocation table address to a5 */
	move.l  #__got_start, %a5

	/* setup stack initially on top of the init ram */
	move.l	#(CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_RAM_SIZE), %sp

	/* BSS clear */
	move.l #__bss_start, %a0
	move.l #__bss_end, %a1
bss_clear:
	cmp.l %a0, %a1
	beq	bss_done
	clr.l (%a0)+
	bra bss_clear
bss_done:
	/* */

	__printch '!'

	/*
	 * if configured, malloc_f arena will be reserved first,
	 * then (and always) gd struct space will be reserved
	 */
	move.l %sp, -(%sp)
	move.l	#board_init_f_alloc_reserve, %a1
	jsr (%a1)
	__printch 'a'

	/* update stack and frame-pointers */
	move.l 	%d0, %sp
	move.l	%sp, %fp

	/* initialize reserved area */
	move.l	%d0, -(%sp)
	move.l	#board_init_f_init_reserve, %a1
	jsr (%a1)
	__printch 'b'

	/* run low-level CPU init code (from flash) */
	move.l #cpu_init_f, %a1
	jsr	(%a1)
	clr.l	%sp@-
	__printch 'c'

	/* run low-level board init code (from flash) */
	move.l	#board_init_f, %a1
	jsr	(%a1)
	__printch 'd'

	/* board_init_f() does not return unless we didn't relocate*/
	clr.l	%sp@-
	clr.l	%sp@-
	move.l	#board_init_r, %a1
	jsr	(%a1)

#define SAVE_ALL					\
	/* space for 15 regs */			\
	subl	#60,%sp;				\
	moveml	%d0-%d7/%a0-%a6,%sp@;

#define RESTORE_ALL					\
	moveml	%sp@,%d0-%d7/%a0-%a6;	\
	/* space for 15 regs */			\
	addl	#60,%sp; \
	rte;

/* exception code */
.globl _fault
_fault:
	bra	_fault

.globl __addressexception
__addressexception:
	__printch '@'
	jmp .


 .macro int_sled num group func
.globl _int_sled_\num
_int_sled_\num:
	SAVE_ALL
	/* push the location of the frame */
	movel	%sp,%sp@-
	/* push the group on the stack */
	movel	#\group,%sp@-
	/* push vector number on stack */
	movel	#\num,%sp@-
	movel	#\func, %a1
	jsr	(%a1)
	/* pop stack */
	addl	#12,%sp
	RESTORE_ALL
.endm

int_sled 4 1 exc_handler
int_sled 5 2 exc_handler
int_sled 8 1 exc_handler
int_sled 32 2 exc_handler

#if defined(CONFIG_MC68EZ328) && !defined(CONFIG_SPL_BUILD)
.globl dummy_intc_handler
dummy_intc_handler:
	move	#0x2700,%sr
	SAVE_ALL
	move.l  #dragonball_intc_maskall, %a1
	jsr (%a1)
	RESTORE_ALL
	rte
#endif

.align 4
