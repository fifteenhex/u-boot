// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */

#include <linux/litex.h>

#include <dm.h>
#include <dm/device_compat.h>
#include <net.h>

struct lance {
	struct udevice *dev;

	void __iomem *base;
};

static int lance_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct lance *priv = dev_get_priv(dev);
	u8 rx_slot;
	int len;


	return len;
}

static int lance_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	struct lance *priv = dev_get_priv(dev);

	return 0;
}

static int lance_start(struct udevice *dev)
{
	struct lance *priv = dev_get_priv(dev);



	return 0;
}

static void lance_stop(struct udevice *dev)
{
	struct lance *priv = dev_get_priv(dev);

}

static int lance_send(struct udevice *dev, void *packet, int len)
{
	struct lance *priv = dev_get_priv(dev);
	void __iomem *txbuffer;



	return 0;
}



static int lance_remove(struct udevice *dev)
{
	lance_stop(dev);

	return 0;
}

static const struct eth_ops lance_ops = {
	.start = lance_start,
	.stop = lance_stop,
	.send = lance_send,
	.recv = lance_recv,
	.free_pkt = lance_free_pkt,
};

static int lance_of_to_plat(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct lance *priv = dev_get_priv(dev);
	void __iomem *buf_base;

	pdata->iobase = dev_read_addr(dev);

	priv->dev = dev;

	priv->base = dev_read_addr(dev);

	return 0;
}

static const struct udevice_id lance_ids[] = {
	{ .compatible = "amd,am7990" },
	{}
};

U_BOOT_DRIVER(lance) = {
	.name = "lance",
	.id = UCLASS_ETH,
	.of_match = lance_ids,
	.of_to_plat = lance_of_to_plat,
	.plat_auto = sizeof(struct eth_pdata),
	.remove = lance_remove,
	.ops = &lance_ops,
	.priv_auto = sizeof(struct lance),
};
