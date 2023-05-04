// SPDX-License-Identifier: GPL-2.0+
/*
 * Based on ux500.c:
 * Copyright (C) 2019 Stephan Gerhold
 */

#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <dm/device_compat.h>
#include "musb_uboot.h"

static struct musb_hdrc_config mstar_musb_hdrc_config = {
	.multipoint	= 0,
	.num_eps	= 4,
};

struct mstar_glue {
	struct musb_host_data mdata;
	struct device dev;
	//struct phy phy;
	bool enabled;
};
#define to_mstar_glue(d)	container_of(d, struct mstar_glue, dev)

void musb_write_fifo(struct musb_hw_ep *hw_ep, u16 len,
				    const u8 *src)
{
	struct musb *musb = hw_ep->musb;
	void __iomem *fifo = hw_ep->fifo;

	if (unlikely(len == 0))
		return;

	dev_dbg(musb->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'T', hw_ep->epnum, fifo, len, src);

	for (int i = 0; i < len; i++)
		writeb(src[i], fifo);
}

void musb_read_fifo(struct musb_hw_ep *hw_ep, u16 len, u8 *dst)
{
	struct musb *musb = hw_ep->musb;
	void __iomem *fifo = hw_ep->fifo;

	if (unlikely(len == 0))
		return;

	dev_dbg(musb->controller, "%cX ep%d fifo %p count %d buf %p\n",
			'R', hw_ep->epnum, fifo, len, dst);

	for (int i = 0; i < len; i++)
		dst[i] = readb(fifo);
}

static int mstar_musb_enable(struct musb *musb)
{
	struct mstar_glue *glue = to_mstar_glue(musb->controller);
	int ret;

	if (glue->enabled)
		return 0;

	//ret = generic_phy_power_on(&glue->phy);
	//if (ret) {
	//	printf("%s: failed to power on USB PHY\n", __func__);
	//	return ret;
	//}

	glue->enabled = true;
	return 0;
}

static void mstar_musb_disable(struct musb *musb)
{
	struct mstar_glue *glue = to_mstar_glue(musb->controller);
	int ret;

	if (!glue->enabled)
		return;

	//ret = generic_phy_power_off(&glue->phy);
	//if (ret) {
	//	printf("%s: failed to power off USB PHY\n", __func__);
	//	return;
	//}

	glue->enabled = false;
}

static irqreturn_t mstar_musb_interrupt(int irq, void *__hci)
{
	unsigned long	flags;
	irqreturn_t	retval = IRQ_NONE;
	struct musb	*musb = __hci;

	spin_lock_irqsave(&musb->lock, flags);

	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX);

	/*
	 * The controller is gadget only, the state of the host mode IRQ bits is
	 * undefined. Mask them to make sure that the musb driver core will
	 * never see them set
	 */
	musb->int_usb &= MUSB_INTR_SUSPEND | MUSB_INTR_RESUME |
			 MUSB_INTR_RESET | MUSB_INTR_SOF;

	if (musb->int_usb || musb->int_tx || musb->int_rx)
		retval = musb_interrupt(musb);

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}

static int mstar_musb_init(struct musb *musb)
{
	struct mstar_glue *glue = to_mstar_glue(musb->controller);
	int ret;
	u8 power;

	printf("%s:%d\n", __func__, __LINE__);

	/* Reset the musb */
	power = musb_readb(musb->mregs, MUSB_POWER);
	power = power | MUSB_POWER_RESET;
	musb_writeb(musb->mregs, MUSB_POWER, power);
	mdelay(100);

	musb->isr = mstar_musb_interrupt;

	//ret = generic_phy_init(&glue->phy);
	//if (ret) {
	//	printf("%s: failed to init USB PHY\n", __func__);
	//	return ret;
	//}

	return 0;
}

static int mstar_musb_exit(struct musb *musb)
{
	struct mstar_glue *glue = to_mstar_glue(musb->controller);
	int ret;

	//ret = generic_phy_exit(&glue->phy);
	//if (ret) {
	//	printf("%s: failed to exit USB PHY\n", __func__);
	//	return ret;
	//}

	return 0;
}

static const struct musb_platform_ops mstar_musb_ops = {
	.init		= mstar_musb_init,
	.exit		= mstar_musb_exit,
	.enable		= mstar_musb_enable,
	.disable	= mstar_musb_disable,
};

int dm_usb_gadget_handle_interrupts(struct udevice *dev)
{
	struct mstar_glue *glue = dev_get_priv(dev);

	glue->mdata.host->isr(0, glue->mdata.host);

	return 0;
}

static int mstar_musb_probe(struct udevice *dev)
{
	struct mstar_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;
	struct musb_hdrc_platform_data pdata;
	void *base = dev_read_addr_ptr(dev);
	int ret;

	if (!base)
		return -EINVAL;

	//ret = generic_phy_get_by_name(dev, "usb", &glue->phy);
	//if (ret) {
	//	dev_err(dev, "failed to get USB PHY: %d\n", ret);
	//	return ret;
	//}

	memset(&pdata, 0, sizeof(pdata));
	pdata.platform_ops = &mstar_musb_ops;
	pdata.config = &mstar_musb_hdrc_config;
	pdata.mode = MUSB_PERIPHERAL;
	host->host = musb_init_controller(&pdata, &glue->dev, base);
	if (!host->host)
		return -EIO;

	return usb_add_gadget_udc(&glue->dev, &host->host->g);
}

static int mstar_musb_remove(struct udevice *dev)
{
	struct mstar_glue *glue = dev_get_priv(dev);
	struct musb_host_data *host = &glue->mdata;

	usb_del_gadget_udc(&host->host->g);
	musb_stop(host->host);
	free(host->host);
	host->host = NULL;

	return 0;
}

static const struct udevice_id mstar_musb_ids[] = {
	{ .compatible = "mstar,msc313-musb" },
	{ }
};

U_BOOT_DRIVER(mstar_musb) = {
	.name		= "mstar-musb",
	.id		= UCLASS_USB_GADGET_GENERIC,
	.of_match	= mstar_musb_ids,
	.probe		= mstar_musb_probe,
	.remove		= mstar_musb_remove,
	.plat_auto	= sizeof(struct usb_plat),
	.priv_auto	= sizeof(struct mstar_glue),
};
