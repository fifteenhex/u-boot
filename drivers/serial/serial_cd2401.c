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
#include <time.h>

#include <cd2401.h>
#include <vic068a.h>

#include <asm/e17/e17.h>

/*
 * Make TX interrupts higher priority that
 * RX so an RX can never interrupt a TX
 */
#define CD2401_TX_IPL 0x2
#define CD2401_RX_IPL 0x1

static volatile u8 *wtf = (void *) 0x400000;

struct serial_cd2401_priv {
	void *base;
	u8 rx_fifo[CD2401_FIFO_SZ * 2];
	u8 rx_head;
	u8 rx_tail;
};

static inline void cd2401_irq_delay(void)
{
	do { __asm__ __volatile__ (" nop; nop; nop; nop;"); } while (0);
}

#define serial_cd2401_read_reg(_priv, _which) readb(_priv->base + _which)
#define serial_cd2401_write_reg(_priv, _which, _val) writeb(_val, _priv->base + _which)

static inline bool serial_cd2401_local_rx_fifo_empty(struct serial_cd2401_priv *priv)
{
	return priv->rx_head == priv->rx_tail;
}

static inline bool serial_cd2401_local_rx_fifo_full(struct serial_cd2401_priv *priv)
{
	return ((priv->rx_head + 1) % ARRAY_SIZE(priv->rx_fifo)) == priv->rx_tail;
}

static void serial_cd2401_local_rx_fifo_push(struct serial_cd2401_priv *priv, u8 val)
{
	if (serial_cd2401_local_rx_fifo_full(priv))
		return;

	priv->rx_fifo[priv->rx_head] = val;
	priv->rx_head = (priv->rx_head + 1) % ARRAY_SIZE(priv->rx_fifo);
}

static u8 serial_cd2401_local_rx_fifo_pop(struct serial_cd2401_priv *priv)
{
	u8 val = priv->rx_fifo[priv->rx_tail];
	priv->rx_tail = (priv->rx_tail + 1) % ARRAY_SIZE(priv->rx_fifo);
	return val;
}

static inline void serial_cd2401_select_chan(struct serial_cd2401_priv *priv, u8 which)
{
	serial_cd2401_write_reg(priv, CD2401_CAR, which);
}

static int serial_cd2401_setbrg(struct udevice *dev, int baudrate)
{
	return 0;
}

static inline void serial_cd2401_enable_tx_irqs(const struct serial_cd2401_priv *priv)
{
	u8 val;

	val = serial_cd2401_read_reg(priv, CD2401_IER);
	val |= CD2401_IER_TXD;
	serial_cd2401_write_reg(priv, CD2401_IER, val);
}

static inline void serial_cd2401_disable_tx_irqs(const struct serial_cd2401_priv *priv)
{
	u8 val;

	val = serial_cd2401_read_reg(priv, CD2401_IER);
	val &= ~CD2401_IER_TXD;
	serial_cd2401_write_reg(priv, CD2401_IER, val);
}

static inline void serial_cd2401_enable_rx_irqs(const struct serial_cd2401_priv *priv)
{
	u8 val;

	val = serial_cd2401_read_reg(priv, CD2401_IER);
	val |= CD2401_IER_RXD;
	serial_cd2401_write_reg(priv, CD2401_IER, val);
}

static inline void serial_cd2401_disable_irqs(struct serial_cd2401_priv *priv)
{
	serial_cd2401_write_reg(priv, CD2401_IER, 0);
}

static bool serial_cd2401_irq_line_asserted(const struct serial_cd2401_priv *priv)
{
	/*
	 * IRQ is active low, if the irq state is high CD2401
	 * isn't waiting yet.
	 */
	if	(vic_lirq_state((void *) E17_VIC, VIC_LICR6))
		return false;

	return true;
}

#define CD2401_IRQ_ASSERT_LOOPS (1 << 16)

#define cd2401_ack_timeout for (unsigned int i = CD2401_IRQ_ASSERT_LOOPS; i > 0; cd2401_irq_delay(), i--)

static int serial_cd2401_ack_irq(struct serial_cd2401_priv *priv,
								 unsigned int ipl,
								 u8 *vector)
{
	u8 v = 0xff;

	cd2401_ack_timeout {
		if (!serial_cd2401_irq_line_asserted(priv)) {
			wtf[8]++;
			continue;
		}
		else
			wtf[9]++;

		/* Interrupt ack */
		v = readb(E17_SERIAL_IACK + ipl);
		if (vector)
			*vector = v;

		return 0;
	};

	return -ETIMEDOUT;
}

static int serial_cd2401_ack_tx_irq(struct serial_cd2401_priv *priv)
{
	u8 vector = 0;

	cd2401_ack_timeout {
		int ret = serial_cd2401_ack_irq(priv, CD2401_TX_IPL, &vector);
		if (ret)
			return ret;

		/* This is not the interrupt you are looking for */
		if (cd2401_vector_type(vector) != CD2401_VECTOR_TYPE_TXD) {
			wtf[3] = vector;
		//	return -EIO;
		}
		else
			wtf[4] = vector;

		if (!(serial_cd2401_read_reg(priv, CD2401_TIR) & CD2401_TIR_TACT))
			continue;

		/* Not for this port */
		if (cd2401_interrupting_port(priv->base) != 0) {
			serial_cd2401_write_reg(priv, CD2401_TEOIR, CD2401_TEOIR_NOTRANS);
			continue;
		}

		return 0;
	}

	return -ETIMEDOUT;
}

static int serial_cd2401_write_tx(struct udevice *dev, const u8* data, unsigned int len)
{
	struct serial_cd2401_priv *priv = dev_get_priv(dev);
	unsigned int cnt;
	int written, ret;

	serial_cd2401_select_chan(priv, 0);
	serial_cd2401_enable_tx_irqs(priv);
	ret = serial_cd2401_ack_tx_irq(priv);
	if (ret)
		goto out;

	cnt = serial_cd2401_read_reg(priv, CD2401_TFTC);

	for (int written = 0; (written < cnt) && (written < len); written++)
		writeb(*(data + written), priv->base + CD2401_TDR);

	writeb(0, priv->base + CD2401_TEOIR);
	serial_cd2401_ack_tx_irq(priv);
	writeb(CD2401_TEOIR_NOTRANS, priv->base + CD2401_TEOIR);

out:
	serial_cd2401_disable_tx_irqs(priv);

	return ret;
}

static int serial_cd2401_putc(struct udevice *dev, const unsigned char ch)
{
	wtf[0]++;
	return serial_cd2401_write_tx(dev, &ch, 1);
}

static int serial_cd2401_ack_rx_irq(struct serial_cd2401_priv *priv)
{
	u8 vector = 0;

	cd2401_ack_timeout {
		int ret = serial_cd2401_ack_irq(priv, CD2401_RX_IPL, &vector);
		if (ret)
			return ret;

		/* This is not the interrupt you are looking for */
		if (cd2401_vector_type(vector) != CD2401_VECTOR_TYPE_RXD) {
			wtf[5] = vector;
			//return -EIO;
		}
		else
			wtf[6] = vector;

		if (!(serial_cd2401_read_reg(priv, CD2401_RIR) & CD2401_RIR_RACT)) {
			continue;
		}
		/* Not for this port */
		if (cd2401_interrupting_port(priv->base) != 0) {
			serial_cd2401_write_reg(priv, CD2401_REOIR, CD2401_REOIR_NOTRANS);
			continue;
		}

		return 0;
	}

	return -ETIMEDOUT;
}

static int serial_cd2401_drain_rx(struct serial_cd2401_priv *priv)
{
	unsigned int cnt;
	int ret;

	/* Get the CD2401 into the rx interrupt state */
	serial_cd2401_select_chan(priv, 0);
	serial_cd2401_enable_rx_irqs(priv);
	ret = serial_cd2401_ack_rx_irq(priv);
	if (ret)
		return ret;

	/* Completely empty the fifo */
	cnt = serial_cd2401_read_reg(priv, CD2401_RFOC);
	for (int i = 0; i < cnt; i++) {
		u8 ch = serial_cd2401_read_reg(priv, CD2401_RDR);
		serial_cd2401_local_rx_fifo_push(priv, ch);
	}

	/* Finish the interrupt */
	serial_cd2401_write_reg(priv, CD2401_REOIR, 0);

	return ret;
}

static int serial_cd2401_getc(struct udevice *dev)
{
	struct serial_cd2401_priv *priv = dev_get_priv(dev);
	int ret;
	wtf[1]++;

	if (serial_cd2401_local_rx_fifo_empty(priv))
		ret = serial_cd2401_drain_rx(priv);
	if (ret)
		return ret;

	if (!serial_cd2401_local_rx_fifo_empty(priv))
		return serial_cd2401_local_rx_fifo_pop(priv);

	return -EAGAIN;
}
#if 0
/* todo use the vic signal ? */
/* No pending RX int? */
if (!(readb(priv->base + CD2401_RIR) & CD2401_RIR_REN))
	ret = 0;
#endif

static int serial_cd2401_pending(struct udevice *dev, bool input)
{
	struct serial_cd2401_priv *priv = dev_get_priv(dev);
	int ret = 1;
	wtf[2]++;

	if (input) {
		/* Have preloaded data */
		if (!serial_cd2401_local_rx_fifo_empty(priv))
			return 1;

		/*
		 * There doesn't seem to be a good way to check how much
		 * incoming data there is without going into the interrupt
		 * handler path so we leave the RX int on and check if it's
		 * asserted.
		 */
		serial_cd2401_select_chan(priv, 0);
		if (!serial_cd2401_irq_line_asserted(priv))
			ret = 0;
	}

	return ret;
}

#ifdef CONFIG_DEBUG_CD2401_SERIAL
#include <debug_uart.h>
#include <asm/e17/e17.h>

static inline void _debug_uart_init(void)
{
}

static inline void _debug_uart_putc(int ch)
{
	void *base = (void*) CONFIG_DEBUG_UART_BASE;

	while (!(readb(base + CD2401_TISR) & CD2401_TISR_TXEMPTY)) {

	}

	writeb(ch, base + CD2401_TDR);
	writeb(0, base + CD2401_TEOIR);
}

DEBUG_UART_FUNCS
#endif

static int serial_cd2401_probe(struct udevice *dev)
{
	struct serial_cd2401_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	priv->rx_head = 0;
	priv->rx_tail = 0;

	for (int i = 0; i < 16; i++)
		wtf[i] = 0;

	/* clean up other channels */
	for (int i = 1; i < 3; i++) {
		serial_cd2401_select_chan(priv, i);
		serial_cd2401_disable_irqs(priv);
		writeb(i << 2, priv->base + CD2401_LIV);
	}

	serial_cd2401_select_chan(priv, 0);

	/*
	 * Make sure DMA is not enabled, async mode has to
	 * be selected until we do the init thing.
	 */
	writeb(CD2401_CMR_ASYNC, priv->base + CD2401_CMR);

	/* Set the interrupt vector to a bogus known value */
	writeb(0, priv->base + CD2401_LIV);

	/*
	 * Set the tx and rx ipls to different values so we
	 * can ack them individually.
	 */
	writeb(CD2401_TX_IPL, priv->base + CD2401_TPILR);
	writeb(CD2401_RX_IPL, priv->base + CD2401_RPILR);

	/*
	 * Wait for any in-progress command to complete,
	 * not that there should be a command running...
	 */
	while(serial_cd2401_read_reg(priv, CD2401_CCR) != 0) {
	}

	writeb(CD2401_CCR_ENBRX | CD2401_CCR_ENBXMTR,
			priv->base + CD2401_CCR);

	/*
	 * Finally, enable the rx interrupt, we'll leave this
	 * on while u-boot is running.
	 */
	serial_cd2401_enable_rx_irqs(priv);

	return 0;
}

static const struct dm_serial_ops serial_cd2401_ops = {
	.setbrg  = serial_cd2401_setbrg,
	.putc    = serial_cd2401_putc,
	.getc    = serial_cd2401_getc,
	.pending = serial_cd2401_pending,
};

static const struct udevice_id serial_cd2401_ids[] = {
	{ .compatible = "cirrus,cd2401" },
	{ }
};

U_BOOT_DRIVER(serial_scc) = {
	.name      = "serial_cd2401",
	.id        = UCLASS_SERIAL,
	.of_match  = serial_cd2401_ids,
	.priv_auto = sizeof(struct serial_cd2401_priv),
	.probe     = serial_cd2401_probe,
	.flags     = DM_FLAG_PRE_RELOC,
	.ops       = &serial_cd2401_ops,
};
