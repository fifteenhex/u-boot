// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * National Semiconductor DP8393x "SONIC" Ethernet, as found on-board classic
 * 68k Macintosh machines such as the Quadra 800 (and emulated by QEMU's q800).
 *
 * Polled U-Boot driver.  The SONIC is a DMA controller that walks descriptor
 * rings in main memory: a CAM descriptor area (address filter), a receive
 * resource area (RX buffers), a receive descriptor area, and a transmit
 * descriptor area.  On the Quadra the chip sits on a 32-bit big-endian bus, so
 * each 16-bit register lives at reg<<2 and each 16-bit descriptor field is the
 * low half of a 32-bit big-endian word.  Register layout and the init sequence
 * follow Linux drivers/net/ethernet/natsemi/{sonic,macsonic}.c, validated
 * against QEMU hw/net/dp8393x.c.
 *
 * The driver only binds on models whose capability table advertises a SONIC
 * (see board oldmac_sonic_base()), so it stays inert on other machines.
 */

#include <dm.h>
#include <net.h>
#include <cpu_func.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/string.h>

/* provided by the board: model-gated SONIC + MAC-PROM base, 0 if absent */
ulong oldmac_sonic_base(void);
ulong oldmac_sonic_prom(void);

/* register indices (byte offset = index << 2 on the Quadra) */
#define SONIC_CMD	0x00
#define SONIC_DCR	0x01
#define SONIC_RCR	0x02
#define SONIC_TCR	0x03
#define SONIC_IMR	0x04
#define SONIC_ISR	0x05
#define SONIC_UTDA	0x06
#define SONIC_CTDA	0x07
#define SONIC_URDA	0x0d
#define SONIC_CRDA	0x0e
#define SONIC_EOBC	0x13
#define SONIC_URRA	0x14
#define SONIC_RSA	0x15
#define SONIC_REA	0x16
#define SONIC_RRP	0x17
#define SONIC_RWP	0x18
#define SONIC_CE	0x25
#define SONIC_CDP	0x26
#define SONIC_CDC	0x27
#define SONIC_DCR2	0x3f

/* command register bits */
#define CR_LCAM		0x0200
#define CR_RRRA		0x0100
#define CR_RST		0x0080
#define CR_ST		0x0020
#define CR_STP		0x0010
#define CR_RXEN		0x0008
#define CR_RXDIS	0x0004
#define CR_TXP		0x0002
/* self-clearing command bits (STP/RXDIS persist and must not be waited on) */
#define CR_ALL		(CR_LCAM | CR_RRRA | CR_RXEN | CR_TXP)

/* data configuration: external bus, block-mode DMA, FIFO thresholds, 32-bit */
#define DCR_EXBUS	0x8000
#define DCR_BMS		0x0010
#define DCR_RFT1	0x0008
#define DCR_TFT0	0x0001
#define DCR_DW		0x0020
#define DCR_VALUE	(DCR_EXBUS | DCR_BMS | DCR_RFT1 | DCR_TFT0 | DCR_DW)

#define RCR_BRD		0x2000	/* accept broadcast */
#define RCR_PRX		0x0001	/* packet received OK */
#define RCR_LPKT	0x0040	/* last packet in buffer */

#define TCR_PTX		0x0001	/* packet transmitted OK */

#define ISR_PKTRX	0x0400
#define ISR_TXDN	0x0200
#define ISR_RBE		0x0020	/* receive buffers exhausted */

#define SONIC_EOL	0x0001	/* end-of-list flag in a descriptor link */

/* descriptor field indices */
#define RR_BUFADR_L	0
#define RR_BUFADR_H	1
#define RR_BUFSIZE_L	2
#define RR_BUFSIZE_H	3
#define RD_STATUS	0
#define RD_PKTLEN	1
#define RD_PKTPTR_L	2
#define RD_PKTPTR_H	3
#define RD_SEQNO	4
#define RD_LINK		5
#define RD_IN_USE	6
#define TD_STATUS	0
#define TD_CONFIG	1
#define TD_PKTSIZE	2
#define TD_FRAG_COUNT	3
#define TD_FRAG_PTR_L	4
#define TD_FRAG_PTR_H	5
#define TD_FRAG_SIZE	6
#define TD_LINK		7
#define CD_ENTRY	0
#define CD_CAP0		1
#define CD_CAP1		2
#define CD_CAP2		3

#define NUM_RRS		4
#define NUM_RDS		4
#define RBSIZE		1520
#define RXBUF_STRIDE	0x800

/* offsets of each structure within the 64 KiB-aligned DMA region */
#define OFF_CDA		0x0000
#define OFF_TDA		0x0040
#define OFF_RDA		0x0080
#define OFF_RRA		0x0100
#define OFF_RXBUF	0x1000
#define OFF_TXBUF	0x3000

/* The SONIC addresses descriptors and buffers as {URRA/UTDA upper 16 bits} |
 * {16-bit offset}, so the whole region must sit inside one 64 KiB page.  A
 * plain __aligned(0x10000) array only fixes the *link-time* address; U-Boot then
 * relocates itself by a non-64-KiB offset, leaving the runtime base unaligned
 * and straddling a page boundary.  So over-allocate and align the base at
 * runtime (see sonic_dma_init()). */
static u8 sonic_dma_buf[0x20000] __aligned(0x10000);
static u8 *sonic_dma;		/* 64 KiB-aligned base within sonic_dma_buf */

static void sonic_dma_init(void)
{
	sonic_dma = (u8 *)(((uintptr_t)sonic_dma_buf + 0xffff) & ~(uintptr_t)0xffff);
}

struct sonic_priv {
	void	*base;
	int	cur_rx;		/* next RX descriptor to inspect */
	int	eol_rx;		/* RX descriptor currently flagged EOL */
	u8	rxbuf[RBSIZE];	/* bounce buffer handed to the net stack */
};

/*
 * 16-bit big-endian register access.  Each register occupies a 32-bit slot at
 * base + (reg << 2), but the onboard Quadra SONIC drives its 16-bit registers on
 * the low half of the bus, so the value lives at byte offset +2 (this is Linux
 * macsonic's reg_offset = 2 for onboard/32-bit cards).  QEMU's dp8393x ignores
 * the low address bits, so offset 0 also worked under emulation, but real
 * hardware reads 0xffff (the floating high half) without the +2.
 */
#define SONIC_REG_OFF	2
static inline u16 sonic_rd(struct sonic_priv *p, int reg)
{
	return *(volatile u16 *)((u8 *)p->base + (reg << 2) + SONIC_REG_OFF);
}

static inline void sonic_wr(struct sonic_priv *p, int reg, u16 val)
{
	*(volatile u16 *)((u8 *)p->base + (reg << 2) + SONIC_REG_OFF) = val;
}

/* --- descriptor field access: each 16-bit field is a 32-bit BE word --- */
static inline void dput(u32 off, int field, u16 val)
{
	*(volatile u32 *)(sonic_dma + off + field * 4) = val;
}

static inline u16 dget(u32 off, int field)
{
	return *(volatile u32 *)(sonic_dma + off + field * 4);
}

static u32 sonic_dma_addr(u32 off)
{
	return (u32)(uintptr_t)sonic_dma + off;
}

/*
 * The descriptor region and buffers are DMA'd by the SONIC.  The 68040's
 * copyback caches are not coherent with device DMA, so push + invalidate them
 * around every hand-off: flush before the chip reads what we wrote (descriptors,
 * TX data), invalidate before we read what it wrote (RX descriptors/data).  On
 * m68k flush_dcache_range() is a full CPUSHA (push + invalidate both caches), so
 * one call serves both directions.
 */
static void sonic_flush(void)
{
	flush_dcache_range((ulong)sonic_dma, (ulong)sonic_dma + 0x10000);
}

static void sonic_invalidate(void)
{
	flush_dcache_range((ulong)sonic_dma, (ulong)sonic_dma + 0x10000);
}

/* Wait until the given command bits clear (command completed). */
static int sonic_quiesce(struct sonic_priv *p, u16 mask)
{
	int timeout = 100000;

	while (sonic_rd(p, SONIC_CMD) & mask)
		if (--timeout <= 0)
			return -ETIMEDOUT;
	return 0;
}

static u8 revbit8(u8 b)
{
	b = (b >> 4) | (b << 4);
	b = ((b & 0xcc) >> 2) | ((b & 0x33) << 2);
	b = ((b & 0xaa) >> 1) | ((b & 0x55) << 1);
	return b;
}

static int sonic_read_rom_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	ulong prom = oldmac_sonic_prom();
	int i;

	if (!prom)
		return -ENODEV;

	/* the MAC bytes are stored bit-reversed in the PROM */
	for (i = 0; i < 6; i++)
		pdata->enetaddr[i] = revbit8(readb((void *)(prom + i)));

	return 0;
}

static void sonic_setup_rings(struct sonic_priv *p, const u8 *mac)
{
	u32 rra = sonic_dma_addr(OFF_RRA);
	u32 rda = sonic_dma_addr(OFF_RDA);
	u32 tda = sonic_dma_addr(OFF_TDA);
	u32 cda = sonic_dma_addr(OFF_CDA);
	int i;

	/* receive resource area: one RX buffer per entry */
	for (i = 0; i < NUM_RRS; i++) {
		u32 buf = sonic_dma_addr(OFF_RXBUF + i * RXBUF_STRIDE);
		u32 e = OFF_RRA + i * (4 * 4);

		dput(e, RR_BUFADR_L, buf & 0xffff);
		dput(e, RR_BUFADR_H, buf >> 16);
		dput(e, RR_BUFSIZE_L, RBSIZE >> 1);
		dput(e, RR_BUFSIZE_H, 0);
	}

	/* receive descriptors: circular list, last flagged EOL */
	for (i = 0; i < NUM_RDS; i++) {
		u32 e = OFF_RDA + i * (7 * 4);
		u16 link = (rda & 0xffff) + ((i + 1) % NUM_RDS) * (7 * 4);

		dput(e, RD_STATUS, 0);
		dput(e, RD_PKTLEN, 0);
		dput(e, RD_PKTPTR_L, 0);
		dput(e, RD_PKTPTR_H, 0);
		dput(e, RD_SEQNO, 0);
		dput(e, RD_IN_USE, 1);
		dput(e, RD_LINK, link | (i == NUM_RDS - 1 ? SONIC_EOL : 0));
	}
	p->cur_rx = 0;
	p->eol_rx = NUM_RDS - 1;

	/* single transmit descriptor, links back to itself */
	dput(OFF_TDA, TD_LINK, tda & 0xffff);

	/* CAM: entry 0 = our MAC, then the CAM-enable word */
	dput(OFF_CDA, CD_ENTRY, 0);
	dput(OFF_CDA, CD_CAP0, mac[1] << 8 | mac[0]);
	dput(OFF_CDA, CD_CAP1, mac[3] << 8 | mac[2]);
	dput(OFF_CDA, CD_CAP2, mac[5] << 8 | mac[4]);
	dput(OFF_CDA, 4, 0x0001);	/* CAM enable for entry 0 */

	sonic_flush();

	/* point the chip at the receive resource area */
	sonic_wr(p, SONIC_URRA, rra >> 16);
	sonic_wr(p, SONIC_RSA, OFF_RRA);
	sonic_wr(p, SONIC_REA, OFF_RRA + NUM_RRS * (4 * 4));
	sonic_wr(p, SONIC_RRP, OFF_RRA);
	sonic_wr(p, SONIC_RWP, OFF_RRA + (NUM_RRS - 1) * (4 * 4));
	sonic_wr(p, SONIC_EOBC, (RBSIZE >> 1) - 2);

	sonic_wr(p, SONIC_URDA, rda >> 16);
	sonic_wr(p, SONIC_CRDA, rda & 0xffff);

	sonic_wr(p, SONIC_UTDA, tda >> 16);
	sonic_wr(p, SONIC_CTDA, tda & 0xffff);

	sonic_wr(p, SONIC_CDP, cda & 0xffff);
	sonic_wr(p, SONIC_CDC, 1);
}

static int sonic_start(struct udevice *dev)
{
	struct sonic_priv *p = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	int ret;

	/* software reset; DCR is only writable while in reset */
	sonic_wr(p, SONIC_CMD, CR_RST);
	sonic_wr(p, SONIC_DCR, DCR_VALUE);
	sonic_wr(p, SONIC_DCR2, 0);
	sonic_wr(p, SONIC_CE, 0);

	/* leave reset, stop and disable the receiver, wait for it to settle */
	sonic_wr(p, SONIC_CMD, 0);
	sonic_wr(p, SONIC_CMD, CR_RXDIS | CR_STP);
	ret = sonic_quiesce(p, CR_ALL);
	if (ret)
		return ret;

	sonic_setup_rings(p, pdata->enetaddr);

	/* load the receive resource pointers */
	sonic_wr(p, SONIC_CMD, CR_RRRA);
	ret = sonic_quiesce(p, CR_RRRA);
	if (ret)
		return ret;

	/* load the CAM address filter */
	sonic_wr(p, SONIC_CMD, CR_LCAM);
	ret = sonic_quiesce(p, CR_LCAM);
	if (ret)
		return ret;

	sonic_wr(p, SONIC_RCR, RCR_BRD);	/* our unicast (CAM) + broadcast */
	sonic_wr(p, SONIC_TCR, 0);
	sonic_wr(p, SONIC_ISR, 0x7fff);		/* clear pending */
	sonic_wr(p, SONIC_IMR, 0);		/* polled: no interrupts */
	sonic_wr(p, SONIC_CMD, CR_RXEN);	/* enable the receiver */

	return 0;
}

static int sonic_send(struct udevice *dev, void *packet, int length)
{
	struct sonic_priv *p = dev_get_priv(dev);
	u32 txbuf = sonic_dma_addr(OFF_TXBUF);
	int timeout = 100000;

	if (length < 60)		/* pad short frames to the minimum */
		length = 60;

	memcpy(sonic_dma + OFF_TXBUF, packet, length);

	dput(OFF_TDA, TD_STATUS, 0);
	dput(OFF_TDA, TD_CONFIG, 0);
	dput(OFF_TDA, TD_PKTSIZE, length);
	dput(OFF_TDA, TD_FRAG_COUNT, 1);
	dput(OFF_TDA, TD_FRAG_PTR_L, txbuf & 0xffff);
	dput(OFF_TDA, TD_FRAG_PTR_H, txbuf >> 16);
	dput(OFF_TDA, TD_FRAG_SIZE, length);
	dput(OFF_TDA, TD_LINK, (sonic_dma_addr(OFF_TDA) & 0xffff) | SONIC_EOL);
	sonic_flush();

	sonic_wr(p, SONIC_CTDA, sonic_dma_addr(OFF_TDA) & 0xffff);
	sonic_wr(p, SONIC_CMD, CR_TXP);

	/* wait for transmit to complete */
	while (sonic_rd(p, SONIC_CMD) & CR_TXP)
		if (--timeout <= 0)
			return -ETIMEDOUT;

	sonic_wr(p, SONIC_ISR, ISR_TXDN);
	sonic_invalidate();

	return (dget(OFF_TDA, TD_STATUS) & TCR_PTX) ? 0 : -EIO;
}

static int sonic_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct sonic_priv *p = dev_get_priv(dev);
	u32 e = OFF_RDA + p->cur_rx * (7 * 4);
	u32 eol = OFF_RDA + p->eol_rx * (7 * 4);
	u16 status;
	int len;

	sonic_invalidate();

	/* the SONIC clears in_use in a descriptor once it has filled it */
	if (dget(e, RD_IN_USE))
		return -EAGAIN;

	status = dget(e, RD_STATUS);
	len = dget(e, RD_PKTLEN);

	if ((status & RCR_PRX) && len > 4) {
		u32 addr = (dget(e, RD_PKTPTR_H) << 16) | dget(e, RD_PKTPTR_L);

		len -= 4;			/* drop the FCS */
		memcpy(p->rxbuf, (void *)(uintptr_t)addr, len);
		*packetp = p->rxbuf;
	} else {
		len = 0;			/* error frame; recycle silently */
	}

	/* recycle this descriptor: mark free, move the EOL flag onto it and
	 * clear it from the previous one, then hand a resource back */
	dput(e, RD_STATUS, 0);
	dput(e, RD_IN_USE, 1);
	dput(e, RD_LINK, dget(e, RD_LINK) | SONIC_EOL);
	dput(eol, RD_LINK, dget(eol, RD_LINK) & ~SONIC_EOL);
	sonic_flush();
	p->eol_rx = p->cur_rx;
	p->cur_rx = (p->cur_rx + 1) % NUM_RDS;

	/* Return one receive buffer resource to the chip, then, if the chip had
	 * exhausted its buffers (RBE), clear RBE so it resumes reception.  Order
	 * matters (per the SONIC): sample RBE *before* advancing RWP and clear it
	 * *after*, so a fresh RBE that asserts in between is not lost.  Without
	 * this the receiver halts permanently once all buffers fill on a busy
	 * network, silently dropping everything (e.g. the DHCP offer). */
	{
		int rbe = sonic_rd(p, SONIC_ISR) & ISR_RBE;
		u16 rwp = sonic_rd(p, SONIC_RWP) + (4 * 4);

		if (rwp >= OFF_RRA + NUM_RRS * (4 * 4))
			rwp = OFF_RRA;
		sonic_wr(p, SONIC_RWP, rwp);
		if (rbe)
			sonic_wr(p, SONIC_ISR, ISR_RBE);
	}
	sonic_wr(p, SONIC_ISR, ISR_PKTRX);

	return len ? len : -EAGAIN;
}

static void sonic_stop(struct udevice *dev)
{
	struct sonic_priv *p = dev_get_priv(dev);

	sonic_wr(p, SONIC_CMD, CR_RXDIS | CR_STP);
	sonic_wr(p, SONIC_CMD, CR_RST);
}

static int sonic_probe(struct udevice *dev)
{
	struct sonic_priv *p = dev_get_priv(dev);
	ulong base = oldmac_sonic_base();

	/* gate on the detected machine: no SONIC on this model -> not present */
	if (!base)
		return -ENODEV;

	p->base = (void *)base;
	sonic_dma_init();

	return 0;
}

static const struct eth_ops sonic_ops = {
	.start		= sonic_start,
	.send		= sonic_send,
	.recv		= sonic_recv,
	.stop		= sonic_stop,
	.read_rom_hwaddr = sonic_read_rom_hwaddr,
};

U_BOOT_DRIVER(sonic_eth) = {
	.name	= "sonic_eth",
	.id	= UCLASS_ETH,
	.probe	= sonic_probe,
	.ops	= &sonic_ops,
	.priv_auto	= sizeof(struct sonic_priv),
	.plat_auto	= sizeof(struct eth_pdata),
};
