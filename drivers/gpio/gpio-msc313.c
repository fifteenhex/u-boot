// SPDX-License-Identifier: GPL-2.0
/*
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/bitops.h>

#define MSC313_GPIO_IN  BIT(0)
#define MSC313_GPIO_OUT BIT(4)
#define MSC313_GPIO_OEN BIT(5)

#define OFF_FUART_RX	0x50
#define OFF_FUART_TX	0x54
#define OFF_FUART_CTS	0x58
#define OFF_FUART_RTS	0x5c

#define FUART_OFFSETS	\
	OFF_FUART_RX,	\
	OFF_FUART_TX,	\
	OFF_FUART_CTS,	\
	OFF_FUART_RTS

/*
 * ssd20x has the same pin names but different ordering
 * of the registers that control the gpio.
 */
#define SSD20XD_OFF_SD_D0	0x140
#define SSD20XD_OFF_SD_D1	0x144
#define SSD20XD_OFF_SD_D2	0x148
#define SSD20XD_OFF_SD_D3	0x14c
#define SSD20XD_OFF_SD_CMD	0x150
#define SSD20XD_OFF_SD_CLK	0x154

#define SSD20XD_SD_OFFSETS	SSD20XD_OFF_SD_CLK, \
				SSD20XD_OFF_SD_CMD, \
				SSD20XD_OFF_SD_D0,  \
				SSD20XD_OFF_SD_D1,  \
				SSD20XD_OFF_SD_D2,  \
				SSD20XD_OFF_SD_D3

#define SSD20XD_OFF_UART0_RX	0x60
#define SSD20XD_OFF_UART0_TX	0x64

#define SSD20XD_UART0_OFFSETS \
	SSD20XD_OFF_UART0_RX, \
	SSD20XD_OFF_UART0_TX

#define SSD20XD_OFF_UART1_RX	0x68
#define SSD20XD_OFF_UART1_TX	0x6c

#define SSD20XD_UART1_OFFSETS \
	SSD20XD_OFF_UART1_RX, \
	SSD20XD_OFF_UART1_TX

#define SSD20XD_TTL_OFFSET_TTL0		0x80
#define SSD20XD_TTL_OFFSET_TTL1		0x84
#define SSD20XD_TTL_OFFSET_TTL2		0x88
#define SSD20XD_TTL_OFFSET_TTL3		0x8c
#define SSD20XD_TTL_OFFSET_TTL4		0x90
#define SSD20XD_TTL_OFFSET_TTL5		0x94
#define SSD20XD_TTL_OFFSET_TTL6		0x98
#define SSD20XD_TTL_OFFSET_TTL7		0x9c
#define SSD20XD_TTL_OFFSET_TTL8		0xa0
#define SSD20XD_TTL_OFFSET_TTL9		0xa4
#define SSD20XD_TTL_OFFSET_TTL10	0xa8
#define SSD20XD_TTL_OFFSET_TTL11	0xac
#define SSD20XD_TTL_OFFSET_TTL12	0xb0
#define SSD20XD_TTL_OFFSET_TTL13	0xb4
#define SSD20XD_TTL_OFFSET_TTL14	0xb8
#define SSD20XD_TTL_OFFSET_TTL15	0xbc
#define SSD20XD_TTL_OFFSET_TTL16	0xc0
#define SSD20XD_TTL_OFFSET_TTL17	0xc4
#define SSD20XD_TTL_OFFSET_TTL18	0xc8
#define SSD20XD_TTL_OFFSET_TTL19	0xcc
#define SSD20XD_TTL_OFFSET_TTL20	0xd0
#define SSD20XD_TTL_OFFSET_TTL21	0xd4
#define SSD20XD_TTL_OFFSET_TTL22	0xd8
#define SSD20XD_TTL_OFFSET_TTL23	0xdc
#define SSD20XD_TTL_OFFSET_TTL24	0xe0
#define SSD20XD_TTL_OFFSET_TTL25	0xe4
#define SSD20XD_TTL_OFFSET_TTL26	0xe8
#define SSD20XD_TTL_OFFSET_TTL27	0xec

#define SSD20XD_TTL_OFFSETS SSD20XD_TTL_OFFSET_TTL0,  \
			    SSD20XD_TTL_OFFSET_TTL1,  \
			    SSD20XD_TTL_OFFSET_TTL2,  \
			    SSD20XD_TTL_OFFSET_TTL3,  \
			    SSD20XD_TTL_OFFSET_TTL4,  \
			    SSD20XD_TTL_OFFSET_TTL5,  \
			    SSD20XD_TTL_OFFSET_TTL6,  \
			    SSD20XD_TTL_OFFSET_TTL7,  \
			    SSD20XD_TTL_OFFSET_TTL8,  \
			    SSD20XD_TTL_OFFSET_TTL9,  \
			    SSD20XD_TTL_OFFSET_TTL10, \
			    SSD20XD_TTL_OFFSET_TTL11, \
			    SSD20XD_TTL_OFFSET_TTL12, \
			    SSD20XD_TTL_OFFSET_TTL13, \
			    SSD20XD_TTL_OFFSET_TTL14, \
			    SSD20XD_TTL_OFFSET_TTL15, \
			    SSD20XD_TTL_OFFSET_TTL16, \
			    SSD20XD_TTL_OFFSET_TTL17, \
			    SSD20XD_TTL_OFFSET_TTL18, \
			    SSD20XD_TTL_OFFSET_TTL19, \
			    SSD20XD_TTL_OFFSET_TTL20, \
			    SSD20XD_TTL_OFFSET_TTL21, \
			    SSD20XD_TTL_OFFSET_TTL22, \
			    SSD20XD_TTL_OFFSET_TTL23, \
			    SSD20XD_TTL_OFFSET_TTL24, \
			    SSD20XD_TTL_OFFSET_TTL25, \
			    SSD20XD_TTL_OFFSET_TTL26, \
			    SSD20XD_TTL_OFFSET_TTL27

#define SSD20XD_GPIO_OFF_GPIO0 0x0
#define SSD20XD_GPIO_OFF_GPIO1 0x4
#define SSD20XD_GPIO_OFF_GPIO2 0x8
#define SSD20XD_GPIO_OFF_GPIO3 0xc
#define SSD20XD_GPIO_OFF_GPIO4 0x10
#define SSD20XD_GPIO_OFF_GPIO5 0x14
#define SSD20XD_GPIO_OFF_GPIO6 0x18
#define SSD20XD_GPIO_OFF_GPIO7 0x1c
#define SSD20XD_GPIO_OFF_GPIO10 0x28
#define SSD20XD_GPIO_OFF_GPIO11 0x2c
#define SSD20XD_GPIO_OFF_GPIO12 0x30
#define SSD20XD_GPIO_OFF_GPIO13 0x34
#define SSD20XD_GPIO_OFF_GPIO14 0x38
#define SSD20XD_GPIO_OFF_GPIO85 0x100
#define SSD20XD_GPIO_OFF_GPIO86 0x104
#define SSD20XD_GPIO_OFF_GPIO90 0x114

#define SSD20XD_GPIO_OFFSETS SSD20XD_GPIO_OFF_GPIO0,  \
			     SSD20XD_GPIO_OFF_GPIO1,  \
			     SSD20XD_GPIO_OFF_GPIO2,  \
			     SSD20XD_GPIO_OFF_GPIO3,  \
			     SSD20XD_GPIO_OFF_GPIO4,  \
			     SSD20XD_GPIO_OFF_GPIO5,  \
			     SSD20XD_GPIO_OFF_GPIO6,  \
			     SSD20XD_GPIO_OFF_GPIO7,  \
			     SSD20XD_GPIO_OFF_GPIO10, \
			     SSD20XD_GPIO_OFF_GPIO11, \
			     SSD20XD_GPIO_OFF_GPIO12, \
			     SSD20XD_GPIO_OFF_GPIO13, \
			     SSD20XD_GPIO_OFF_GPIO14, \
			     SSD20XD_GPIO_OFF_GPIO85, \
			     SSD20XD_GPIO_OFF_GPIO86, \
			     SSD20XD_GPIO_OFF_GPIO90

static const unsigned int ssd20xd_offsets[] = {
	FUART_OFFSETS,
	SSD20XD_SD_OFFSETS,
	SSD20XD_UART0_OFFSETS,
	SSD20XD_UART1_OFFSETS,
	SSD20XD_TTL_OFFSETS,
	SSD20XD_GPIO_OFFSETS,
};

struct msc313_gpio_info {
	const unsigned int *offsets;
	unsigned int noffsets;
};

static const struct msc313_gpio_info ssd20xd_info = {
	.offsets = &ssd20xd_offsets,
	.noffsets = ARRAY_SIZE(ssd20xd_offsets),
};

struct msc313_gpio_chip {
	void __iomem *regs;
	const struct msc313_gpio_info *info;
};

static int msc313_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	struct msc313_gpio_chip *priv = dev_get_priv(dev);
	u16 tmp;

	tmp = readw(priv->regs + priv->info->offsets[offset]);
	tmp |= MSC313_GPIO_OEN;
	writew(tmp, priv->regs + priv->info->offsets[offset]);

	return 0;
}

static int msc313_gpio_direction_output(struct udevice *dev, unsigned offset,
				     int value)
{
	struct msc313_gpio_chip *priv = dev_get_priv(dev);
	u16 tmp;

	tmp = readw(priv->regs + priv->info->offsets[offset]);
	tmp &= ~MSC313_GPIO_OEN;
	if(value)
		tmp |= MSC313_GPIO_OUT;
	else
		tmp &= !MSC313_GPIO_OUT;
	writew(tmp, priv->regs + priv->info->offsets[offset]);

	return 0;
}

static int msc313_gpio_get_value(struct udevice *dev, unsigned int offset)
{
	struct msc313_gpio_chip *priv = dev_get_priv(dev);

	if (readw(priv->regs + priv->info->offsets[offset]) & MSC313_GPIO_IN)
		return 1;

	return 0;
}

static int msc313_gpio_set_value(struct udevice *dev, unsigned int offset,
			     int value)
{
	return msc313_gpio_direction_output(dev, offset, value);
}

static int msc313_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	struct msc313_gpio_chip *priv = dev_get_priv(dev);

	if (readw(priv->regs + priv->info->offsets[offset]) & MSC313_GPIO_OEN)
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static int msc313_gpio_gpio_probe(struct udevice  *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct msc313_gpio_chip *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr_ptr(dev);
	priv->info = dev_get_driver_data(dev);
	uc_priv->gpio_count = priv->info->noffsets;

	return 0;
}

static const struct dm_gpio_ops msc313_gpio_gpio_ops = {
	.direction_input	= msc313_gpio_direction_input,
	.direction_output	= msc313_gpio_direction_output,
	.set_value		= msc313_gpio_set_value,
	.get_value		= msc313_gpio_get_value,
	.get_function		= msc313_gpio_get_function,
};

static const struct udevice_id msc313_gpio_gpio_ids[] = {
#ifdef CONFIG_MSTAR_INFINITY1
	{
		.compatible = "mstar,msc313-gpio",
		.data = NULL, //fixme
	},
#endif
#ifdef CONFIG_MSTAR_INFINITY2M
	{
		.compatible = "sstar,ssd20xd-gpio",
		.data = &ssd20xd_info,
	},
#endif
	{ }
};

U_BOOT_DRIVER(gpio_msc313) = {
	.name		= "gpio_msc313",
	.id		= UCLASS_GPIO,
	.ops		= &msc313_gpio_gpio_ops,
	.of_match	= msc313_gpio_gpio_ids,
	.probe		= msc313_gpio_gpio_probe,
	.priv_auto	= sizeof(struct msc313_gpio_chip),
};
