// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <clk.h>
#include <clk-uclass.h>
#include <chenxingv7.h>
#include <regmap.h>
#include <syscon.h>

#define MAYBEPLL_04			0x04

static const ulong output_rates[] = {
		432000000,
		345000000,
		288000000,
		216000000,
		172000000,
		144000000,
		123000000,
		124000000,
		86000000
};


struct mstar_mpll_priv {
	struct regmap *regmap;
	struct regmap *pmsleep;
};

static ulong mstar_mpll_get_rate(struct clk *clk)
{
	return output_rates[clk->id];
}

static int mstar_mpll_enable(struct clk *clk)
{
	return 0;
}

static int mstar_mpll_disable(struct clk *clk)
{
	return 0;
}

const struct clk_ops mstar_mpll_ops = {
	.get_rate = mstar_mpll_get_rate,
	.enable = mstar_mpll_enable,
	.disable = mstar_mpll_disable,
};

static int mstar_mpll_probe(struct udevice *dev)
{
	struct mstar_mpll_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	priv->pmsleep = syscon_regmap_lookup_by_phandle(dev, "mstar,pmsleep");
	if(!priv->pmsleep){
		ret = -ENODEV;
		goto out;
	}

	// this might be power control for the pll?
	regmap_write(priv->pmsleep, PMSLEEP_F4, 0);
	// vendor code has a delay here
	mdelay(10);

	// this seems to turn the pll that supplies mpll clocks
	regmap_write(priv->regmap, MAYBEPLL_04, 0);
	// vendor code has a delay
	mdelay(10);

	// this too
	uint16_t tmp;
	tmp = readw_relaxed(CLKGEN_PM + CLKGEN_SPI_MCU_PM);
	tmp |= 0x5000;
	writew_relaxed(tmp, CLKGEN_PM + CLKGEN_SPI_MCU_PM);

out:
	return ret;
}

static const struct udevice_id mstar_mpll_ids[] = {
	{ .compatible = "mstar,mpll", },
	{ }
};

U_BOOT_DRIVER(mstar_mpll) = {
	.name = "mstar_mpll",
	.id = UCLASS_CLK,
	.of_match = mstar_mpll_ids,
	.probe = mstar_mpll_probe,
	.priv_auto_alloc_size = sizeof(struct mstar_mpll_priv),
	.ops = &mstar_mpll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
