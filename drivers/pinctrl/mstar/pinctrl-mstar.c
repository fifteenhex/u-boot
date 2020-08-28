#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

struct mstar_pinctrl_priv {
};

struct mstar_pinctrl_group {
	const char *name;
};

struct mstar_pinctrl_function {
	const char *name;
};

struct mstar_pinctrl_data {
	const struct mstar_pinctrl_group *groups;
	unsigned numgroups;
	const struct mstar_pinctrl_function *functions;
	unsigned numfunctions;
};

static const struct mstar_pinctrl_group msc313_groups[] = {
	{ .name = "eth" },
};

static const struct mstar_pinctrl_function msc313_functions[] = {
	{ .name = "eth" },
};

static const struct mstar_pinctrl_data msc313_data = {
	.groups = msc313_groups,
	.numgroups = ARRAY_SIZE(msc313_groups),
	.functions = msc313_functions,
	.numfunctions = ARRAY_SIZE(msc313_functions),
};

static int mstar_pinctrl_get_groups_count(struct udevice *dev)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->numgroups;
}

static const char* mstar_pinctrl_get_group_name(struct udevice *dev, unsigned selector)
{
	struct mstar_pinctrl_data *data = dev_get_driver_data(dev);

	return data->groups[selector].name;
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
	printf("mstar pinctrl g %d, f %d\n", group_selector, func_selector);

	return 0;
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
	return 0;
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
