// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <clk.h>
#include <clk-uclass.h>
#include <regmap.h>
#include <chenxingv7.h>

#define MAYBEPLL1_04			0x4
#define MAYBEPLL1_08			0x8
#define MAYBEPLL1_0C			0xc
#define MAYBEPLL1_10			0x10


struct mstar_miupll_magicnumbers {
	u16 pll_magic_08, pll_magic_0c, pll_magic_10;
};

struct mstar_miupll_priv {
	struct regmap *regmap;
	struct mstar_miupll_magicnumbers magicnumbers;
};

static int mstar_miupll_enable(struct clk *clk)
{
	struct mstar_miupll_priv *priv = dev_get_priv(clk->dev);

	/* seems to be power on, i3 ipl has this after setting the registers
	 * the i2 and m5 ipls has this before setting the registers.
	 */
	regmap_write(priv->regmap, MAYBEPLL1_04, 0);
	// vendor code has a delay
	mstar_delay(1000);

	if(priv->magicnumbers.pll_magic_08 > 0)
		regmap_write(priv->regmap, MAYBEPLL1_08, priv->magicnumbers.pll_magic_08);
	if(priv->magicnumbers.pll_magic_0c > 0)
		regmap_write(priv->regmap, MAYBEPLL1_0C, priv->magicnumbers.pll_magic_0c);
	if(priv->magicnumbers.pll_magic_10 > 0)
		regmap_write(priv->regmap, MAYBEPLL1_10, priv->magicnumbers.pll_magic_10);

	return 0;
}

static int mstar_miupll_disable(struct clk *clk)
{
	return 0;
}

const struct clk_ops mstar_miupll_ops = {
	.enable = mstar_miupll_enable,
	.disable = mstar_miupll_disable,
};

static int mstar_miupll_probe(struct udevice *dev)
{
	struct mstar_miupll_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	priv->magicnumbers.pll_magic_08 = -1;
	priv->magicnumbers.pll_magic_0c = -1;
	priv->magicnumbers.pll_magic_10 = -1;

	switch(mstar_chiptype()){
		case CHIPTYPE_MSC313E:
			priv->magicnumbers.pll_magic_0c = 0x22c;
		break;
		case CHIPTYPE_SSC8336:
		case CHIPTYPE_SSC8336N:
			priv->magicnumbers.pll_magic_08 = 0x0100;
			priv->magicnumbers.pll_magic_0c = 0x0216;
			priv->magicnumbers.pll_magic_10 = 0x0010;
		break;
		default:
			dev_err(dev, "Don't know miu pll config\n");
			ret = -EINVAL;
			goto out;
	}
out:
	return ret;
}

static const struct udevice_id mstar_miupll_ids[] = {
	{ .compatible = "mstar,miupll", },
	{ }
};

U_BOOT_DRIVER(mstar_miupll) = {
	.name = "mstar_miupll",
	.id = UCLASS_CLK,
	.of_match = mstar_miupll_ids,
	.probe = mstar_miupll_probe,
	.priv_auto_alloc_size = sizeof(struct mstar_miupll_priv),
	.ops = &mstar_miupll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
