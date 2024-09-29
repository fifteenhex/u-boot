// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/device_compat.h>
#include <wdt.h>
#include <linux/delay.h>

struct mmio_wdt_priv {
	bool always_running;
};

static int mmio_wdt_reset(struct udevice *dev)
{
	struct mmio_wdt_priv *priv = dev_get_priv(dev);

	return 0;
}

static int mmio_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct mmio_wdt_priv *priv = dev_get_priv(dev);

	if (priv->always_running)
		return 0;

	return -ENOSYS;
}

static int dm_probe(struct udevice *dev)
{
	struct mmio_wdt_priv *priv = dev_get_priv(dev);
	int ret;
	//const char *algo = dev_read_string(dev, "hw_algo");


	priv->always_running = dev_read_bool(dev, "always-running");
	if (priv->always_running)
		ret = mmio_wdt_reset(dev);

	return ret;
}

static const struct wdt_ops mmio_wdt_ops = {
	.start = mmio_wdt_start,
	.reset = mmio_wdt_reset,
};

static const struct udevice_id mmio_wdt_ids[] = {
	{ .compatible = "linux,wdt-gpio" },
	{}
};

U_BOOT_DRIVER(wdt_gpio) = {
	.name = "wdt_mmio",
	.id = UCLASS_WDT,
	.of_match = mmio_wdt_ids,
	.ops = &mmio_wdt_ops,
	.probe	= dm_probe,
	.priv_auto = sizeof(struct mmio_wdt_priv),
};
