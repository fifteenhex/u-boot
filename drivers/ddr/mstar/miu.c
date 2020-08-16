#include <common.h>
#include <dm.h>
#include <ram.h>
#include <regmap.h>
#include <clk.h>

#include <chenxingv7.h>

#include "ddr.h"

#ifndef CONFIG_MSTAR_IPL

#ifndef CONFIG_SPL_RAM
#error "SPL_RAM must be selected"
#endif

struct mstar_miu_priv {
	struct regmap	*ana;
	struct regmap	*dig;
	struct clk	miupll;
};

static int mstar_miu_probe(struct udevice *dev)
{
	struct mstar_miu_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->ana, 0);
	if(ret)
		goto out;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->dig, 1);
	if(ret)
		goto out;

	ret = clk_get_by_name(dev, "miupll", &priv->miupll);
	if(ret)
		goto out;

	clk_enable(&priv->miupll);

	mstar_ddr_init(mstar_chiptype(), priv->ana, priv->dig);

out:
	return ret;
}

static int mstar_miu_get_info(struct udevice *dev,
				      struct ram_info *info)
{
	return 0;
}

static const struct ram_ops mstar_miu_ops = {
	.get_info = mstar_miu_get_info,
};

static const struct udevice_id mstar_miu_ids[] = {
	{ .compatible = "mstar,msc313-miu" },
	{ .compatible = "mstar,ssc8336-miu" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(mstar_miu) = {
	.name = "mstar_miu",
	.id = UCLASS_RAM,
	.of_match = mstar_miu_ids,
	.ops = &mstar_miu_ops,
	.probe = mstar_miu_probe,
	.priv_auto_alloc_size = sizeof(struct mstar_miu_priv),
};

#endif
