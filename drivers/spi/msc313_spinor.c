// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <spi.h>
#include <asm/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/clk-provider.h>
#include <dm/device_compat.h>
#include <log.h>

#define REG_PASSWORD			0x0
#define VAL_PASSWORD_UNLOCK		0xAAAA
#define VAL_PASSWORD_LOCK		0x5555
#define REG_SPI_WDATA			0x10
#define REG_SPI_RDATA			0x14
#define REG_SPI_CLKDIV			0x18
#define VAL_SPI_CLKDIV_128		0x400
#define REG_SPI_CECLR			0x20
#define BIT_SPI_CECLR_CLEAR		BIT(0)
#define REG_SPI_RDREQ			0x30
#define BIT_SPI_RDREQ_REQ		BIT(0)
#define REG_SPI_RD_DATARDY		0x54
#define BIT_SPI_RD_DATARDY_READY	BIT(0)
#define REG_SPI_WR_DATARDY		0x58
#define BIT_SPI_WR_DATARDY_READY	BIT(0)
#define REG_TRIGGER_MODE		0xa8
#define VAL_TRIGGER_MODE_ENABLE		0x3333
#define VAL_TRIGGER_MODE_DISABLE	0x2222

struct msc313_spinor_platdata {
	fdt_addr_t reg_base;
	fdt_addr_t mem_base;
};

struct msc313_spinor_priv {
	void *regs;
	unsigned long *mem_base;
};

static int msc313_spinor_claim_bus(struct udevice *dev)
{
	/* nothing to do */
	return 0;
}

static int msc313_spinor_release_bus(struct udevice *dev)
{
	/* nothing to do */
	return 0;
}

static void msc313_spinor_spi_transaction_start(struct msc313_spinor_priv *priv){
	writew(VAL_PASSWORD_UNLOCK, priv->regs + REG_PASSWORD);
	writew(VAL_TRIGGER_MODE_ENABLE, priv->regs + REG_TRIGGER_MODE);
}

static void msc313_spinor_spi_writebyte(struct msc313_spinor_priv *priv, u8 value)
{
	writeb(value, priv->regs + REG_SPI_WDATA);
	while(!(readb(priv->regs + REG_SPI_WR_DATARDY) & BIT_SPI_WR_DATARDY_READY)){

	};
}

static void msc313_spinor_spi_readbyte(struct msc313_spinor_priv *priv, u8 *dest)
{
	u8 b;

	writeb(BIT_SPI_RDREQ_REQ, priv->regs + REG_SPI_RDREQ);
	while(!(readb(priv->regs + REG_SPI_RD_DATARDY) & BIT_SPI_RD_DATARDY_READY)){
	}

	b = readb(priv->regs + REG_SPI_RDATA);
	*dest = b;
}

static void msc313_spinor_spi_transaction_end(struct msc313_spinor_priv *priv){
	writeb(BIT_SPI_CECLR_CLEAR, priv->regs + REG_SPI_CECLR);
	/* Make sure the slave actually sees the CS edge */
	udelay(5);
	writew(VAL_TRIGGER_MODE_DISABLE, priv->regs + REG_TRIGGER_MODE);
	writew(VAL_PASSWORD_LOCK, priv->regs + REG_PASSWORD);
}

static int msc313_spinor_xfer(struct udevice *dev, unsigned int bitlen,
			 const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct msc313_spinor_priv *priv = dev_get_priv(bus);
	const u8* txd = dout;
	u8* rxd = din;
	unsigned bytes = bitlen / 8;
	int i;

	if (flags & SPI_XFER_BEGIN)
		msc313_spinor_spi_transaction_start(priv);

	for(i = 0; i < bytes; i++){
		if(txd)
			msc313_spinor_spi_writebyte(priv, *txd++);
		else if(rxd)
			msc313_spinor_spi_readbyte(priv, rxd++);
	}

	if (flags & SPI_XFER_END)
		msc313_spinor_spi_transaction_end(priv);

	return  0;
}

static int msc313_spinor_set_speed(struct udevice *bus, uint speed)
{
	/* nothing to do */

	printf("set speed: %u\n", speed);

	return 0;
}

static int msc313_spinor_set_mode(struct udevice *bus, uint mode)
{
	/* nothing to do */
	printf("set mode: %u\n", mode);

	if (mode & (SPI_TX_BYTE | SPI_TX_DUAL | SPI_TX_QUAD | SPI_TX_OCTAL)) {
		dev_err(bus, "Only full duplex SIO TX is supported, mode: %x\n", mode);
		return -EINVAL;
	}

	if (mode & (SPI_RX_SLOW | SPI_RX_DUAL | SPI_RX_QUAD | SPI_RX_OCTAL)) {
		dev_err(bus, "Only full duplex SIO RX is supported, mode: %x\n", mode);
		return -EINVAL;
	}

	return 0;
}

static int msc313_spinor_ofdata_to_platdata(struct udevice *bus)
{
	struct resource res_reg, res_mem;
	struct msc313_spinor_platdata *plat = dev_get_plat(bus);
	int ret;

	ret = dev_read_resource_byname(bus, "reg_base", &res_reg);
	if (ret) {
		debug("can't get reg_base resource(ret = %d)\n", ret);
		return -ENOMEM;
	}

	ret = dev_read_resource_byname(bus, "mem_base", &res_mem);
	if (ret) {
		debug("can't get map_base resource(ret = %d)\n", ret);
		return -ENOMEM;
	}

	plat->mem_base = res_mem.start;
	plat->reg_base = res_reg.start;

	return 0;
}

static const struct clk_div_table div_table[] = {
	{0x1, 2},
	{0x4, 4},
	{0x40, 8},
	{0x80, 16},
	{0x100, 32},
	{0x200, 64},
	{0x400, 128},
	{ 0 },
};

static int msc313_spinor_probe(struct udevice *bus)
{
	struct msc313_spinor_platdata *plat = dev_get_plat(bus);
	struct msc313_spinor_priv *priv = dev_get_priv(bus);

	priv->regs = (u32 *) plat->reg_base;
	priv->mem_base = (unsigned long *)plat->mem_base;

	/* initially force the clock down to 54MHz */
	writew(0x40, priv->regs + REG_SPI_CLKDIV);

	return 0;
}

static const struct dm_spi_ops msc313_spinor_ops = {
	.claim_bus      = msc313_spinor_claim_bus,
	.release_bus    = msc313_spinor_release_bus,
	.xfer           = msc313_spinor_xfer,
	.set_speed      = msc313_spinor_set_speed,
	.set_mode       = msc313_spinor_set_mode,
};

static const struct udevice_id msc313_spinor_ids[] = {
	{ .compatible = "mstar,msc313-spinor" },
	{ }
};

U_BOOT_DRIVER(msc313_spinor) = {
	.name			  = "msc313_spinor",
	.id			  = UCLASS_SPI,
	.of_match		  = msc313_spinor_ids,
	.ops			  = &msc313_spinor_ops,
	.of_to_plat		  = msc313_spinor_ofdata_to_platdata,
	.plat_auto		  = sizeof(struct msc313_spinor_platdata),
	.priv_auto		  = sizeof(struct msc313_spinor_priv),
	.probe			  = msc313_spinor_probe,
};
