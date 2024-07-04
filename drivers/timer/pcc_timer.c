// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <clk-uclass.h>
#include <common.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdt_support.h>
#include <timer.h>
#include <regmap.h>
#include <syscon.h>

#define CTRL_ENABLE BIT(0)
#define CTRL_ENACNT BIT(1)
#define CTRL_CLROVF BIT(2)
#define CTRL_OVF_SHIFT 4
#define CTRL_OVR_MASK 0xf

struct pcc_timer_priv {
	struct regmap *pcc16, *pcc8;
	uint32_t overflows;
};

static u64 pcc_timer_get_count(struct udevice *dev)
{
	struct pcc_timer_priv *priv = dev_get_priv(dev);
	uint ctrl, cnt;
	u64 ticks;

	regmap_read(priv->pcc16, 2, &cnt);
	regmap_read(priv->pcc8, 1, &ctrl);
	regmap_write(priv->pcc8, 1, CTRL_ENACNT |
								CTRL_ENABLE |
								CTRL_CLROVF);

	priv->overflows += (ctrl >> CTRL_OVF_SHIFT) & CTRL_OVR_MASK;

	ticks = (priv->overflows << 16);
	ticks += cnt;

	return ticks;
}

static int pcc_timer_probe(struct udevice *dev)
{
	struct pcc_timer_priv *priv = dev_get_priv(dev);

	priv->pcc16 = syscon_regmap_lookup_by_phandle(dev, "pcc16");
	if (IS_ERR(priv->pcc16))
		return -ENODEV;

	priv->pcc8 = syscon_regmap_lookup_by_phandle(dev, "pcc8");
	if (IS_ERR(priv->pcc8))
		return -ENODEV;

	regmap_write(priv->pcc8, 1, CTRL_ENACNT |
							    CTRL_ENABLE |
								CTRL_CLROVF);

	return 0;
}

static const struct timer_ops pcc_timer_ops = {
	.get_count = pcc_timer_get_count,
};

static const struct udevice_id pcc_timer_ids[] = {
	{ .compatible = "motorola,mvme147-pcc-timer", },
	{ }
};

U_BOOT_DRIVER(pcc_timer) = {
	.name      = "pcc_timer",
	.id        = UCLASS_TIMER,
	.of_match  = of_match_ptr(pcc_timer_ids),
	.probe     = pcc_timer_probe,
	.ops       = &pcc_timer_ops,
	.flags     = DM_FLAG_PRE_RELOC,
	.priv_auto = sizeof(struct pcc_timer_priv),
};
