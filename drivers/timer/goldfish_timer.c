// SPDX-License-Identifier: GPL-2.0+
/*
 * See: https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 */

#include <dm.h>
#include <asm/io.h>
#include <timer.h>

#define REG_TIME_LOW		0x00
#define REG_TIME_HIGH		0x04
#define REG_ALARM_LOW		0x08
#define REG_ALARM_HIGH		0x0c
#define REG_CLEAR_INTERRUPT	0x10
#define REG_CLEAR_ALARM		0x14

struct goldfish_timer_priv {
	void __iomem *base;
};

static int goldfish_timer_probe(struct udevice *dev)
{
	struct goldfish_timer_priv *priv = dev_get_priv(dev);
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	priv->base = (unsigned char*) dev_read_addr_ptr(dev);
	uc_priv->clock_rate = 1000000000;

	return 0;
}

static const struct udevice_id goldfish_timer_ids[] = {
	{ .compatible = "google,goldfish-timer" },
	{ }
};

static u64 goldfish_timer_get_count(struct udevice *dev)
{
	struct goldfish_timer_priv *priv = dev_get_priv(dev);
	u64 count;

	u32 low = readl(priv->base + REG_TIME_LOW);
	u32 high = readl(priv->base + REG_TIME_HIGH);

	count = high;
	count <<= 32;
	count += low;

	return count;
}

static const struct timer_ops goldfish_timer_ops = {
	.get_count = goldfish_timer_get_count,
};

U_BOOT_DRIVER(goldfish_timer) = {
	.name		= "goldfish_timer",
	.id		= UCLASS_TIMER,
	.of_match	= goldfish_timer_ids,
	.probe		= goldfish_timer_probe,
	.ops		= &goldfish_timer_ops,
	.priv_auto	= sizeof(struct goldfish_timer_priv),
};
