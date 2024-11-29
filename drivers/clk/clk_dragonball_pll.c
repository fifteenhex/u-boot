// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <asm/io.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <misc.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>

#define DRV_NAME "dragonball_pll"

#define REG_PLLCR	0x0
#define SYSCLK_PRESC_SHIFT	5
#define SYSCLK_PRESC_MASK	1
#define SYSCLK_SEL_SHIFT	8
#define SYSCLK_SEL_MASK		0x7
#define LCDCLK_SEL_SHIFT	11
#define LCDCLK_SEL_MASK		0x7
#define REG_PLLFSR	0x2
#define PC_SHIFT	0
#define PC_MASK		0xff
#define QC_SHIFT	8
#define QC_MASK		0xf

struct dragonball_pll_output {
	struct dragonball_pll_priv *priv;
	struct clk clk;
};

#define to_pll_output(_clk) container_of(_clk, struct dragonball_pll_output, clk)

struct dragonball_pll_priv {
	void *base;
	struct clk osc;
	const char *output_names[3];
	struct dragonball_pll_output outputs[3];

	ulong dmaclk_rate, sysclk_rate, lcdclk_rate;
};

static void dragonball_pll_recalc_rates(struct dragonball_pll_priv *priv)
{
	u16 pllcr = readw(priv->base + REG_PLLCR);
	u16 pllfsr = readw(priv->base + REG_PLLFSR);

	/* Get the parameters need to work out the frequencies */
	unsigned int presc = (pllcr >> SYSCLK_PRESC_SHIFT) & SYSCLK_PRESC_MASK;
	unsigned int sysclk_sel = (pllcr >> SYSCLK_SEL_SHIFT) & SYSCLK_SEL_MASK;
	unsigned int lcdclk_sel = (pllcr >> LCDCLK_SEL_SHIFT) & LCDCLK_SEL_MASK;
	unsigned int pc = (pllfsr >> PC_SHIFT) & PC_MASK;
	unsigned int qc = (pllfsr >> QC_SHIFT) & QC_MASK;

	/* Work out the vco freq */
	unsigned long divisor = 14 * (pc + 1) + qc + 1;
	unsigned long vco_freq = clk_get_rate(&priv->osc) * divisor;

	/* Work out the divisors */
	unsigned long sysclk_div = (sysclk_sel & 0x4) ? 1 : (2 << sysclk_sel);
	unsigned long lcdclk_div = (lcdclk_sel & 0x4) ? 1 : (2 << lcdclk_sel);

	/* Work out the output rates */
	priv->dmaclk_rate = vco_freq / (presc + 1);
	priv->sysclk_rate = priv->dmaclk_rate / sysclk_div;
	priv->lcdclk_rate = priv->dmaclk_rate / lcdclk_div;

	debug("dmaclk: %lu, sysclk %lu, lcdclk %lu\n",
			priv->dmaclk_rate, priv->sysclk_rate, priv->lcdclk_rate);

	/* Update global data */
	gd->cpu_clk = priv->sysclk_rate;
	gd->bd->bi_intfreq = priv->sysclk_rate;
}

static ulong dragonball_pll_get_rate(struct clk *clk)
{
	struct dragonball_pll_priv *priv = dev_get_priv(clk->dev);

	switch(clk->id){
	case 0:
		return priv->dmaclk_rate;
	case 1:
		return priv->sysclk_rate;
	case 2:
		return priv->lcdclk_rate;
	}

	return -EINVAL;
}

#ifndef CONFIG_SPL_BUILD
static ulong dragonball_pll_set_rate(struct clk *clk, ulong rate)
{
	printf("%s:%d\n", __func__, __LINE__);

	return rate;
}
#endif

static struct clk_ops dragonball_pll_ops = {
	.get_rate = dragonball_pll_get_rate,
#ifndef CONFIG_SPL_BUILD
	.set_rate = dragonball_pll_set_rate,
#endif
};

extern void dragonball_timer_arm_timer_wakeup(struct udevice *timerdev, struct udevice *intcdev);
extern void dragonball_timer_disarm_timer_wakeup(struct udevice *timerdev, struct udevice *intcdev);

int dragonball_pll_beastmode(struct udevice *plldev,
			     struct udevice *timerdev,
			     struct udevice *intcdev)
{
	struct dragonball_pll_priv *priv = dev_get_priv(plldev);
	u16 pllcr = readw(priv->base + REG_PLLCR);
	u16 pllfsr = readw(priv->base + REG_PLLFSR);

	return 0;

	/* Disable the by 2 divider on the VCO output */
	pllcr &= ~(SYSCLK_PRESC_MASK << SYSCLK_PRESC_SHIFT);
	writew(pllcr, priv->base + REG_PLLCR);

	/*
	 * Increase the VCO frequency as documented by
	 * Motorola.
	 */
	// 0x23, 0x1 -- default
	// 0x30, 0x7 -- 22mhz -- seems ok
	// 0x35, 0xc -- 25mhz -- seems ok
	// 0x47, 0x4 -- >30mhz, uart no work, kernel is booting and running though, PLL too far off?
	pllfsr &= ~(PC_MASK << PC_SHIFT);
	pllfsr |= 0x35;
	pllfsr &= ~(QC_MASK << QC_SHIFT);
	pllfsr |= (0xc << QC_SHIFT);

	dragonball_timer_arm_timer_wakeup(timerdev, intcdev);

#if 1
	asm volatile ("1: btst.b #7, (%[pllfsr])\n"
		      "beq.s 1b\n"
		      "2: btst.b #7, (%[pllfsr])\n"
		      "bne.s 2b\n"
		      "move.w %[value], (%[pllfsr])\n"
		      "stop  #0x2000\n"
			:
			: [pllcr] "a" (priv->base + REG_PLLCR),
			  [pllfsr] "a" (priv->base + REG_PLLFSR),
			  [value] "d" (pllfsr)
			:);
#endif

	dragonball_timer_disarm_timer_wakeup(timerdev, intcdev);

	dragonball_pll_recalc_rates(priv);

	return 0;
};

static int dragonball_pll_probe(struct udevice *dev)
{
	struct dragonball_pll_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr_ptr(dev);

	ret = clk_get_by_name(dev, "osc", &priv->osc);
	if (ret)
		return ret;

	dragonball_pll_recalc_rates(priv);

	ret = dev_read_string_count(dev, "clock-output-names");
	if (ret < 0)
		return ret;
	if (ret != ARRAY_SIZE(priv->outputs))
		return -EINVAL;

	for (int i = 0; i < ARRAY_SIZE(priv->outputs); i++) {
		struct dragonball_pll_output *output = &priv->outputs[i];
		struct clk *clk = &output->clk;

		output->priv = priv;

		ret = dev_read_string_index(dev, "clock-output-names", i,
				&priv->output_names[i]);
		if (ret)
			return ret;

		ret = clk_register(clk, DRV_NAME, priv->output_names[i],
				dev->name);
		if (ret)
			return ret;
	}

	return ret;
}

static const struct udevice_id dragonball_pll_ids[] = {
	{ .compatible = "motorola,mc68ez328-pll", },
	{}
};

U_BOOT_DRIVER(dragonball_pll) = {
	.name = DRV_NAME,
	.id = UCLASS_CLK,
	.of_match = dragonball_pll_ids,
	.ops = &dragonball_pll_ops,
	.priv_auto	= sizeof(struct dragonball_pll_priv),
	.probe = dragonball_pll_probe,
};
