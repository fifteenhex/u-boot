// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Zilog 8530 (SCC/ESCC) serial driver for classic 68k Macintosh.
 *
 * Based on the Zilog SCC driver from the fifteenhex m68k tree (MVME147),
 * adapted for the Macintosh register spacing where, for a given channel, the
 * data register sits 4 bytes above the control register (e.g. Quadra 800:
 * channel A control 0x5000c022, data 0x5000c026).  Platform data carries the
 * two absolute register addresses so the board can wire it without a DT.
 */

#include <dm.h>
#include <errno.h>
#include <serial.h>
#include <serial_scc.h>
#include <asm/io.h>

/* Read register 0 (status) bits */
#define SCC_RR0			0
#define SCC_RR0_RX_AVAIL	BIT(0)
#define SCC_RR0_TX_EMPTY	BIT(2)
/* Write register 3 (receive control) */
#define SCC_WR3			3
#define SCC_WR3_RX_8BITS	0xc0
#define SCC_WR3_RX_ENABLE	0x01
/* Write register 4 (misc tx/rx parameters) */
#define SCC_WR4			4
#define SCC_WR4_X16_1STOP	0x44	/* x16 clock, 1 stop bit, no parity */
/* Write register 5 (transmit control) */
#define SCC_WR5			5
#define SCC_WR5_TX_8BITS	0x60
#define SCC_WR5_TX_ENABLE	0x08
#define SCC_WR5_DTR		0x80
#define SCC_WR5_RTS		0x02
/* Write register 9 (master interrupt / reset) */
#define SCC_WR9			9
#define SCC_WR9_RESET_A		0x80	/* channel A reset */
/* Write register 11 (clock mode) */
#define SCC_WR11		11
#define SCC_WR11_BRG		0x50	/* Rx and Tx clock from the BRG */
/* Write registers 12/13 (baud-rate generator time constant) */
#define SCC_WR12		12
#define SCC_WR13		13
/* Write register 14 (BRG control) */
#define SCC_WR14		14
#define SCC_WR14_BRG_EN		0x01	/* baud-rate generator enable */
#define SCC_WR14_BRG_PCLK	0x02	/* BRG source = PCLK (else the RTxC pin) */

/*
 * Classic-Mac SCC baud clock.  On real 68k Macs the 3.6864 MHz baud-rate clock
 * is wired to the SCC's RTxC pin (not PCLK), so we source the baud-rate
 * generator from RTxC (WR14 without SCC_WR14_BRG_PCLK).  At the x16 sample rate
 * the time constant is (clk / (32 * baud)) - 2, e.g. 10 for 9600.  (QEMU wires
 * PCLK and RTxC to the same clock, so it works either way there - but real
 * hardware only clocks RTxC, which is why sourcing PCLK gives ~1/8 the rate.)
 */
#define SCC_RTXC		3686400
#define SCC_BRG_TC(baud)	((SCC_RTXC) / (32 * (baud)) - 2)

struct scc_serial_priv {
	void *ctrl;
	void *data;
};

/* Write register 'reg' via the control port.  Reading it first resets the
 * register pointer to 0; then we select the register and write the value. */
static void scc_wr(void *ctrl, u8 reg, u8 val)
{
	readb(ctrl);
	writeb(reg, ctrl);
	writeb(val, ctrl);
}

/*
 * Fully bring up an SCC channel for polled 8N1 async at the given BRG time
 * constant.  On QEMU the ESCC transmits with almost no setup, but a real Z8530
 * has no configured clock until the BRG is programmed, so we must reset the
 * channel and set the clock mode, clock source and time constant ourselves -
 * the ROM does not leave it in a state we can rely on.
 */
static void scc_channel_init(void *ctrl, unsigned int tc)
{
	int i;

	readb(ctrl);
	writeb(SCC_WR9, ctrl);
	writeb(SCC_WR9_RESET_A, ctrl);	/* channel reset */
	for (i = 0; i < 64; i++)	/* let the reset settle (>= 4 PCLKs) */
		readb(ctrl);

	scc_wr(ctrl, SCC_WR4, SCC_WR4_X16_1STOP);
	scc_wr(ctrl, SCC_WR3, SCC_WR3_RX_8BITS);
	scc_wr(ctrl, SCC_WR5, SCC_WR5_TX_8BITS | SCC_WR5_DTR | SCC_WR5_RTS);
	scc_wr(ctrl, SCC_WR11, SCC_WR11_BRG);
	scc_wr(ctrl, SCC_WR12, tc & 0xff);
	scc_wr(ctrl, SCC_WR13, (tc >> 8) & 0xff);
	scc_wr(ctrl, SCC_WR14, SCC_WR14_BRG_EN);	/* BRG source = RTxC */
	scc_wr(ctrl, SCC_WR3, SCC_WR3_RX_8BITS | SCC_WR3_RX_ENABLE);
	scc_wr(ctrl, SCC_WR5,
	       SCC_WR5_TX_8BITS | SCC_WR5_TX_ENABLE | SCC_WR5_DTR | SCC_WR5_RTS);
}

/* Reading the control port returns the pointed-at read register and resets the
 * register pointer back to 0, so read_reg() is self-contained. */
static u8 scc_read_reg(struct scc_serial_priv *priv, u8 which)
{
	if (which)
		writeb(which, priv->ctrl);
	return readb(priv->ctrl);
}

static int scc_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct scc_serial_priv *priv = dev_get_priv(dev);

	if (baudrate <= 0)
		return -EINVAL;

	scc_channel_init(priv->ctrl, SCC_BRG_TC(baudrate));
	return 0;
}

static int scc_serial_putc(struct udevice *dev, const char ch)
{
	struct scc_serial_priv *priv = dev_get_priv(dev);

	if (!(scc_read_reg(priv, SCC_RR0) & SCC_RR0_TX_EMPTY))
		return -EAGAIN;

	writeb(ch, priv->data);
	return 0;
}

static int scc_serial_getc(struct udevice *dev)
{
	struct scc_serial_priv *priv = dev_get_priv(dev);

	if (!(scc_read_reg(priv, SCC_RR0) & SCC_RR0_RX_AVAIL))
		return -EAGAIN;

	return readb(priv->data);
}

static int scc_serial_pending(struct udevice *dev, bool input)
{
	struct scc_serial_priv *priv = dev_get_priv(dev);
	u8 rr0 = scc_read_reg(priv, SCC_RR0);

	if (input)
		return (rr0 & SCC_RR0_RX_AVAIL) ? 1 : 0;

	return (rr0 & SCC_RR0_TX_EMPTY) ? 0 : 1;
}

static int scc_serial_probe(struct udevice *dev)
{
	struct scc_serial_plat *plat = dev_get_plat(dev);
	struct scc_serial_priv *priv = dev_get_priv(dev);

	priv->ctrl = (void *)plat->ctrl;
	priv->data = (void *)plat->data;

	/* Bring the channel up at a sane default; the serial core calls
	 * setbrg() afterwards with the configured baud rate. */
	scc_channel_init(priv->ctrl, SCC_BRG_TC(9600));

	return 0;
}

static const struct dm_serial_ops scc_serial_ops = {
	.setbrg  = scc_serial_setbrg,
	.putc    = scc_serial_putc,
	.getc    = scc_serial_getc,
	.pending = scc_serial_pending,
};

U_BOOT_DRIVER(serial_scc) = {
	.name      = "serial_scc",
	.id        = UCLASS_SERIAL,
	.plat_auto = sizeof(struct scc_serial_plat),
	.priv_auto = sizeof(struct scc_serial_priv),
	.probe     = scc_serial_probe,
	.ops       = &scc_serial_ops,
	.flags     = DM_FLAG_PRE_RELOC,
};

#ifdef CONFIG_DEBUG_UART_SCC
#include <debug_uart.h>

static inline void _debug_uart_init(void)
{
	scc_channel_init((void *)CONFIG_VAL(DEBUG_UART_BASE), SCC_BRG_TC(9600));
}

static inline void _debug_uart_putc(int ch)
{
	void *ctrl = (void *)CONFIG_VAL(DEBUG_UART_BASE);
	void *data = ctrl + 4;

	while (1) {
		writeb(0, ctrl);
		if (readb(ctrl) & SCC_RR0_TX_EMPTY)
			break;
	}
	writeb(ch, data);
}

DEBUG_UART_FUNCS
#endif
