// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * AMD/NCR 53C9x "ESP" SCSI driver (polled, pseudo-DMA) for classic 68k
 * Macintosh machines such as the Quadra 800, as emulated by QEMU's q800.
 *
 * The controller registers are byte-wide, spaced 16 bytes apart (it_shift=4);
 * data is moved through a separate pseudo-DMA window rather than real DMA.
 *
 * On real hardware that window inserts bus wait-states until the ESP asserts its
 * DMA request (DRQ), so touching it before a byte is ready stalls the CPU
 * forever.  DRQ is exposed as bit 0 of the VIA2 interrupt flag register, so we
 * poll that (and bail on an ESP interrupt = transfer done/aborted) before every
 * window access.  QEMU models this same live DRQ bit (hw/misc/mac_via.c), so the
 * one code path works on both QEMU q800 and a real Quadra.
 */

#include <dm.h>
#include <scsi.h>
#include <asm/io.h>
#include <linux/delay.h>

/* register indices (multiply by 16 for the byte offset) */
#define ESP_TCLO	0x0	/* transfer count low */
#define ESP_TCMID	0x1	/* transfer count mid */
#define ESP_FIFO	0x2	/* FIFO data */
#define ESP_CMD		0x3	/* command */
#define ESP_STAT	0x4	/* (r) status */
#define ESP_BUSID	0x4	/* (w) select/reselect bus id */
#define ESP_INTR	0x5	/* (r) interrupt */
#define ESP_STIMEG	0x5	/* (w) select/reselect timeout */
#define ESP_SEQ		0x6	/* (r) sequence step */
#define ESP_SYNCTP	0x6	/* (w) synchronous transfer period */
#define ESP_FFLAGS	0x7	/* (r) FIFO flags */
#define ESP_SYNCOFF	0x7	/* (w) synchronous offset */
#define ESP_CFG1	0x8	/* configuration 1 */
#define ESP_CCF		0x9	/* (w) clock conversion factor */
#define ESP_CFG2	0xb	/* configuration 2 */
#define ESP_TCHI	0xe	/* transfer count high */

/* commands (ESP_CMD) */
#define CMD_DMA		0x80
#define CMD_NOP		0x00
#define CMD_FLUSH	0x01
#define CMD_RESET	0x02
#define CMD_BUSRESET	0x03
#define CMD_TI		0x10	/* transfer information */
#define CMD_ICCS	0x11	/* initiator command complete sequence */
#define CMD_MSGACC	0x12	/* message accepted */
#define CMD_SELATN	0x42	/* select with ATN */

/* status (ESP_STAT) */
#define STAT_PHASE	0x07
#define STAT_DO		0x00	/* data out */
#define STAT_DI		0x01	/* data in */
#define STAT_CD		0x02	/* command */
#define STAT_ST		0x03	/* status */
#define STAT_MO		0x06	/* message out */
#define STAT_MI		0x07	/* message in */
#define STAT_TC		0x10	/* terminal count */
#define STAT_INT	0x80	/* interrupt pending */

/* interrupt (ESP_INTR) */
#define INTR_FC		0x08	/* function complete */
#define INTR_BS		0x10	/* bus service */
#define INTR_DC		0x20	/* disconnect */
#define INTR_IL		0x40	/* illegal command */
#define INTR_RST	0x80	/* SCSI bus reset */

/* config 1 */
#define CFG1_HOSTID	0x07	/* host (initiator) id in low 3 bits */
#define CFG1_PARENB	0x10	/* enable parity */

#define ESP_HOST_ID	7

struct esp_priv {
	void *regs;
	void *pdma;
};

static inline u8 esp_rd(struct esp_priv *p, unsigned int reg)
{
	return readb(p->regs + (reg << 4));
}

static inline void esp_wr(struct esp_priv *p, unsigned int reg, u8 val)
{
	writeb(val, p->regs + (reg << 4));
}

/* Wait for an ESP interrupt, then latch and return the INTR register. */
static int esp_wait_intr(struct esp_priv *p, u8 *intr)
{
	int timeout = 1000000;

	while (!(esp_rd(p, ESP_STAT) & STAT_INT)) {
		if (--timeout <= 0)
			return -ETIMEDOUT;
		udelay(1);
	}
	*intr = esp_rd(p, ESP_INTR);	/* reading INTR clears the latch */

	return 0;
}

static int esp_reset(struct esp_priv *p)
{
	esp_wr(p, ESP_CMD, CMD_RESET);
	udelay(100);
	esp_wr(p, ESP_CMD, CMD_NOP);
	/* host id 7, parity off; NCR clock conversion factor and select timeout */
	esp_wr(p, ESP_CFG1, ESP_HOST_ID);
	esp_wr(p, ESP_CFG2, 0);
	esp_wr(p, ESP_CCF, 4);
	esp_wr(p, ESP_STIMEG, 0x80);
	esp_wr(p, ESP_CMD, CMD_FLUSH);

	return 0;
}

/*
 * VIA2 interrupt flag register (VIA1 + 0x2000, 6522 register 13 at 16*0x200);
 * bit 0 (CA2) reflects the live SCSI DRQ line on the Quadra.  Poll it before
 * each pseudo-DMA window access so we never stall the bus waiting for a byte
 * that is not yet in the ESP FIFO.
 */
#define VIA2_IFR	((void *)0x50f03a00)
#define VIA2_SCSI_DRQ	0x01

static int esp_wait_dreq(struct esp_priv *p)
{
	int timeout = 500000;

	while (!(readb(VIA2_IFR) & VIA2_SCSI_DRQ)) {
		if (esp_rd(p, ESP_STAT) & STAT_INT)
			return 1;	/* ESP ended the transfer (done/aborted) */
		if (--timeout <= 0)
			return -ETIMEDOUT;
		udelay(2);
	}

	return 0;
}

static int esp_data(struct esp_priv *p, struct scsi_cmd *cmd, bool in)
{
	unsigned long len = cmd->datalen;
	u8 *buf = cmd->pdata;
	u8 intr;

	esp_wr(p, ESP_TCLO, len & 0xff);
	esp_wr(p, ESP_TCMID, (len >> 8) & 0xff);
	esp_wr(p, ESP_TCHI, (len >> 16) & 0xff);
	esp_wr(p, ESP_CMD, CMD_TI | CMD_DMA);

	/*
	 * The pseudo-DMA moves a 16-bit word at a time: DRQ is asserted only while
	 * the ESP FIFO holds a full word (>= 2 bytes), so drain/fill two bytes for
	 * every DRQ.  A byte-at-a-time loop desyncs from that threshold and stalls.
	 */
	while (len >= 2) {
		if (esp_wait_dreq(p))
			goto out;	/* transfer ended early or timed out */
		/*
		 * One 16-bit access transfers a whole word: the Quadra pseudo-DMA
		 * hardware pops two bytes from the ESP per bus cycle regardless of
		 * access size, so a byte access would drain the ESP twice as fast
		 * as it fills our buffer (dropping every other byte).  Use word
		 * accesses like Linux's movew; __raw_* keeps the native byte order.
		 */
		if (in) {
			u16 w = __raw_readw(p->pdma);

			*buf++ = w >> 8;
			*buf++ = w;
		} else {
			__raw_writew((buf[0] << 8) | buf[1], p->pdma);
			buf += 2;
		}
		len -= 2;
	}
	/* trailing odd byte (rare; the word-threshold DRQ never fires for it) */
	if (len) {
		if (in)
			*buf++ = readb(p->pdma);
		else
			writeb(*buf++, p->pdma);
	}

out:
	return esp_wait_intr(p, &intr);
}

static int esp_exec(struct udevice *dev, struct scsi_cmd *cmd)
{
	struct esp_priv *p = dev_get_priv(dev);
	u8 intr, stat, phase;
	int i, ret;

	if (cmd->lun || cmd->target > 6)
		return -EINVAL;

	/* load IDENTIFY + CDB into the FIFO and select the target with ATN */
	esp_wr(p, ESP_CMD, CMD_FLUSH);
	esp_wr(p, ESP_BUSID, cmd->target);
	esp_wr(p, ESP_FIFO, 0x80 | (cmd->lun & 7));	/* IDENTIFY, no disconnect */
	for (i = 0; i < cmd->cmdlen; i++)
		esp_wr(p, ESP_FIFO, cmd->cmd[i]);
	esp_wr(p, ESP_CMD, CMD_SELATN);

	ret = esp_wait_intr(p, &intr);
	if (ret)
		return ret;
	if (intr & INTR_DC)		/* selection timeout / no device */
		return -EIO;

	/* run the SCSI phases until command complete */
	for (;;) {
		stat = esp_rd(p, ESP_STAT);
		phase = stat & STAT_PHASE;

		switch (phase) {
		case STAT_DI:
			ret = esp_data(p, cmd, true);
			break;
		case STAT_DO:
			ret = esp_data(p, cmd, false);
			break;
		case STAT_ST:
			/* status + message in: latch via ICCS */
			esp_wr(p, ESP_CMD, CMD_ICCS);
			ret = esp_wait_intr(p, &intr);
			if (ret)
				return ret;
			cmd->status = esp_rd(p, ESP_FIFO);	/* status byte */
			(void)esp_rd(p, ESP_FIFO);		/* message byte */
			/* accept the message; the target then disconnects */
			esp_wr(p, ESP_CMD, CMD_MSGACC);
			esp_wait_intr(p, &intr);		/* consume INTR_DC */
			return 0;
		case STAT_MI:
			esp_wr(p, ESP_CMD, CMD_MSGACC);
			ret = esp_wait_intr(p, &intr);
			break;
		default:
			return -EIO;
		}
		if (ret)
			return ret;
	}
}

static int esp_scsi_bus_reset(struct udevice *dev)
{
	struct esp_priv *p = dev_get_priv(dev);

	esp_wr(p, ESP_CMD, CMD_BUSRESET);
	udelay(1000);
	esp_reset(p);

	return 0;
}

/* Boards without a device tree supply the controller base here (0 = none). */
__weak phys_addr_t board_esp_base(void)
{
	return 0;
}

static int esp_scsi_probe(struct udevice *dev)
{
	struct scsi_plat *plat = dev_get_uclass_plat(dev);
	struct esp_priv *p = dev_get_priv(dev);

	if (!plat->base)
		plat->base = board_esp_base();
	if (!plat->base)
		return -ENODEV;
	p->regs = (void *)plat->base;
	p->pdma = (void *)(plat->base + 0x100);	/* PDMA window */
	plat->max_id = 7;
	plat->max_lun = 1;
	plat->max_bytes_per_req = 0x8000;

	esp_reset(p);

	return 0;
}

static struct scsi_ops esp_scsi_ops = {
	.exec		= esp_exec,
	.bus_reset	= esp_scsi_bus_reset,
};

U_BOOT_DRIVER(esp_scsi) = {
	.name		= "esp_scsi",
	.id		= UCLASS_SCSI,
	.ops		= &esp_scsi_ops,
	.probe		= esp_scsi_probe,
	.priv_auto	= sizeof(struct esp_priv),
	.flags		= DM_FLAG_PRE_RELOC,	/* bind in SPL */
};
