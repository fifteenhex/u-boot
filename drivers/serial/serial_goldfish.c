// SPDX-License-Identifier: GPL-2.0+
/*
 * See: https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 */

#include <debug_uart.h>
#include <dm.h>
#include <efi.h>
#include <efi_api.h>
#include <errno.h>
#include <fdtdec.h>
#include <log.h>
#include <linux/compiler.h>
#include <asm/io.h>
#include <serial.h>
#include <linux/io.h>

/* Goldfish tty register's offsets */
#define	GOLDFISH_TTY_REG_BYTES_READY	0x04
#define	GOLDFISH_TTY_REG_CMD		0x08
#define	GOLDFISH_TTY_REG_DATA_PTR	0x10
#define	GOLDFISH_TTY_REG_DATA_LEN	0x14
#define	GOLDFISH_TTY_REG_DATA_PTR_HIGH	0x18
#define	GOLDFISH_TTY_REG_VERSION	0x20

/* Goldfish tty commands */
#define	GOLDFISH_TTY_CMD_INT_DISABLE	0
#define	GOLDFISH_TTY_CMD_INT_ENABLE	1
#define	GOLDFISH_TTY_CMD_WRITE_BUFFER	2
#define	GOLDFISH_TTY_CMD_READ_BUFFER	3

struct serial_goldfish_priv {
	unsigned int ver;
	void *base;
	u8 tmp[1];
};

static int serial_goldfish_getc(struct udevice *dev)
{
	struct serial_goldfish_priv *priv = dev_get_priv(dev);

	iowrite32(GOLDFISH_TTY_CMD_READ_BUFFER,
		  priv->base + GOLDFISH_TTY_REG_CMD);

	return priv->tmp[0];
}

static int serial_goldfish_putc(struct udevice *dev, const unsigned char ch)
{
	struct serial_goldfish_priv *priv = dev_get_priv(dev);

	iowrite32(ch, priv->base);

	return 0;
}

static int serial_goldfish_pending(struct udevice *dev, bool input)
{
	struct serial_goldfish_priv *priv = dev_get_priv(dev);
	u32 ready = ioread32(priv->base + GOLDFISH_TTY_REG_BYTES_READY);

	return ready;
}

#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	void *out = (void*) CONFIG_DEBUG_UART_BASE;

	iowrite32(ch, out);
}

DEBUG_UART_FUNCS

static int serial_goldfish_probe(struct udevice *dev)
{
	struct serial_goldfish_priv *priv = dev_get_priv(dev);

	priv->base = (unsigned char*) dev_read_addr_ptr(dev);
	priv->ver = ioread32(priv->base + GOLDFISH_TTY_REG_VERSION);

	iowrite32(GOLDFISH_TTY_CMD_INT_DISABLE,
		  priv->base + GOLDFISH_TTY_REG_CMD);
	iowrite32((u32) &priv->tmp[0], priv->base + GOLDFISH_TTY_REG_DATA_PTR);
	iowrite32(sizeof(priv->tmp), priv->base + GOLDFISH_TTY_REG_DATA_LEN);

	return 0;
}

static const struct dm_serial_ops serial_goldfish_ops = {
	.putc = serial_goldfish_putc,
	.getc = serial_goldfish_getc,
	.pending = serial_goldfish_pending,
};

static const struct udevice_id serial_goldfish_ids[] = {
	{ .compatible = "google,goldfish-tty" },
	{ }
};

U_BOOT_DRIVER(serial_goldfish) = {
	.name	= "serial_goldfish",
	.id	= UCLASS_SERIAL,
	.of_match = serial_goldfish_ids,
	.priv_auto	= sizeof(struct serial_goldfish_priv),
	.probe = serial_goldfish_probe,
	.ops	= &serial_goldfish_ops,
};
