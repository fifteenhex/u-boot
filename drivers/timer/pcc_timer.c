// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdt_support.h>
#include <timer.h>
#include <regmap.h>
#include <syscon.h>
#include <cyclic.h>

#define CTRL_ENABLE BIT(0)
#define CTRL_ENACNT BIT(1)
#define CTRL_CLROVF BIT(2)
#define CTRL_OVF_SHIFT 4
#define CTRL_OVR_MASK 0xf

#define PCC16_TIMER1_PRELOAD	0
#define PCC16_TIMER1_COUNT		2
#define PCC8_TIMER1_INTCTRL		0
#define PCC8_TIMER1_CTRL		1

#define ENABLE_AND_CLEAR (CTRL_ENABLE | CTRL_ENACNT | CTRL_CLROVF)

struct pcc_timer_priv {
	struct regmap *pcc16, *pcc8;
	u8 lastoverflows;
	u64 overflows, lastvalue;
	struct cyclic_info cyclic_info;
};

static void pcc_timer_collect_overflows(struct pcc_timer_priv *priv)
{
	uint ctrl, overflows;

	/* Messing with the ctrl bits to clear the overflows seems to
	 * cause missing an overflow sometimes, disabling the counter
	 * to read the overflows and clear them while avoid missing
	 * an overflow seems to make the overflows always read as
	 * zero.
	 *
	 * So,.. detect when overflows overflows instead.
	 */

	regmap_read(priv->pcc8, PCC8_TIMER1_CTRL, &ctrl);
	overflows = (ctrl >> CTRL_OVF_SHIFT) & CTRL_OVR_MASK;

	if (overflows == priv->lastoverflows)
		return;

	if (overflows < priv->lastoverflows)
		priv->overflows = ((priv->overflows >> 4) + 1) << 4;

	priv->lastoverflows = overflows;
	priv->overflows = ((priv->overflows >> 4) << 4) | (overflows & 0xf);
}

static void pcc_cyclic(struct cyclic_info *cyclic)
{
	struct pcc_timer_priv *priv = container_of(cyclic, struct pcc_timer_priv, cyclic_info);

	pcc_timer_collect_overflows(priv);
}

static u64 pcc_timer_get_count(struct udevice *dev)
{
	struct pcc_timer_priv *priv = dev_get_priv(dev);
	u64 ticks;
	uint cnt;

	regmap_read(priv->pcc16, PCC16_TIMER1_COUNT, &cnt);
	pcc_timer_collect_overflows(priv);

	/*
	 * It seems like sometimes the counter rolls over before
	 * the overflow is counted? Don't let the timer value
	 * go backwards
	 */
	ticks = (priv->overflows << 16) | (cnt & 0xffff);
	if (ticks < priv->lastvalue)
		return priv->lastvalue;

	priv->lastvalue = ticks;

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

	cyclic_register(&priv->cyclic_info, pcc_cyclic, 10000000, dev->name);

	priv->lastoverflows = 0;
	priv->lastvalue = 0;
	priv->overflows = 0;

	regmap_write(priv->pcc16, PCC16_TIMER1_PRELOAD, 0);
	regmap_write(priv->pcc8, PCC8_TIMER1_CTRL, 0);
	regmap_write(priv->pcc8, PCC8_TIMER1_CTRL, ENABLE_AND_CLEAR);

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
