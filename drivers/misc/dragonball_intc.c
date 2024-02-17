// SPDX-License-Identifier: GPL-2.0+
/*
 * Not real interrupt support. Just for bumping the VCO
 */

#include <dm.h>
#include <asm/io.h>

#define REG_IVR	0x0
#define REG_ICR 0x2
#define REG_IMR 0x4
#define REG_ISR 0xc

static void *base;

struct dragonball_intc_priv {
	void *base;
};

int dragonball_intc_unmask(struct udevice *dev, unsigned int which)
{
	struct dragonball_intc_priv *priv = dev_get_priv(dev);
	u32 mask = (1 << which);
	u32 imr;

	imr = readl(priv->base + REG_IMR);
	imr &= ~mask;
	writel(imr, priv->base + REG_IMR);

	return 0;
}

int dragonball_intc_mask(struct udevice *dev, unsigned int which)
{
	struct dragonball_intc_priv *priv = dev_get_priv(dev);
	u32 mask = (1 << which);
	u32 imr;

	imr = readl(priv->base + REG_IMR);
	imr |= mask;
	writel(imr, priv->base + REG_IMR);

	return 0;
}


static int dragonball_intc_probe(struct udevice *dev)
{
	struct dragonball_intc_priv *priv = dev_get_priv(dev);

	priv->base = (unsigned char*) dev_read_addr_ptr(dev);
	base = priv->base;

	/* Set the vector base to the start of the user vectors */
	writeb(0x40, priv->base + REG_IVR);
	writel(~0, priv->base + REG_IMR);

	return 0;
}

static const struct udevice_id dragonball_intc_ids[] = {
	{ .compatible = "motorola,mc68ez328-intc" },
	{ }
};

void dragonball_intc_maskall(void) __attribute__((used));
void dragonball_intc_maskall(void)
{
	writel(~0, base + REG_IMR);
}

U_BOOT_DRIVER(dragonball_intc_lpc) = {
	.name		= "dragonball-intc",
	.id			= UCLASS_MISC,
	.of_match	= dragonball_intc_ids,
	.probe		= dragonball_intc_probe,
	.priv_auto	= sizeof(struct dragonball_intc_priv),
};
