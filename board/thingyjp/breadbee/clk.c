/*
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "clk.h"

static void mstar_clks_dumpreg(void)
{
	uint16_t mystery_c0, mystery_f4, maybepll_04,
	maybepll1_04, maybepll1_0c, uartclkgen;

	mystery_c0 = readw_relaxed(PMSLEEP + PMSLEEP_C0);
	mystery_f4 = readw_relaxed(PMSLEEP + PMSLEEP_F4);
	maybepll_04 = readw_relaxed(MAYBEPLL + MAYBEPLL_04);
	maybepll1_04 = readw_relaxed(MAYBEPLL1 + MAYBEPLL1_04);
	maybepll1_0c = readw_relaxed(MAYBEPLL1 + MAYBEPLL1_0C);
	uartclkgen = readw_relaxed(CLKGEN + CLKGEN_UART);

	printf("mystery_c0: %04x\n"
	       "mystery_f4: %04x\n"
	       "maybepll_04 %04x\n"
	       "maybepll1_04 %04x\n"
	       "maybepll1_0c %04x\n"
	       "uart clkgen: %04x\n",
	       mystery_c0, mystery_f4, maybepll_04, maybepll1_04, maybepll1_0c, uartclkgen);
}



/* this is a hack */
void mstar_early_clksetup()
{
	int i, j;
	uint16_t mystery_c0, mystery_f4, maybepll_04,
		maybepll1_04, maybepll1_0c, uartclkgen;

	printf("doing early clock setup...\n");

	printf("before:\n");
	mstar_clks_dumpreg();

	// this might be power control for the pll?
	writew_relaxed(0, PMSLEEP + PMSLEEP_F4);
	// vendor code has a delay here
	mdelay(10);

	// this seems to turn the pll that supplies mpll clocks
	writew_relaxed(0, MAYBEPLL + MAYBEPLL_04);

	// vendor code has a delay
	mdelay(10);

	printf("after:\n");
	mstar_clks_dumpreg();

	mystery_c0 |= PMSLEEP_C0_BIT4;
	uartclkgen &= ~CLKGEN_UART_UART0MUX_MASK;

	writew_relaxed(mystery_c0, PMSLEEP + PMSLEEP_C0);
	writew_relaxed(uartclkgen, CLKGEN + CLKGEN_UART);

	printf("done\n");
}
