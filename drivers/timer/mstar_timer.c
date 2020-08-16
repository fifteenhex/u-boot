// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <timer.h>
#include <regmap.h>

#define TIMER_CTRL			0x0
#define TIMER_CTRL_EN			BIT(0)
#define TIMER_CTRL_CLR			BIT(3)
#define TIMER_COUNTER_L		0x10
#define TIMER_COUNTER_H		0x14

struct mstar_timer_priv {
	struct regmap *regmap;
	struct clk clk;
};

static int mstar_timer_get_count(struct udevice *dev, u64 *count)
{
	struct mstar_timer_priv *priv = dev_get_priv(dev);
	uint tmp;

	regmap_read(priv->regmap, TIMER_COUNTER_H, &tmp);
	*count = tmp << 16;
	regmap_read(priv->regmap, TIMER_COUNTER_L, &tmp);
	*count |= tmp;

	return 0;
}

static int mstar_timer_probe(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct mstar_timer_priv *priv = dev_get_priv(dev);
	u32 rate;
	int ret;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0)
		goto out;

	clk_enable(&priv->clk);

	uc_priv->clock_rate = clk_get_rate(&priv->clk);

	regmap_write(priv->regmap, TIMER_CTRL, TIMER_CTRL_CLR | TIMER_CTRL_EN);
out:
	return ret;
}

static const struct timer_ops mstar_timer_ops = {
	.get_count = mstar_timer_get_count,
};

static const struct udevice_id mstar_timer_ids[] = {
	{ .compatible = "mstar,timer" },
	{}
};

U_BOOT_DRIVER(mstar_timer) = {
	.name = "mstar_timer",
	.id = UCLASS_TIMER,
	.of_match = mstar_timer_ids,
	.priv_auto_alloc_size = sizeof(struct mstar_timer_priv),
	.probe = mstar_timer_probe,
	.ops = &mstar_timer_ops,
};

