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

void cpu_clk_setup(void)
{
	uint16_t temp;
#if 0
	  DAT_1f206005 = 0;
	                    /* this is inserting the current frequency */
	  DAT_1f206580 = 0xb9;
	  DAT_1f206581 = 0x1e;
	  DAT_1f206584 = 0x45;
	  DAT_1f206588 = 1;
	  DAT_1f206589 = 0;
	  DAT_1f206445 = 0;
	  DAT_1f206448 = 0x88;
	  delay?(0x4b0);
	  DAT_1f2041f0 = 1;
	  mstar_writew(0x84, L3BRIDGE + L3BRIDGE_04);
	  _DAT_1f206540 = 0x1eb9;
	  _DAT_1f206544 = 0x45;
	  return;
#endif

	  //m5
	  mstar_writew(0x0088, 0x1f206448);
	  temp = readw(0x1f206444);
	  mstar_writew(temp | 0x0100, 0x1f206444);
	  mstar_writew(0xAE14, CPUPLL + CPUPLL_CURFREQ_L);
	  mstar_writew(0x0067, CPUPLL + CPUPLL_CURFREQ_H);
	  mstar_writew(1, 0x1f206588);
	  mstar_writew(temp & ~0x0100, 0x1f206444);
	  mstar_delay(1000);

	  printf("waiting for cpupll lock..");

	  //while (!(readw(CPUPLL + CPUPLL_LPF_LOCK))) {
	//	  printf("waiting for cpupll lock\n");
	 // }

	  mstar_writew(0x0001, 0x1f2041f0);
	  mstar_writew(0x0484, 0x1f204404);
	  mstar_delay(1000);


}

static void mstar_cpu_clk_readback(void)
{
	uint16_t readback;

	mstar_writew(0x0001, 0x1f20442c);
	mstar_writew(0x0004, 0x1f203ddc);
	mstar_writew(0x4004, 0x1f203dd4);
	mstar_writew(0x0001, 0x1f203dd8);

	mstar_writew(0x0000, 0x1f203dc0);
	mstar_writew(0x8000, 0x1f203dc0);

	mstar_delay(100);

	readback = readw(0x1f203dc4);

	printf("readback: %04x\n", readback);

}

#define FREQ_400 0x0067AE14
#define FREQ_800 0x0043b3d5
#define FREQ_1000 0x002978d4
#define BUMPFREQ FREQ_400

void mstar_bump_cpufreq()
{
	uint16_t temp1, temp2;

	printf("attempting to bump cpu frequency\n");


	temp1 = readw(CPUPLL + CPUPLL_CURFREQ_L);
	temp2 = readw(CPUPLL + CPUPLL_CURFREQ_H);
	mstar_writew(temp1, CPUPLL + CPUPLL_LPF_LOW_L);
	mstar_writew(temp2, CPUPLL + CPUPLL_LPF_LOW_H);

	mstar_writew(BUMPFREQ & 0xFFFF, CPUPLL + CPUPLL_LPF_HIGH_BOTTOM);
	mstar_writew((BUMPFREQ >> 16) & 0xFFFF, CPUPLL + CPUPLL_LPF_HIGH_TOP);


	mstar_writew(0x1, CPUPLL + CPUPLL_LPF_MYSTERYONE);
	mstar_writew(0x6, CPUPLL + CPUPLL_LPF_MYSTERYTWO);
	mstar_writew(0x8, CPUPLL + CPUPLL_LPF_UPDATE_COUNT);
	mstar_writew(BIT(12), CPUPLL + CPUPLL_LPF_TRANSITIONCTRL);

	mstar_writew(0, CPUPLL + CPUPLL_LPF_TOGGLE);
	mstar_writew(1, CPUPLL + CPUPLL_LPF_TOGGLE);

	while (!(readw(CPUPLL + CPUPLL_LPF_LOCK))) {
		printf("waiting for cpupll lock\n");
	}

	mstar_cpu_clk_readback();
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
