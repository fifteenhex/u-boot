// SPDX-License-Identifier: GPL-2.0+

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

#include <linux/delay.h>

struct __attribute__((packed)) everdrive_cmd {
	uint8_t preamble;
	uint8_t _preable;
	uint8_t cmd;
	uint8_t _cmd;
};

#define CMD_PREAMBLE	'+'
#define CMD_STATUS	0x10
#define CMD_USB_WRITE	0x22
#define CMD_FIFO_WRITE	0x23
#define DEFINECMD(_cmd) { CMD_PREAMBLE, ~CMD_PREAMBLE, _cmd, ~_cmd };
static const struct everdrive_cmd cmd_status = DEFINECMD(CMD_STATUS);
static const struct everdrive_cmd cmd_usbwr = DEFINECMD(CMD_USB_WRITE);

#define FIFO_CPU_RXF BIT(15)
#define FIFO_RXF_MSK 0x7FF

static inline void fifo_write(void *fifo, const u8 *src, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++)
		writew(src[i], fifo);
}

static inline void fifo_read(void *fifo, u8 *dst, unsigned int len)
{
	int i;

	for (i = 0; i < len; i++) {
		while (!(readw(fifo + 2) & FIFO_RXF_MSK)) {
			/* spin until there is something to read */
		}
		dst[i] = readw(fifo);
	}
}

static inline void read_status(void *fifo)
{
	uint16_t status;

	fifo_write(fifo, (u8*) &cmd_status, sizeof(cmd_status));
	fifo_read(fifo, (u8*) &status, sizeof(status));

	printf("status; 0x%04x\n", (unsigned) status);
}

struct serial_everdrive_priv {
	void __iomem *base;
};

static int serial_everdrive_getc(struct udevice *dev)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);
	uint8_t ch;
	uint16_t status;

	fifo_read(priv->base, &ch, 1);

	return ch;
}

static void _serial_everdrive_putc(void *fifo, const unsigned char ch)
{
	uint16_t len = 1;

	fifo_write(fifo, (u8*) &cmd_usbwr, sizeof(cmd_usbwr));
	fifo_write(fifo, (u8*) &len, sizeof(len));
	fifo_write(fifo, (u8*) &ch, sizeof(ch));
}

static int serial_everdrive_putc(struct udevice *dev, const unsigned char ch)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

	_serial_everdrive_putc(priv->base, ch);

	return 0;
}

static int serial_everdrive_pending(struct udevice *dev, bool input)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

	if(readw(priv->base + 2) & FIFO_RXF_MSK)
		return 1;

	return 0;
}

#if CONFIG_IS_ENABLED(DEBUG_EVERDRIVE_SERIAL)
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	void *fifo = (void*) CONFIG_DEBUG_UART_BASE;

	_serial_everdrive_putc(fifo, ch);
}

DEBUG_UART_FUNCS
#endif

static void fifo_gobble(void *fifo)
{
	while (readw(fifo + 2) & FIFO_RXF_MSK)
		readw(fifo);
}

static int serial_everdrive_probe(struct udevice *dev)
{
	struct serial_everdrive_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	fifo_gobble(priv->base);

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
