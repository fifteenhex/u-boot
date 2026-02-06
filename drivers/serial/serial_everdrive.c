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

struct serial_everdrive_priv {
	unsigned int ver;
	void *base;
	u8 tmp[1];
};

static int serial_everdrive_getc(struct udevice *dev)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

//	iowrite32(GOLDFISH_TTY_CMD_READ_BUFFER,
//		  priv->base + GOLDFISH_TTY_REG_CMD);

//	return priv->tmp[0];
	return 0;
}

static int serial_everdrive_putc(struct udevice *dev, const unsigned char ch)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

//	iowrite32(ch, priv->base);

	void *out = (void*) CONFIG_DEBUG_UART_BASE;

	iowrite16(0x2b, out);
	iowrite16(0xd4, out);
	iowrite16(0x22, out);
	iowrite16(0xdd, out);

	/* len */
	iowrite16(0x00, out);
	iowrite16(0x01, out);

	/* Data */
	iowrite16(ch, out);

	return 0;
}

static int serial_everdrive_pending(struct udevice *dev, bool input)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);
//	u32 ready = ioread32(priv->base + GOLDFISH_TTY_REG_BYTES_READY);

//	return ready;
	return 0;
}

#if CONFIG_IS_ENABLED(DEBUG_EVERDRIVE_SERIAL)
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	void *out = (void*) CONFIG_DEBUG_UART_BASE;

	iowrite16(0x2b, out);
	iowrite16(0xd4, out);
	iowrite16(0x22, out);
	iowrite16(0xdd, out);

	/* len */
	iowrite16(0x00, out);
	iowrite16(0x01, out);

	/* Data */
	iowrite16(ch, out);
}

DEBUG_UART_FUNCS
#endif

static int serial_everdrive_probe(struct udevice *dev)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

	priv->base = (unsigned char*) dev_read_addr_ptr(dev);

	return 0;
}

static const struct dm_serial_ops serial_everdrive_ops = {
	.putc = serial_everdrive_putc,
	.getc = serial_everdrive_getc,
	.pending = serial_everdrive_pending,
};

static const struct udevice_id serial_everdrive_ids[] = {
	{ .compatible = "krikzz,everdrive-serial" },
	{ }
};

U_BOOT_DRIVER(serial_everdrive) = {
	.name	= "serial_everdrive",
	.id	= UCLASS_SERIAL,
	.of_match = serial_everdrive_ids,
	.priv_auto	= sizeof(struct serial_everdrive_priv),
	.probe = serial_everdrive_probe,
	.ops	= &serial_everdrive_ops,
};
