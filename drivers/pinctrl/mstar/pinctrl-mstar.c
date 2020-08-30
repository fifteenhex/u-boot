#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <linux/bitops.h>
#include <regmap.h>

#include "pinctrl-mstar.h"

DECLARE_GLOBAL_DATA_PTR;

struct mstar_pinctrl_priv {
	struct regmap *regmap;
};

struct mstar_pinctrl_selector {
	const char *groupname;
	const u16 val;
};

struct mstar_pinctrl_function {
	const char *name;
	const unsigned reg;
	const u16 mask;
	const struct mstar_pinctrl_selector *selectors;
	unsigned numselectors;
};

struct mstar_pinctrl_data {
	const struct mstar_pinctrl_function *functions;
	unsigned numfunctions;
	const char** groupnames;
	unsigned numgroupnames;
};

static const char* msc313_group_names[] = {
	GROUPNAME_ETH
};

static const struct mstar_pinctrl_selector selectors_eth[] = {
	{ .groupname = GROUPNAME_ETH, .val = BIT(2) },
};

static const struct mstar_pinctrl_function msc313_functions[] = {
	{
		.name = FUNCTIONNAME_ETH,
		.reg = REG_JTAG,
		.mask = MASK_ETH,
		.selectors = selectors_eth,
		.numselectors = ARRAY_SIZE(selectors_eth),
	},
};

static const struct mstar_pinctrl_data msc313_data = {
	.functions = msc313_functions,
	.numfunctions = ARRAY_SIZE(msc313_functions),
	.groupnames = msc313_group_names,
	.numgroupnames = ARRAY_SIZE(msc313_group_names),
};

static int mstar_pinctrl_get_groups_count(struct udevice *dev)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->numgroupnames;
}

static const char* mstar_pinctrl_get_group_name(struct udevice *dev, unsigned selector)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->groupnames[selector];
}

static int  mstar_pinctrl_get_functions_count(struct udevice *dev)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->numfunctions;
}

static const char* mstar_pinctrl_get_function_name(struct udevice *dev, unsigned selector)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->functions[selector].name;
}

static int mstar_pinctrl_pinmux_group_set(struct udevice *dev, unsigned group_selector, unsigned func_selector)
{
	struct mstar_pinctrl_priv *priv = dev_get_priv(dev);
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);
	const struct mstar_pinctrl_function *function = &data->functions[func_selector];
	const struct mstar_pinctrl_selector *selector;
	const char *groupname = data->groupnames[group_selector];
	int i = 0;

	printf("mstar pinctrl g %d, f %d\n", group_selector, func_selector);

	for(i = 0; i < function->numselectors; i++){
		selector = &function[i];
		if(strcmp(groupname, selector->groupname) == 0){
			printf("found selector\n");
			regmap_update_bits(priv->regmap, function->reg, function->mask, selector->val);
			return 0;
		}
	}

	return -EINVAL;
}

static struct pinctrl_ops mstar_pinctrl_ops = {
	.get_groups_count = mstar_pinctrl_get_groups_count,
	.get_group_name = mstar_pinctrl_get_group_name,
	.get_functions_count = mstar_pinctrl_get_functions_count,
	.get_function_name = mstar_pinctrl_get_function_name,
	.pinmux_group_set = mstar_pinctrl_pinmux_group_set,
	.set_state = pinctrl_generic_set_state,
};

int mstar_pinctrl_probe(struct udevice *dev)
{
	struct mstar_pinctrl_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem(dev_ofnode(dev), &priv->regmap);
	if(ret)
		goto out;

out:
	return ret;
}

static const struct udevice_id mstar_pinctrl_ids[] = {
	{ .compatible = "mstar,msc313-pinctrl", .data = &msc313_data },
	{ .compatible = "mstar,msc313e-pinctrl", .data = &msc313_data },
	{ }
};

U_BOOT_DRIVER(pinctrl_mstar) = {
	.name			= "pinctrl_mstar",
	.id			= UCLASS_PINCTRL,
	.of_match		= mstar_pinctrl_ids,
	.ops			= &mstar_pinctrl_ops,
	.probe			= mstar_pinctrl_probe,
	.priv_auto_alloc_size	= sizeof(struct mstar_pinctrl_priv),
};
