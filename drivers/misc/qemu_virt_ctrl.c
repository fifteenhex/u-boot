// SPDX-License-Identifier: GPL-2.0+
/*
 * See: https://github.com/qemu/qemu/blob/master/docs/specs/virt-ctlr.rst
 */

#include <dm.h>
#include <asm/io.h>

#define REG_FEATURE 0x0
#define REG_COMMAND 0x4

#define COMMAND_RESET	1
#define COMMAND_HALT	2
#define COMMAND_PANIC	3

struct virt_ctrl_priv {
	void *base;
};

void qemu_virt_ctrl_halt(struct udevice *dev)
{
	struct virt_ctrl_priv *priv = dev_get_priv(dev);

	writel(COMMAND_HALT, priv->base + REG_COMMAND);
}

static int qemu_virt_ctrl_probe(struct udevice *dev)
{
	struct virt_ctrl_priv *priv = dev_get_priv(dev);

	priv->base = (unsigned char*) dev_read_addr_ptr(dev);

	return 0;
}

static const struct udevice_id qemu_virt_ctrl_ids[] = {
	{ .compatible = "qemu,virt-ctrl" },
	{ }
};

U_BOOT_DRIVER(google_qemu_virt_ctrl_lpc) = {
	.name		= "qemu_virt_ctrl",
	.id		= UCLASS_MISC,
	.of_match	= qemu_virt_ctrl_ids,
	.probe		= qemu_virt_ctrl_probe,
	.priv_auto	= sizeof(struct virt_ctrl_priv),
};
