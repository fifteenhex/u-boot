// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * NuBus DP8390 (NS8390) Ethernet driver for the classic 68k Macintosh.
 *
 * Polled U-Boot port of the shared-memory, 16-bit-access variant of Linux's
 * mac8390 (the "Apple/Asante/sane" family): the DP8390 registers are memory
 * mapped with a "back4" spacing (register R at reg_base + (15 - R) * 4) and the
 * receive/transmit ring lives in on-card RAM that must be accessed 16 bits at a
 * time.  The card, its register/RAM base (NuBus MinorBaseOS) and its station
 * address are discovered from the NuBus declaration ROM by nubus_find_eth();
 * the board binds this driver manually (no device tree) when a card is present.
 *
 * References: Linux drivers/net/ethernet/8390/{mac8390.c,lib8390.c}, National
 * Semiconductor DP8390D datasheet.
 */

#include <dm.h>
#include <errno.h>
#include <net.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/types.h>
#include "oldmac_eth.h"

/* Bus-error-safe probing from nubus_buserr.S, reused to map the slot. */
int nubus_read_safe(void *addr, unsigned char *out);
int nubus_read16_safe(void *addr, unsigned short *out);
void nubus_buserr_begin(void);
void nubus_buserr_end(void);

/* DP8390 command register (register 0) bits */
#define E8390_STOP	0x01
#define E8390_START	0x02
#define E8390_TRANS	0x04
#define E8390_NODMA	0x20
#define E8390_PAGE0	0x00
#define E8390_PAGE1	0x40

/* Page-0 registers */
#define EN0_STARTPG	0x01
#define EN0_STOPPG	0x02
#define EN0_BOUNDARY	0x03
#define EN0_TPSR	0x04
#define EN0_TCNTLO	0x05
#define EN0_TCNTHI	0x06
#define EN0_ISR		0x07
#define EN0_RCNTLO	0x0a
#define EN0_RCNTHI	0x0b
#define EN0_RXCR	0x0c
#define EN0_TXCR	0x0d
#define EN0_DCFG	0x0e
#define EN0_IMR		0x0f

/* Page-1 registers */
#define EN1_PHYS	0x01	/* PAR0..5 at 1..6 */
#define EN1_CURPAG	0x07
#define EN1_MULT	0x08	/* MAR0..7 at 8..15 */

/* Interrupt status bits */
#define ENISR_RX	0x01
#define ENISR_TX	0x02
#define ENISR_OVER	0x10
#define ENISR_RDC	0x40

#define TX_PAGES	12	/* two Tx slots, matching Linux 8390.h */
#define ETH_MIN_LEN	60

struct mac8390_priv {
	ulong	reg_base;	/* DP8390 register block */
	ulong	mem_start;	/* on-card shared RX/TX ring RAM */
	u8	tx_start_page;
	u8	rx_start_page;
	u8	stop_page;
	u8	next;		/* next ring page we will read */
};

/*
 * The card's registers and ring RAM live in the 0xFxxxxxxx NuBus super-slot
 * space, which U-Boot leaves cacheable once the boot block's teardown cleared
 * DTT1.  That is fatal for device access: an 040 byte/word write to a cacheable
 * address pulls a 16-byte burst line-fill first, which bus-errors on the sparse
 * register block (and would return stale data from the ring RAM).  Point DTT1
 * at 0xF0000000-0xFFFFFFFF as identity + cache-inhibited so every access is
 * precise.  Only reached on a machine that actually has the card, and that
 * machine has no other user of this region (its on-board video is disabled).
 */
static inline void mac8390_cache_inhibit(void)
{
	ulong ttr = 0xf00fe060;		/* base 0xF0, mask 0x0F, E, cache-inhibit */

	asm volatile("movec %0,%%dtt1" : : "d"(ttr));
}

/* Register R lives at reg_base + (15 - R) * 4 ("back4" spacing), byte-wide. */
static inline u8 nr(struct mac8390_priv *p, int reg)
{
	return readb((void *)(p->reg_base + (15 - reg) * 4));
}

static inline void nw(struct mac8390_priv *p, int reg, u8 val)
{
	writeb(val, (void *)(p->reg_base + (15 - reg) * 4));
}

/*
 * The ring RAM only answers 16-bit accesses.  On the big-endian 68k a 16-bit
 * read/write preserves the on-card byte order, so these behave like a byte
 * copy done a word at a time.  @off is a byte offset into the ring RAM.
 */
static void mem_from(struct mac8390_priv *p, void *dst, ulong off, int count)
{
	volatile u16 *from = (volatile u16 *)(p->mem_start + off);
	u16 *to = dst;

	for (count = (count + 1) / 2; count > 0; count--)
		*to++ = *from++;
}

static void mem_to(struct mac8390_priv *p, ulong off, const void *src, int count)
{
	volatile u16 *to = (volatile u16 *)(p->mem_start + off);
	const u16 *from = src;

	for (count = (count + 1) / 2; count > 0; count--)
		*to++ = *from++;
}

/* Probe the on-card RAM size (4 KiB granularity) by writing and reading back. */
static ulong mac8390_memsize(struct mac8390_priv *p)
{
	ulong i;

	writew(0x1234, (void *)p->mem_start);
	for (i = 0x1000; i < 0x8000; i += 0x1000) {
		writew(0xa5a5, (void *)(p->mem_start + i));
		if (readw((void *)p->mem_start) != 0x1234)
			break;			/* aliased back to offset 0 */
		if (readw((void *)(p->mem_start + i)) != 0xa5a5)
			break;			/* write did not stick */
	}
	return i;
}

static int mac8390_start(struct udevice *dev)
{
	struct mac8390_priv *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	int i;

	nw(priv, 0, E8390_NODMA | E8390_PAGE0 | E8390_STOP);
	nw(priv, EN0_DCFG, 0x49);		/* word transfer, normal, FIFO 8 */
	nw(priv, EN0_RCNTLO, 0);
	nw(priv, EN0_RCNTHI, 0);
	nw(priv, EN0_RXCR, 0x20);		/* monitor while we set up */
	nw(priv, EN0_TXCR, 0x02);		/* internal loopback */
	nw(priv, EN0_TPSR, priv->tx_start_page);
	nw(priv, EN0_BOUNDARY, priv->rx_start_page);
	nw(priv, EN0_STARTPG, priv->rx_start_page);
	nw(priv, EN0_STOPPG, priv->stop_page);
	nw(priv, EN0_ISR, 0xff);
	nw(priv, EN0_IMR, 0x00);		/* polled: no interrupts */

	nw(priv, 0, E8390_NODMA | E8390_PAGE1 | E8390_STOP);
	for (i = 0; i < 6; i++)
		nw(priv, EN1_PHYS + i, pdata->enetaddr[i]);
	for (i = 0; i < 8; i++)
		nw(priv, EN1_MULT + i, 0);
	nw(priv, EN1_CURPAG, priv->rx_start_page + 1);

	nw(priv, 0, E8390_NODMA | E8390_PAGE0 | E8390_START);
	nw(priv, EN0_TXCR, 0x00);		/* normal transmit */
	nw(priv, EN0_RXCR, 0x04);		/* accept broadcast + our unicast */

	priv->next = priv->rx_start_page + 1;
	return 0;
}

static int mac8390_send(struct udevice *dev, void *packet, int length)
{
	struct mac8390_priv *priv = dev_get_priv(dev);
	int tmo;

	if (length < ETH_MIN_LEN)
		length = ETH_MIN_LEN;

	mem_to(priv, priv->tx_start_page << 8, packet, length);

	nw(priv, 0, E8390_NODMA | E8390_PAGE0 | E8390_START);
	nw(priv, EN0_TCNTLO, length & 0xff);
	nw(priv, EN0_TCNTHI, length >> 8);
	nw(priv, EN0_TPSR, priv->tx_start_page);
	nw(priv, 0, E8390_NODMA | E8390_TRANS | E8390_START);

	for (tmo = 0; tmo < 100000; tmo++) {
		if (nr(priv, EN0_ISR) & (ENISR_TX | 0x08 /* TX_ERR */))
			break;
		udelay(1);
	}
	nw(priv, EN0_ISR, ENISR_TX | 0x08);
	return 0;
}

static int mac8390_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct mac8390_priv *priv = dev_get_priv(dev);
	static uchar rxbuf[PKTSIZE_ALIGN];
	u8 hdr[4], curr, next_page;
	int len;
	ulong off;

	/* Current write page (page 1); no packet if it equals our read page. */
	nw(priv, 0, E8390_NODMA | E8390_PAGE1 | E8390_START);
	curr = nr(priv, EN1_CURPAG);
	nw(priv, 0, E8390_NODMA | E8390_PAGE0 | E8390_START);
	if (curr == priv->next)
		return -EAGAIN;

	/* 4-byte ring header: status, next page, count lo, count hi. */
	off = (ulong)priv->next << 8;
	mem_from(priv, hdr, off, 4);
	next_page = hdr[1];
	len = (hdr[2] | (hdr[3] << 8)) - 4;	/* count includes the header */

	if (len < ETH_MIN_LEN || len > PKTSIZE ||
	    next_page < priv->rx_start_page || next_page > priv->stop_page) {
		/* Corrupt ring: resync to the chip and drop everything. */
		priv->next = curr;
		nw(priv, EN0_BOUNDARY, curr > priv->rx_start_page ?
		   curr - 1 : priv->stop_page - 1);
		return -EAGAIN;
	}

	/* Copy the frame out of the ring, wrapping at stop_page if needed. */
	off += 4;
	if (off + len <= (ulong)priv->stop_page << 8) {
		mem_from(priv, rxbuf, off, len);
	} else {
		int first = (priv->stop_page << 8) - off;

		mem_from(priv, rxbuf, off, first);
		mem_from(priv, rxbuf + first,
			 (ulong)priv->rx_start_page << 8, len - first);
	}

	/* Release the pages: BOUNDARY = next_page - 1 (wrap into the ring). */
	priv->next = next_page;
	nw(priv, EN0_BOUNDARY, next_page > priv->rx_start_page ?
	   next_page - 1 : priv->stop_page - 1);

	*packetp = rxbuf;
	return len;
}

static void mac8390_stop(struct udevice *dev)
{
	struct mac8390_priv *priv = dev_get_priv(dev);

	nw(priv, 0, E8390_NODMA | E8390_PAGE0 | E8390_STOP);
	nw(priv, EN0_RXCR, 0x20);		/* monitor mode */
}

static int mac8390_read_rom_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct oldmac_eth_info info;

	if (nubus_find_eth(&info) == 0)
		memcpy(pdata->enetaddr, info.enetaddr, 6);
	return 0;
}

static int mac8390_probe(struct udevice *dev)
{
	struct mac8390_priv *priv = dev_get_priv(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct oldmac_eth_info info;
	ulong base_addr, mem_size;

	if (nubus_find_eth(&info))
		return -ENODEV;

	mac8390_cache_inhibit();

	/*
	 * Linux mac8390 (Apple/sane layout): the shared RAM sits MinorBaseOS into
	 * the slot's minor space and the DP8390 registers 0x10000 above it.  Those
	 * are logical addresses in the ROM's MMU mapping; on the 040 Macs the ROM
	 * maps the register window non-identity, so U-Boot proper (MMU off) cannot
	 * reach it there.  The SPL translated both windows to physical while the ROM
	 * MMU was still on and left them at OLDMAC_ETH_XLATE_ADDR - prefer those.
	 */
	base_addr = info.slot_addr | ((ulong)(info.slot & 0xf) << 20);
	priv->mem_start = base_addr + info.minor_base;
	priv->reg_base = priv->mem_start + 0x10000;

	{
		const struct oldmac_eth_xlate *x =
			(const void *)OLDMAC_ETH_XLATE_ADDR;

		if (x->magic == OLDMAC_ETH_XLATE_MAGIC && x->slot == info.slot &&
		    x->reg_phys && x->mem_phys) {
			priv->reg_base = x->reg_phys;
			priv->mem_start = x->mem_phys;
		}
	}

	/*
	 * If the register window still is not reachable (no SPL translation, or a
	 * machine where the SPL did not run), probe it bus-error-safe and
	 * self-disable rather than register a dead eth device that only ever times
	 * out.
	 */
	{
		u16 w;
		int reachable;

		nubus_buserr_begin();
		reachable = nubus_read16_safe((void *)priv->reg_base, &w) == 0;
		nubus_buserr_end();
		if (!reachable) {
			printf("mac8390: slot %X DP8390 MAC %pM: registers unreachable (SPL translation missing?)\n",
			       info.slot, pdata->enetaddr);
			return -ENODEV;
		}
	}

	/* Registers reachable: size the ring RAM and set up the page layout. */
	mem_size = mac8390_memsize(priv);
	priv->tx_start_page = 0;
	priv->rx_start_page = TX_PAGES;
	priv->stop_page = mem_size >> 8;

	memcpy(pdata->enetaddr, info.enetaddr, 6);

	printf("mac8390: slot %X DP8390, regs %08lx ram %08lx (%luK), MAC %pM\n",
	       info.slot, priv->reg_base, priv->mem_start, mem_size >> 10,
	       pdata->enetaddr);
	return 0;
}

static const struct eth_ops mac8390_ops = {
	.start		= mac8390_start,
	.send		= mac8390_send,
	.recv		= mac8390_recv,
	.stop		= mac8390_stop,
	.read_rom_hwaddr = mac8390_read_rom_hwaddr,
};

U_BOOT_DRIVER(mac8390) = {
	.name		= "mac8390",
	.id		= UCLASS_ETH,
	.probe		= mac8390_probe,
	.ops		= &mac8390_ops,
	.priv_auto	= sizeof(struct mac8390_priv),
	.plat_auto	= sizeof(struct eth_pdata),
};
