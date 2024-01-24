// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <dm.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/gpio.h>
#include <linux/bitops.h>

#define GPIOPERPORT				8
#define PORTS					7

#define PORT_OFFSET				0x8
#define REG_DIRDATA				0x0
#define REG_DIRDATA_DIR_SHIFT	8
#define REG_DIRDATA_DIR_MASK	0xff
#define REG_PUENSEL				0x2

#define PORTD	3

#define gpio_to_port(__gpio) (__gpio >> 3)
#define gpio_to_portoffset(_gpio) (gpio_to_port(_gpio) * PORT_OFFSET)
#define gpio_to_bitmask(_gpio) (BIT(_gpio & 0x7))

struct dragonball_gpio_plat {
	void *base;
};

static int dragonball_gpio_get_value(struct udevice *dev, u32 offset)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct dragonball_gpio_plat *plat = dev_get_plat(dev);
	unsigned int port = gpio_to_portoffset(offset);
	u16 mask = gpio_to_bitmask(offset);
	u16 reg;

	if (offset > uc_priv->gpio_count)
		return -EINVAL;

	reg = readw(plat->base + port + REG_DIRDATA);

	return (reg & mask) ? 1 : 0;
}

static int dragonball_gpio_set_flags(struct udevice *dev, unsigned int offset, ulong flags)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct dragonball_gpio_plat *plat = dev_get_plat(dev);
	unsigned int port = gpio_to_portoffset(offset);
	u16 datamask = gpio_to_bitmask(offset);
	u16 dirmask = (gpio_to_bitmask(offset) << REG_DIRDATA_DIR_SHIFT);
	u16 datadir, puensel;

	if (offset > uc_priv->gpio_count)
		return -1;

	/* Update the direction and value */
	datadir = readw(plat->base + port + REG_DIRDATA);
	datadir &= ~(dirmask | datamask);
	if (flags & GPIOD_IS_OUT) {
		datadir |= dirmask;

		if (flags & GPIOD_IS_OUT_ACTIVE)
			datadir |= datamask;
	}

	/* Update the pullup value */
	puensel = readw(plat->base + port + REG_PUENSEL);
	puensel &= ~dirmask;
	if (flags & GPIOD_PULL_UP)
		puensel |= dirmask;
	// todo, this is just making sure IO mode is selected for outputs
	// make this work for inputs too
	if (flags & (GPIOD_IS_OUT /*| GPIOD_IS_IN*/))
		puensel |= datamask;

	/* Write new values */
	writew(datadir, plat->base + port + REG_DIRDATA);
	writew(puensel, plat->base + port + REG_PUENSEL);

	return 0;
}

static int dragonball_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct dragonball_gpio_plat *plat = dev_get_plat(dev);
	unsigned int port = gpio_to_portoffset(offset);
	u16 selmask = gpio_to_bitmask(offset);
	u16 dirmask = (gpio_to_bitmask(offset) << REG_DIRDATA_DIR_SHIFT);
	u16 datadir, puensel;

	if (offset > uc_priv->gpio_count)
		return -1;

	puensel = readw(plat->base + port + REG_PUENSEL);

	/*
	 * The bottom 4 bits of PDSEL are always 0 but those
	 * pins are always GPIO.
	 */
	if (gpio_to_port(offset) == PORTD)
		puensel |= 0xf;

	/*
	 * If the bit is *not* set the dedicated function
	 * is active.
	 */
	if (!(puensel & selmask))
		return GPIOF_FUNC;

	datadir = readw(plat->base + port + REG_DIRDATA);
	if (datadir & dirmask)
		return GPIOF_OUTPUT;

	return 0;
}

static int dragonball_gpio_xlate(struct udevice *dev, struct gpio_desc *desc,
								 struct ofnode_phandle_args *args)
{
	unsigned int port;
	unsigned int pin;

	if (args->args_count != 3)
		return -EINVAL;

	port = args->args[0];
	pin =  args->args[1];

	desc->offset = (port * GPIOPERPORT) + pin;
	desc->flags = gpio_flags_xlate(args->args[2]);

	return 0;
}

static const struct udevice_id dragonball_gpio_match[] = {
	{ .compatible = "motorola,mc68ez328-gpio" },
	{ }
};

static const struct dm_gpio_ops dragonball_gpio_ops = {
	.get_value = dragonball_gpio_get_value,
	.set_flags = dragonball_gpio_set_flags,
	.get_function = dragonball_gpio_get_function,
	.xlate = dragonball_gpio_xlate,
};

static int dragonball_gpio_probe(struct udevice *dev)
{
	struct dragonball_gpio_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	char name[18], *str;

	sprintf(name, "gpio@%4lx_", (uintptr_t)plat->base);
	str = strdup(name);
	if (!str)
		return -ENOMEM;
	uc_priv->bank_name = str;
	uc_priv->gpio_count = GPIOPERPORT * PORTS;

	return 0;
}

static int dragonball_gpio_of_to_plat(struct udevice *dev)
{
	struct dragonball_gpio_plat *plat = dev_get_plat(dev);

	plat->base = dev_read_addr_ptr(dev);
	if (!plat->base)
		return -EINVAL;

	return 0;
}

U_BOOT_DRIVER(gpio_dragonball) = {
	.name	= "gpio_dragonball",
	.id	= UCLASS_GPIO,
	.of_match = dragonball_gpio_match,
	.of_to_plat = of_match_ptr(dragonball_gpio_of_to_plat),
	.plat_auto	= sizeof(struct dragonball_gpio_plat),
	.ops	= &dragonball_gpio_ops,
	.probe	= dragonball_gpio_probe,
};
