// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/device_compat.h>
#include <wdt.h>
#include <linux/delay.h>
#include <regmap.h>
#include <syscon.h>

#define PCC8_WDTCTRL 0x5

#define WDTCTRL_ENABLE	 BIT(0)
#define WDTCTRL_CLEAR	 BIT(1)
#define WDTCTRL_RESET	 BIT(2)
#define WDTCTRL_TIMEDOUT BIT(3)

// WDTCTRL_RESET
#define ENABLE_OR_RESET (WDTCTRL_ENABLE | WDTCTRL_CLEAR |    WDTCTRL_TIMEDOUT)

struct pcc_wdt_priv {
	struct regmap *pcc8;
};

static int pcc_wdt_reset(struct udevice *dev)
{
	struct pcc_wdt_priv *priv = dev_get_priv(dev);

	regmap_write(priv->pcc8, PCC8_WDTCTRL, ENABLE_OR_RESET |
										   (0xf << 4));

	return 0;
}

static int pcc_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct pcc_wdt_priv *priv = dev_get_priv(dev);

	regmap_write(priv->pcc8, PCC8_WDTCTRL, ENABLE_OR_RESET |
										   (0xf << 4));

	return 0;
}

static int pcc_wdt_stop(struct udevice *dev)
{
	struct pcc_wdt_priv *priv = dev_get_priv(dev);
	uint ctrl;

	regmap_read(priv->pcc8, PCC8_WDTCTRL, &ctrl);
	regmap_write(priv->pcc8, PCC8_WDTCTRL, ctrl & ~WDTCTRL_ENABLE);

	return 0;
}

static int pcc_wdt_probe(struct udevice *dev)
{
	struct pcc_wdt_priv *priv = dev_get_priv(dev);

	priv->pcc8 = syscon_regmap_lookup_by_phandle(dev, "pcc8");
	if (IS_ERR(priv->pcc8))
		return -ENODEV;

	return 0;
}

static const struct wdt_ops pcc_wdt_ops = {
	.start = pcc_wdt_start,
	.stop  = pcc_wdt_stop,
	.reset = pcc_wdt_reset,
};

static const struct udevice_id pcc_wdt_ids[] = {
	{ .compatible = "motorola,mvme147-pcc-wdt" },
	{}
};

U_BOOT_DRIVER(wdt_gpio) = {
	.name      = "wdt_pcc",
	.id        = UCLASS_WDT,
	.of_match  = pcc_wdt_ids,
	.ops       = &pcc_wdt_ops,
	.probe     = pcc_wdt_probe,
	.priv_auto = sizeof(struct pcc_wdt_priv),
};
