// SPDX-License-Identifier: GPL-2.0+
/*
 */

#define DEBUG 1

#include <clk.h>
#include <debug_uart.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <asm/io.h>
#include <serial.h>
#include <linux/delay.h>

struct serial_scc_priv {
	void *base;
};

static int serial_scc_setbrg(struct udevice *dev, int baudrate)
{
	return 0;
}

static inline void serial_scc_reset(struct serial_scc_priv *priv)
{
}

static int serial_scc_putc(struct udevice *dev, const unsigned char ch)
{
	struct serial_scc_priv *priv = dev_get_priv(dev);
	u8 status;

	do {
		writeb(0, priv->base);
		status = readb(priv->base);
	} while (!(status & 0x4));

	writeb(ch, priv->base + 1);

	return 0;
}

static inline bool serial_scc_tryc(struct serial_scc_priv *priv)
{
	return 0;
}

static int serial_scc_getc(struct udevice *dev)
{
	return -EAGAIN;
}

static int serial_scc_pending(struct udevice *dev, bool input)
{
	return 0;
}

#ifdef CONFIG_DEBUG_SCC_SERIAL
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	void *base = (void*) CONFIG_DEBUG_UART_BASE;
	u8 status;

	do {
		writeb(0, base);
		status = readb(base);
	} while (!(status & 0x4));

	writeb(ch, base + 1);
}

DEBUG_UART_FUNCS
#endif

static int serial_scc_probe(struct udevice *dev)
{
	struct serial_scc_priv *priv = dev_get_priv(dev);

	debug("probe!\n");

        priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static const struct dm_serial_ops serial_scc_ops = {
	.setbrg = serial_scc_setbrg,
	.putc = serial_scc_putc,
	.getc = serial_scc_getc,
	.pending = serial_scc_pending,
};

static const struct udevice_id serial_scc_ids[] = {
	{ .compatible = "zilog,scc" },
	{ }
};

U_BOOT_DRIVER(serial_scc) = {
	.name      = "serial_scc",
	.id        = UCLASS_SERIAL,
	.of_match  = serial_scc_ids,
	.priv_auto = sizeof(struct serial_scc_priv),
	.probe     = serial_scc_probe,
	.flags     = DM_FLAG_PRE_RELOC,
	.ops       = &serial_scc_ops,
};
