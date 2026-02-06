// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */

#include <asm/io.h>
#include <dm.h>
#include <linux/errno.h>
#include <mapmem.h>
#include <timer.h>

struct everdrive_timer_priv {
	void __iomem *base;
};

static u64 everdrive_timer_get_count(struct udevice *dev)
{
	struct everdrive_timer_priv *priv = dev_get_priv(dev);

	return readw(priv->base);
}

static int everdrive_timer_probe(struct udevice *dev)
{
	struct everdrive_timer_priv *priv = dev_get_priv(dev);
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	uc_priv->clock_rate = 1000;

	return 0;
}

static const struct udevice_id everdrive_timer_ids[] = {
	{ .compatible = "krikzz,everdrive-timer" },
	{ }
};

static const struct timer_ops everdrive_timer_ops = {
	.get_count = everdrive_timer_get_count,
};

U_BOOT_DRIVER(goldfish_timer) = {
	.name	= "everdrive_timer",
	.id	= UCLASS_TIMER,
	.of_match = everdrive_timer_ids,
	.ops	= &everdrive_timer_ops,
	.probe	= everdrive_timer_probe,
	.priv_auto = sizeof(struct everdrive_timer_priv),

};
