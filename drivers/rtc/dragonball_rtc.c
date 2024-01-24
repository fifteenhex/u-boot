// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 *
 */

#include <clk.h>
#include <dm.h>
#include <rtc.h>
#include <asm/io.h>

struct dragonball_rtc_priv {
	void *base;
	struct clk osc;
};

static int dragonball_rtc_get(struct udevice *dev, struct rtc_time *tm)
{
	struct dragonball_rtc_priv *priv = dev_get_priv(dev);

	return 0;
}

static int dragonball_rtc_set(struct udevice *dev, const struct rtc_time *tm)
{
	return 0;
}

static int dragonball_rtc_reset(struct udevice *dev)
{
	return 0;
}

static int dragonball_rtc_probe(struct udevice *dev)
{
	struct dragonball_rtc_priv *priv = dev_get_priv(dev);
	struct clk clk;
	int ret;

	priv->base = dev_read_addr(dev);

	ret = clk_get_by_name(dev, "osc", &priv->osc);
	if (ret)
		return ret;

	return 0;
}

static const struct rtc_ops dragonball_rtc_ops = {
	.get = dragonball_rtc_get,
	.set = dragonball_rtc_set,
	.reset = dragonball_rtc_reset,
};

static const struct udevice_id dragonball_rtc_ids[] = {
	{ .compatible = "motorola,mc68ez328-rtc" },
	{ }
};

U_BOOT_DRIVER(rtc_dragonball) = {
	.name	= "rtc-dragonball",
	.id	= UCLASS_RTC,
	.probe	= dragonball_rtc_probe,
	.of_match = dragonball_rtc_ids,
	.ops	= &dragonball_rtc_ops,
	.priv_auto	= sizeof(struct dragonball_rtc_priv),
};
