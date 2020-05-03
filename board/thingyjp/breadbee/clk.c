/*
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "clk.h"

/* this is a hack */
void mstar_early_clksetup()
{
	int i, j;
	uint16_t mystery_c0, mystery_f4, maybepll_04, uartclkgen;
	printf("doing early clock setup...\n");

	mystery_c0 = readw_relaxed(PMSLEEP + PMSLEEP_C0);
	mystery_f4 = readw_relaxed(PMSLEEP + PMSLEEP_F4);
	maybepll_04 = readw_relaxed(MAYBEPLL + MAYBEPLL_04);
	uartclkgen = readw_relaxed(CLKGEN + CLKGEN_UART);

	printf("mystery_c0: %04x, mystery_f4: %04x, maybepll_04 %04x, uart clkgen: %04x\n", mystery_c0,
			mystery_f4, maybepll_04, uartclkgen);

	writew_relaxed(0, PMSLEEP + PMSLEEP_F4);
	// vendor code has a delay here
	for(i = 0; i < 512; i++)
		printf(".");
	printf("\nwoot\n");

	writew_relaxed(0, MAYBEPLL + MAYBEPLL_04);
	// vendor code has a delay here
	for(i = 0; i < 10000; i++)
		printf(".");
	printf("\nw00t\n");

	mystery_c0 |= PMSLEEP_C0_BIT4;
	uartclkgen &= ~CLKGEN_UART_UART0MUX_MASK;

	writew_relaxed(mystery_c0, PMSLEEP + PMSLEEP_C0);
	writew_relaxed(uartclkgen, CLKGEN + CLKGEN_UART);

	printf("done\n");
}
