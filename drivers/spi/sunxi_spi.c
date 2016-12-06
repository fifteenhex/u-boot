/*
 * Copyright (c) 2016 Daniel Palmer <daniel@0x0f.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <spi.h>
#include <asm/gpio.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

// sun6i registers
#define SUN6I_GCR			0x04
#define SUN6I_TCR			0x08
#define SUN6I_CCTL			0x24
#define SUN6I_FIFO_STA		0x1C
#define SUN6I_MBC			0x30
#define SUN6I_MTC			0x34
#define SUN6I_BCC			0x38
#define SUN6I_TXD			0x200
#define SUN6I_RXD			0x300


#define SUN6I_GCR_ENABLE	BIT(0)
#define SUN6I_GCR_MASTER	BIT(1)
#define SUN6I_GCR_SRST		BIT(31)

#define SUN6I_TCR_CPHA		BIT(0)
#define SUN6I_TCR_CPOL		BIT(1)
#define SUN6I_TCR_SS_OWNER	BIT(6)
#define SUN6I_TCR_SS_LEVEL	BIT(7)
#define SUN6I_TCR_XCH		BIT(31)

#define SUN6I_BUS_SOFT_RST_REG0     (0x01C20000 + 0x2C0)

struct sunxi_spi_platdata {
	s32 frequency;
	fdt_addr_t regs;
};

struct sunxi_spi_priv {
	unsigned fifodepth;
	void* gcr;
	void* tcr;

	void* mbc;
	void* mtc;
	void* bcc;

	void* txd;
	void* rxd;
};

static int sunxi_spi_claim_bus(struct udevice *dev)
{
	return 0;
}

static int sunxi_spi_release_bus(struct udevice *dev)
{
	return 0;
}

static int sunxi_spi_xfer(struct udevice *dev, unsigned int bitlen,
			 const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct sunxi_spi_priv *priv = dev_get_priv(bus);

	unsigned totalbytes, pos, loopbytes, loopbyte;
	char value;

	if (bitlen % 8 != 0)
		return -EINVAL;

	totalbytes = bitlen / 8;

	if (flags & SPI_XFER_BEGIN)
		clrbits_le32(priv->tcr, SUN6I_TCR_SS_LEVEL);

	for (pos = 0; pos < totalbytes; pos += priv->fifodepth) {
		loopbytes = min(priv->fifodepth, totalbytes - pos);
		writel(loopbytes, priv->mbc);
		writel(loopbytes, priv->mtc);
		writel(loopbytes, priv->bcc);

		for (loopbyte = 0; loopbyte < loopbytes; loopbyte++) {
			value = *(((char*) dout) + pos + loopbyte);
			writeb(value, priv->txd);
		}

		setbits_le32(priv->tcr, SUN6I_TCR_XCH);
		while (readl(priv->tcr) & SUN6I_TCR_XCH)
			;

		for (loopbyte = 0; loopbyte < loopbytes; loopbyte++) {
			value = readb(priv->rxd);
			*(((char*) din) + pos + loopbyte) = value;
		}
	}

	if (flags & SPI_XFER_END)
		setbits_le32(priv->tcr, SUN6I_TCR_SS_LEVEL);

	return 0;
}

static int sunxi_spi_set_speed(struct udevice *dev, unsigned int speed)
{
	return 0;
}

static int sunxi_spi_set_mode(struct udevice *dev, unsigned int mode)
{
	struct sunxi_spi_priv *priv = dev_get_priv(dev);
	clrsetbits_le32(priv->tcr, SUN6I_TCR_CPOL | SUN6I_TCR_CPHA,
			mode);
	return 0;
}

static const struct dm_spi_ops sunxi_spi_ops = {
	.claim_bus	= sunxi_spi_claim_bus,
	.release_bus	= sunxi_spi_release_bus,
	.xfer		= sunxi_spi_xfer,
	.set_speed	= sunxi_spi_set_speed,
	.set_mode	= sunxi_spi_set_mode,
};

static int sunxi_spi_ofdata_to_platdata(struct udevice *dev)
{
	struct sunxi_spi_platdata *plat = dev->platdata;
	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;

	plat->regs = dev_get_addr(dev);
	plat->frequency = fdtdec_get_int(blob, node, "spi-max-frequency",
			500000);

	return 0;
}

// stolen from SPL driver

#define CCM_AHB_GATING0             (0x01C20000 + 0x60)
#define CCM_SPI0_CLK                (0x01C20000 + 0xA0)
#define SUN6I_BUS_SOFT_RST_REG0     (0x01C20000 + 0x2C0)

#define AHB_RESET_SPI0_SHIFT        20
#define AHB_GATE_OFFSET_SPI0        20

static void sunxi_spi_clock(struct sunxi_spi_priv *priv){
	/* Deassert SPI0 reset on SUN6I */
	setbits_le32(SUN6I_BUS_SOFT_RST_REG0,
			     (1 << AHB_RESET_SPI0_SHIFT));

	/* Open the SPI0 gate */
	setbits_le32(CCM_AHB_GATING0, (1 << AHB_GATE_OFFSET_SPI0));
	/* 24MHz from OSC24M */
	writel((1 << 31), CCM_SPI0_CLK);
}

static void sunxi_spi_pinmux(struct sunxi_spi_priv *priv){
	unsigned int pin;
	for (pin = SUNXI_GPC(0); pin <= SUNXI_GPC(3); pin++)
		sunxi_gpio_set_cfgpin(pin, SUNXI_GPC_SPI0);
}

static void sunxi_spi_reset(struct sunxi_spi_priv *priv){
	setbits_le32(priv->gcr, SUN6I_GCR_SRST);
	while (readl(priv->gcr) & SUN6I_GCR_SRST)
		;
	setbits_le32(priv->gcr, SUN6I_GCR_MASTER | SUN6I_GCR_ENABLE);

	// configure for software controlled CS
	setbits_le32(priv->tcr, SUN6I_TCR_SS_LEVEL | SUN6I_TCR_SS_OWNER);
}

static int sunxi_spi_probe(struct udevice *dev)
{
	struct sunxi_spi_platdata *plat = dev->platdata;
	struct sunxi_spi_priv *priv = dev_get_priv(dev);

	printf("sunxi probe, regs @0x%x\n", (unsigned) plat->regs);

	priv->fifodepth = 64;

	priv->gcr = (void*) (plat->regs + SUN6I_GCR);
	priv->tcr = (void*) (plat->regs + SUN6I_TCR);

	priv->mbc = (void*) (plat->regs + SUN6I_MBC);
	priv->mtc = (void*) (plat->regs + SUN6I_MTC);
	priv->bcc = (void*) (plat->regs + SUN6I_BCC);

	priv->txd = (void*) (plat->regs + SUN6I_TXD);
	priv->rxd = (void*) (plat->regs + SUN6I_RXD);

	sunxi_spi_clock(priv);
	sunxi_spi_pinmux(priv);
	sunxi_spi_reset(priv);

	return 0;
}

static const struct udevice_id sunxi_spi_ids[] = {
	{ .compatible = "allwinner,sun8i-h3-spi" },
	{ }
};

U_BOOT_DRIVER(sunxi_spi) = {
	.name = "sunxi_spi",
	.id	= UCLASS_SPI,
	.of_match = sunxi_spi_ids,
	.ops = &sunxi_spi_ops,
	.ofdata_to_platdata = sunxi_spi_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct sunxi_spi_platdata),
	.priv_auto_alloc_size = sizeof(struct sunxi_spi_priv),
	.probe = sunxi_spi_probe,
};
