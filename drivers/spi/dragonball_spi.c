// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 *
 */

#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <spi.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <linux/bitfield.h>

#define FORCE_INLINE inline __attribute__((always_inline))

#define REG_DATA			0x0
#define REG_DATA_BITS			16
#define REG_CONT			0x2
#define REG_CONT_BIT_COUNT_MASK		0xf
#define REG_CONT_POL			BIT(4)
#define REG_CONT_PHA			BIT(5)
#define REG_CONT_IRQEN			BIT(6)
#define REG_CONT_IRQ			BIT(7)
#define REG_CONT_XCH			BIT(8)
#define REG_CONT_ENABLE			BIT(9)
#define REG_CONT_DATARATE_MASK		0x7
#define REG_CONT_DATARATE_SHIFT		13

#define MAX_CS_COUNT			4

struct dragonball_spi_priv {
	void *base;
	struct clk sysclk;
	struct gpio_desc cs_gpios[MAX_CS_COUNT];
};

static int dragonball_spi_claim_bus(struct udevice *slave)
{
	return 0;
}

static int dragonball_spi_release_bus(struct udevice *slave)
{
	/*
	 * Disabling the controller stops it driving the pins.
	 * Probably don't want that.
	 */
	return 0;
}

static int dragonball_spi_set_mode(struct udevice *bus, uint mode)
{
	struct dragonball_spi_priv *priv = dev_get_priv(bus);
	u16 cont;

	cont = readw(priv->base + REG_CONT);
	cont &= ~(REG_CONT_POL | REG_CONT_PHA);

	switch(mode)
	{
	case 0:
		break;
	case 1:
		cont |= REG_CONT_PHA;
		break;
	case 2:
		cont |= REG_CONT_POL;
		break;
	case 3:
		cont |= (REG_CONT_POL | REG_CONT_PHA);
		break;
	default:
		return -EINVAL;
	}

	writew(cont, priv->base + REG_CONT);

	return 0;
}

static int dragonball_spi_set_speed(struct udevice *bus, uint hz)
{
	struct dragonball_spi_priv *priv = dev_get_priv(bus);
	ulong base_rate= clk_get_rate(&priv->sysclk);
	int i, divisor;
	u16 cont;

	for (i = 0, divisor = 4; i < 8; i++, divisor *= 2) {
		if ((base_rate / divisor) <= hz)
			break;
	}

	cont = readw(priv->base + REG_CONT);
	cont &= ~(REG_CONT_DATARATE_MASK << REG_CONT_DATARATE_SHIFT);
	cont |= (i & REG_CONT_DATARATE_MASK) << REG_CONT_DATARATE_SHIFT;

	writew(cont, priv->base + REG_CONT);

	return 0;
}

static int dragonball_spi_set_cs(struct udevice *dev, unsigned int cs, bool enable)
{
	struct dragonball_spi_priv *priv = dev_get_priv(dev);

	if (cs >= MAX_CS_COUNT)
		return -ENODEV;

	if (!dm_gpio_is_valid(&priv->cs_gpios[cs]))
		return -EINVAL;

	return dm_gpio_set_value(&priv->cs_gpios[cs], enable ? 1 : 0);
}

static FORCE_INLINE void dragonball_spi_setbits(struct dragonball_spi_priv *priv,
										  unsigned int bits)
{
	u16 cont;

	cont = readw(priv->base + REG_CONT);
	cont &= ~REG_CONT_BIT_COUNT_MASK;
	cont |= bits - 1;

	writew(cont, priv->base + REG_CONT);
}

static FORCE_INLINE u16 dragonball_spi_setdata(struct dragonball_spi_priv *priv,
					       unsigned int bits, const void* dout, unsigned offset)
{
	const u8* b = dout + offset;
	/*
	 * Seems like a good idea to transmit all 1s if there is no data.
	 * mmc_spi.c at least seems to expect this.
	 */
	u16 w = ~0;

	if (dout) {
		w = b[0] & 0xff;

		if (bits > 8) {
			w <<= 8;
			w |= b[1] & 0xff;
		}
	}

	return w;
}

static FORCE_INLINE void dragonball_spi_getdata(struct dragonball_spi_priv *priv,
						unsigned int bits, void* din, unsigned offset,
						u16 w)
{
	u8* b = din + offset;

	if (!din)
		return;


	if (bits > 8) {
		b[1] = w & 0xff;
		w >>= 8;
	}

	b[0] = w & 0xff;
}

static FORCE_INLINE void dragonball_spi_waitidle(struct dragonball_spi_priv *priv)
{
	u16 cont = readw(priv->base + REG_CONT);

	do {
		cont = readw(priv->base + REG_CONT);
	} while (cont & REG_CONT_XCH);
}

static FORCE_INLINE void dragonball_spi_startxfer(struct dragonball_spi_priv *priv)
{
	u16 cont = readw(priv->base + REG_CONT);

	cont |= REG_CONT_XCH;
	cont &= ~REG_CONT_IRQ;
	writew(cont, priv->base + REG_CONT);
	/*
	 * To get the best performance we need to be setting up the next transfer
	 * while the controller is running, so DONT wait for it to finish here.
	 */
}

static FORCE_INLINE void dragonball_spi_one(struct dragonball_spi_priv *priv,
					    const void *dout, void *din,
					    const unsigned int bits, const unsigned int offset)
{
	u16 w;

	w = dragonball_spi_setdata(priv, bits, dout, offset);
	writew(w, priv->base + REG_DATA);

	dragonball_spi_startxfer(priv);
	dragonball_spi_waitidle(priv);
	w = readw(priv->base + REG_DATA);
	dragonball_spi_getdata(priv, bits, din, offset, w);
}

static int dragonball_spi_xfer(struct udevice *slave, unsigned int bitlen,
			  const void *dout, void *din, unsigned long flags) __attribute__ ((optimize(2)));
static int dragonball_spi_xfer(struct udevice *slave, unsigned int bitlen,
			  const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev_get_parent(slave);
	struct dragonball_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_plat *slave_plat;
	const unsigned int bytesperword = REG_DATA_BITS / 8;
	unsigned int partial = bitlen % REG_DATA_BITS;
	unsigned int words = (bitlen - partial) / REG_DATA_BITS;
	int ret;

	//printf("%s:%d bits %d, do 0x%p di 0x%p flags %lu - 0x%p\n", __func__, __LINE__,
	//		bitlen, dout, din, flags, priv);

	slave_plat = dev_get_parent_plat(slave);

	if (flags & SPI_XFER_BEGIN) {
		/* make sure mosi is high before cs */
		writew(0xffff, priv->base + REG_DATA);
		ret = dragonball_spi_set_cs(bus, slave_plat->cs, true);
		udelay(10);
		if (ret)
			return ret;
	}

	/* Do 16 bit xfers first */
	dragonball_spi_setbits(priv, REG_DATA_BITS);

	/* Interleaved path */
	if (words > 1) {
		int i;
		u16 nextout;
		unsigned int lastoffset = 0;

		/* Manually do the first word */
		nextout = dragonball_spi_setdata(priv, REG_DATA_BITS, dout, 0);
		writew(nextout, priv->base + REG_DATA);
		dragonball_spi_startxfer(priv);
		/* Pre-fetch the next word */
		nextout = dragonball_spi_setdata(priv, REG_DATA_BITS, dout, bytesperword);

		for (i = 1; i < words; i++) {
			const unsigned int nextoffset = (i + 1) * bytesperword;
			u16 in;

			/* Wait for the previous transfer to complete */
			dragonball_spi_waitidle(priv);
			/* Stash the read in value */
			in = readw(priv->base + REG_DATA);
			/* Kick the next transfer */
			writew(nextout, priv->base + REG_DATA);
			dragonball_spi_startxfer(priv);

			/* Process the read in data */
			dragonball_spi_getdata(priv, REG_DATA_BITS, din, lastoffset, in);
			lastoffset += bytesperword;
			/* Pre-fetch the next word */
			if ((i + 1) != words)
				nextout = dragonball_spi_setdata(priv, REG_DATA_BITS, dout, nextoffset);
		}

		/* Wait for the final transfer to complete */
		dragonball_spi_waitidle(priv);
		/* Process the read in for the last transfer */
		dragonball_spi_getdata(priv, REG_DATA_BITS, din, lastoffset, readw(priv->base + REG_DATA));
	}
	/* Single word */
	else if (words) {
		dragonball_spi_one(priv, dout, din, REG_DATA_BITS, 0);
	}

	/* Send the last part, probably the odd 8 bits */
	if (partial) {
		const unsigned int offset = words * bytesperword;
		dragonball_spi_setbits(priv, partial);
		dragonball_spi_one(priv, dout, din, partial, offset);
	}

	dragonball_spi_waitidle(priv);

	if (flags & SPI_XFER_END) {
		/* make sure mosi is high before cs */
		writew(0xffff, priv->base + REG_DATA);
		ret = dragonball_spi_set_cs(bus, slave_plat->cs, false);
		if (ret)
			return ret;
		udelay(10);
	}

	return 0;
}

static int dragonball_spi_probe(struct udevice *dev)
{
	struct dragonball_spi_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr_ptr(dev);

	ret = clk_get_by_name(dev, "sysclk", &priv->sysclk);
	if (ret)
		return ret;

	ret = gpio_request_list_by_name(dev, "cs-gpios", priv->cs_gpios,
			ARRAY_SIZE(priv->cs_gpios), GPIOD_IS_OUT);
	if (ret < 0)
		return -ENOENT;

	clk_prepare_enable(&priv->sysclk);
	clk_get_rate(&priv->sysclk);

	// enable
	u16 cont = readw(priv->base + REG_CONT);
	cont |= REG_CONT_ENABLE;
	writew(cont, priv->base + REG_CONT);

	return 0;
};

static const struct dm_spi_ops dragonball_spi_ops = {
	.claim_bus	= dragonball_spi_claim_bus,
	.release_bus	= dragonball_spi_release_bus,
	.set_mode	= dragonball_spi_set_mode,
	.set_speed	= dragonball_spi_set_speed,
	.xfer		= dragonball_spi_xfer,
};

static const struct udevice_id dragonball_spi_ids[] = {
	{ .compatible = "motorola,mc68ez328-spi", },
	{ }
};

U_BOOT_DRIVER(stm32_spi) = {
	.name			= "dragonball_spi",
	.id			= UCLASS_SPI,
	.of_match		= dragonball_spi_ids,
	.ops			= &dragonball_spi_ops,
	.priv_auto		= sizeof(struct dragonball_spi_priv),
	.probe			= dragonball_spi_probe,
};
