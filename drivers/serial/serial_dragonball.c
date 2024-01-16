// SPDX-License-Identifier: GPL-2.0+
/*
 */

//#define DEBUG 1

#include <clk.h>
#include <debug_uart.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <asm/io.h>
#include <serial.h>
#include <linux/delay.h>

/* Dragonball serial register's offsets */
#define	REG_USTCNT			0x0
#define REG_USTCNT_87			BIT(8)
#define REG_TXEN			BIT(13)
#define REG_RXEN			BIT(14)
#define REG_USTCNT_UEN			BIT(15)
#define REG_UBAUD			0x2
#define REG_UBAUD_SRC			BIT(11)
#define REG_UBAUD_DIVIDE_MASK	0x3
#define REG_UBAUD_DIVIDE_SHIFT	8
#define REG_UBAUD_PRESCALER_MASK 0x3f
#define REG_URX				0x4
#define REG_URX_PARITYERROR		BIT(8)
#define REG_URX_FRAMEERROR		BIT(10)
#define REG_URX_OVERRUN			BIT(11)
#define REG_URX_DATAREADY		BIT(13)
#define REG_URX_FIFOHALF		BIT(14)
#define REG_UTX				0x6
#define REG_UTX_TXAVAIL			BIT(13)
#define REG_UTX_BUSY			BIT(10)
#define REG_UMISC			0x8
#define REG_NIPR			0xa

struct serial_dragonball_priv {
	void *base;
	struct clk sysclk;

	bool waiting, error, fifohalf;
	u8 tmp;

	unsigned long sysclk_rate;
	int baudrate;
};

struct serial_dragonball_cheat_table_entry {
	unsigned long sysclk_rate;
	int baud_rate;
	u8 divisor;
	u8 prescaler;
};

static const struct serial_dragonball_cheat_table_entry cheat_table[] =
{
	{
		.sysclk_rate = 8290304,
		.baud_rate = 9600,
		.divisor = 0,
		.prescaler = 54,
	},
	{
		.sysclk_rate = 25198592,
		.baud_rate = 9600,
		.divisor = 2,
		.prescaler = 41,
	},
};

static int serial_dragonball_setbrg(struct udevice *dev, int baudrate) __attribute__ ((optimize(2)));
static int serial_dragonball_setbrg(struct udevice *dev, int baudrate)
{
	struct serial_dragonball_priv *priv = dev_get_priv(dev);
	u16 ustcnt = readw(priv->base + REG_UBAUD);
	u32 target = baudrate * 16;
	unsigned long sysclk_rate = clk_get_rate(&priv->sysclk);
	unsigned int divpow, prescaler;
	u16 baud = 0;
	unsigned int i;

	/* This takes a while don't do it again just for fun.. */
	if ((priv->baudrate == baudrate) &&
			(priv->sysclk_rate == sysclk_rate))
		return 0;

	/* Current best match */
	int bestdivpow, bestprescaler;
	u32 bestdiff = ~0;

	for (i = 0; i < ARRAY_SIZE(cheat_table); i++) {
		const struct serial_dragonball_cheat_table_entry *entry = &cheat_table[i];

		if ((entry->sysclk_rate == sysclk_rate) &&
			(entry->baud_rate == baudrate)) {
				debug("Found cheat entry\n");
				bestdivpow = entry->divisor;
				bestprescaler = entry->prescaler;
				goto configure;
		}
	}

	for (divpow = 0; divpow < 8; divpow++) {
		u32 divisor = 1 << divpow;
		u32 lastdiff = ~0;
		for (prescaler = 2; prescaler < 66; prescaler++) {
			u32 freq = (sysclk_rate / prescaler) / divisor;
			u32 diff = max(freq, target) - min(freq, target);
			/*
			 * Is this prescaler and divider combo better than the current
			 * best?
			 */
			if (diff < bestdiff) {
				bestdivpow = divpow;
				bestprescaler = prescaler;
				bestdiff = diff;
				debug("%u(%u) %d %lu %u %u %u\n",
					   divisor, divpow, prescaler, sysclk_rate, target, freq, diff);
			}
			/*
			 * If the difference is now getting bigger again we have passed
			 * the sweet spot and there is no point continuing.
			 */
			else if (diff > lastdiff)
				break;

			lastdiff = diff;
		}
	}

configure:
	debug("%u %u\n", bestdivpow, bestprescaler);

	/* And here we goooo... */
	baud |= bestdivpow << REG_UBAUD_DIVIDE_SHIFT;
	baud |= 65 - bestprescaler;
	writew(baud,priv->base + REG_UBAUD);

	/*
	 * Cache the baud rate and the base clock used
	 * so we can skip calculating again.
	 */
	priv->sysclk_rate = sysclk_rate;
	priv->baudrate = baudrate;

	return 0;
}

static inline void serial_dragonball_reset(struct serial_dragonball_priv *priv)
{
	u16 ustcnt = readw(priv->base + REG_USTCNT);

	priv->waiting = false;
	priv->error = false;

	/* disable and then renable the rx and tx to reset the fifos */
	writew(ustcnt & ~(REG_RXEN | REG_TXEN), priv->base + REG_USTCNT);
	mdelay(1);
	writew(ustcnt | (REG_RXEN | REG_TXEN), priv->base + REG_USTCNT);
}

static int serial_dragonball_putc(struct udevice *dev, const unsigned char ch)
{
	struct serial_dragonball_priv *priv = dev_get_priv(dev);
	u16 utx;

	utx = readw(priv->base + REG_UTX);
	if (!(utx & REG_UTX_TXAVAIL))
		return -EAGAIN;

	writeb(ch, priv->base + (REG_UTX + 1));

	return 0;
}

/*
 * Checking if there is a character also reads it,
 * so pending will cause the char to get read so
 * instead make sure we only read once in either
 * pending or getc and then don't read again until
 * the char is consumed.
 */
static inline bool serial_dragonball_tryc(struct serial_dragonball_priv *priv)
{
	u16 urx;

	/* Already have a char */
	if (priv->waiting)
		return true;

	/* Don't have a char, try to get one */
	urx = readw(priv->base + REG_URX);
	if (urx & REG_URX_DATAREADY) {
		priv->fifohalf = (urx & REG_URX_FIFOHALF) ? 1 : 0;
		if (urx & (REG_URX_PARITYERROR | REG_URX_FRAMEERROR | REG_URX_OVERRUN))
			priv->error = true;

		priv->waiting = true;
		priv->tmp = urx & 0xff;

		return true;
	}

	/* Nope */
	return false;
}

static int serial_dragonball_getc(struct udevice *dev)
{
	struct serial_dragonball_priv *priv = dev_get_priv(dev);

	if (serial_dragonball_tryc(priv)) {

		if (priv->error) {
			serial_dragonball_reset(priv);

			return -EIO;
		}

		priv->waiting = false;
		return priv->tmp;
	}

	return -EAGAIN;
}

static int serial_dragonball_pending(struct udevice *dev, bool input)
{
	struct serial_dragonball_priv *priv = dev_get_priv(dev);

	if (input)
		return (priv->fifohalf | serial_dragonball_tryc(priv)) ? 1 : 0;
	else {
		u16 utx = readw(priv->base + REG_UTX);
		return (utx & REG_UTX_TXAVAIL) ? 0 : 1;
	}
}

#ifdef CONFIG_DEBUG_DRAGONBALL_SERIAL
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
	void *base = (void*) CONFIG_DEBUG_UART_BASE;
	u16 ucnt;

	/* Enable the UART, select 8 bit mode */
	ucnt = REG_USTCNT_87 | REG_TXEN | REG_RXEN | REG_USTCNT_UEN;
	writew(ucnt, base + REG_USTCNT);

	/* Manual says to read urx to init the fifo */
	readw(base + REG_URX);
}

static inline void _debug_uart_putc(int ch)
{
	void *out = (void*) CONFIG_DEBUG_UART_BASE;
	u16 utx;

	do {
		utx = readw(out + REG_UTX);
	} while (!(utx & REG_UTX_TXAVAIL));

	writeb(ch, out + (REG_UTX + 1));
}

DEBUG_UART_FUNCS
#endif

static int serial_dragonball_probe(struct udevice *dev)
{
	struct serial_dragonball_priv *priv = dev_get_priv(dev);
	u16 ustcnt;
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	priv->waiting = false;
	priv->error = false;
	priv->baudrate = -1;
	priv->sysclk_rate = ~0;

	ret = clk_get_by_name(dev, "sysclk", &priv->sysclk);
	//if (ret)
	//	return ret;

	/* Enable the UART, select 8 bit mode */
	ustcnt = REG_USTCNT_87 | REG_USTCNT_UEN;
	writew(ustcnt, priv->base + REG_USTCNT);

	serial_dragonball_reset(priv);

	/* Manual says to read urx to init the fifo after hard reset */
	readw(priv->base + REG_URX);

	return 0;
}

static const struct dm_serial_ops serial_dragonball_ops = {
	.setbrg = serial_dragonball_setbrg,
	.putc = serial_dragonball_putc,
	.getc = serial_dragonball_getc,
	.pending = serial_dragonball_pending,
};

static const struct udevice_id serial_dragonball_ids[] = {
	{ .compatible = "motorola,mc68ez328-uart" },
	{ }
};

U_BOOT_DRIVER(serial_dragonball) = {
	.name	= "serial_dragonball",
	.id	= UCLASS_SERIAL,
	.of_match = serial_dragonball_ids,
	.priv_auto	= sizeof(struct serial_dragonball_priv),
	.probe = serial_dragonball_probe,
	.flags = DM_FLAG_PRE_RELOC,
	.ops	= &serial_dragonball_ops,
};
