#include <common.h>
#include <dm.h>
#include <dm/pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

struct mstar_pinctrl_priv {
};

static int mstar_pinctrl_get_pins_count(struct udevice *dev)
{
	return 0;
}

static const char *mstar_pinctrl_get_pin_name(struct udevice *dev,
					      unsigned int selector)
{
	return NULL;
}

static int mstar_pinctrl_get_pin_muxing(struct udevice *dev,
					unsigned int selector,
					char *buf,
					int size)
{
	return 0;
}

int mstar_pinctrl_probe(struct udevice *dev)
{
	return 0;
}

static int mstar_pinctrl_set_state(struct udevice *dev, struct udevice *config)
{
	return 0;
}

static int mstar_pinctrl_set_state_simple(struct udevice *dev,
					  struct udevice *periph)
{
	return 0;
}

static struct pinctrl_ops mstar_pinctrl_ops = {
	.set_state		= mstar_pinctrl_set_state,
	.set_state_simple	= mstar_pinctrl_set_state_simple,
	.get_pin_name		= mstar_pinctrl_get_pin_name,
	.get_pins_count		= mstar_pinctrl_get_pins_count,
	.get_pin_muxing		= mstar_pinctrl_get_pin_muxing,
};

static const struct udevice_id mstar_pinctrl_ids[] = {
	{ .compatible = "mstar,msc313-pinctrl" },
	{ .compatible = "mstar,msc313e-pinctrl" },
	{ }
};

U_BOOT_DRIVER(pinctrl_mstar) = {
	.name			= "pinctrl_mstar",
	.id			= UCLASS_PINCTRL,
	.of_match		= mstar_pinctrl_ids,
	.ops			= &mstar_pinctrl_ops,
	.bind			= dm_scan_fdt_dev,
	.probe			= mstar_pinctrl_probe,
	.priv_auto_alloc_size	= sizeof(struct mstar_pinctrl_priv),
};
