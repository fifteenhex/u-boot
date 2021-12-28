// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/device_compat.h>
#include <asm/io.h>
#include <wdt.h>

#define REG_WDT_CLR			0x0
#define REG_WDT_MAX_PRD_L		0x10
#define REG_WDT_MAX_PRD_H		0x14

struct msc313e_wdt_priv {
	void __iomem *base;
};

static int msc313e_wdt_reset(struct udevice *dev)
{
	struct msc313e_wdt_priv *priv = dev_get_priv(dev);

	writew(1, priv->base + REG_WDT_CLR);

	return 0;
}

static int msc313e_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct msc313e_wdt_priv *priv = dev_get_priv(dev);
	u32 timeoutval = (timeout * 12000000) / 1000;

	writew(timeoutval & 0xffff, priv->base + REG_WDT_MAX_PRD_L);
	writew((timeoutval >> 16) & 0xffff, priv->base + REG_WDT_MAX_PRD_H);
	writew(1, priv->base + REG_WDT_CLR);

	return 0;
}

static int msc313e_wdt_probe(struct udevice *dev)
{
	struct msc313e_wdt_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static const struct wdt_ops msc313e_wdt_ops = {
	.start = msc313e_wdt_start,
	.reset = msc313e_wdt_reset,
};

static const struct udevice_id msc313e_wdt_ids[] = {
	{ .compatible = "mstar,msc313e-wdt" },
	{}
};

U_BOOT_DRIVER(wdt_gpio) = {
	.name		= "wdt_msc313e",
	.id		= UCLASS_WDT,
	.of_match	= msc313e_wdt_ids,
	.ops		= &msc313e_wdt_ops,
	.probe		= msc313e_wdt_probe,
	.priv_auto	= sizeof(struct msc313e_wdt_priv),
};
