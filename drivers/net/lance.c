// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */

#include <linux/litex.h>

#include <dm.h>
#include <dm/device_compat.h>
#include <net.h>
#include <linux/delay.h>

#define LANCE_TMDS	1
#define LANCE_RMDS	16

#define LANCE_RXBUFFSZ 1518

#define LANCE_STACK_U32(_x,_v)	\
	_x[0] = _v & 0xffff;		\
	_x[1] = (_v >> 16) & 0xffff

#define LANCE_BCNT(_len) (_len | (0xf << 12))

#define CSR0_INIT	BIT(0)
#define CSR0_STRT	BIT(1)
#define CSR0_STOP	BIT(2)
#define CSR0_TDMD	BIT(3)
#define CSR0_TXON	BIT(4)
#define CSR0_RXON	BIT(5)
#define CSR0_INEA	BIT(6)
#define CSR0_INTR	BIT(7)
#define CSR0_IDON	BIT(8)
#define CSR0_TINT	BIT(9)
#define CSR0_RINT	BIT(10)
#define CSR0_MERR	BIT(11)
#define CSR0_MISS	BIT(12)
#define CSR0_CERR	BIT(13)
#define CSR0_BABL	BIT(14)
#define CSR0_ERR	BIT(15)

#define CSR3_BCON	BIT(0)
#define CSR3_ACON	BIT(1)
#define CSR3_BSWP	BIT(2)

#define INIT_MODE_DRX	BIT(0)
#define INIT_MODE_DTX	BIT(1)
#define INIT_MODE_LOOP	BIT(2)
#define INIT_MODE_DTCR	BIT(3)
#define INIT_MODE_COLL	BIT(4)
#define INIT_MODE_DRTY	BIT(5)
#define INIT_MODE_INTL	BIT(6)
#define INIT_MODE_PROM	BIT(15)

#define DESCRIPTOR_OWN	BIT(31)
#define DESCRIPTOR_ERR	BIT(30)
#define DESCRIPTOR_FRAM	BIT(29)
#define DESCRIPTOR_OFLO BIT(28)
#define DESCRIPTOR_CRC	BIT(27)
#define DESCRIPTOR_BUFF BIT(26)
#define DESCRIPTOR_STP	BIT(25)
#define DESCRIPTOR_ENP	BIT(24)

struct lance_initblock {
	u16 mode;
	u16 padr[3];
	u16 ladrf[4];
	u16 rdra[2];
	u16 tdra[2];
};

struct lance_rmd {
	u16 addr[2];
	u16 bcnt;
	u16 mcnt;
};

struct lance_tmd {
	u16 addr[2];
	u16 bcnt;
	u16 flags;
};

struct lance {
	struct udevice *dev;

	void __iomem *base;

	struct lance_initblock initblock;
	struct lance_rmd rmds[LANCE_RMDS];
	struct lance_tmd tmds[LANCE_TMDS];

	void *rxbuf;
	bool processedrmds[LANCE_RMDS];
	unsigned lastrxpkt;
};

static u16 lance_reg_read(struct lance *priv, unsigned int which)
{
	writew(which, priv->base + 2);
	return readw(priv->base);
}

static void lance_reg_write(struct lance *priv, unsigned int which, u16 value)
{
	writew(which, priv->base + 2);
	writew(value, priv->base);
}

static void lance_dump_initblock(struct lance *priv)
{
	u32 rdra, tdra;

	rdra = priv->initblock.rdra[1];
	rdra <<= 16;
	rdra |= priv->initblock.rdra[0];

	tdra = priv->initblock.tdra[1];
	tdra <<= 16;
	tdra |= priv->initblock.tdra[0];

	printf("mode: 0x%04x, rdra: 0x%08x, tdra: 0x%08x\n",
			(unsigned int) priv->initblock.mode,
			rdra, tdra);
}

#ifdef DEBUG
static void lance_dump_csrs(struct lance *priv)
{
	for (int i = 0; i < 4; i++){
		printf("csr %d: 0x%04x\n", i, (unsigned int) lance_reg_read(priv, i));
	}
}
#else
static void lance_dump_csrs(struct lance *priv)
{

}
#endif

static inline u32 lance_unpack_md(const struct lance_rmd *rmd)
{
	u32 md;

	md = rmd->addr[1];
	md <<= 16;
	md |= rmd->addr[0];

	return md;
}

#define lance_for_each_rmd(_idx, _rmds, __rmd, __md, __off) \
	for(_idx = 0, __rmd = &priv->rmds[__off], __md = lance_unpack_md(rmd);\
		_idx < ARRAY_SIZE(_rmds);\
		_idx++, __rmd = &priv->rmds[(_idx + __off) % ARRAY_SIZE(_rmds)], __md = lance_unpack_md(__rmd))

#ifdef DEBUG
static void lance_dump_flags(u32 md)
{
	printf("- %c%c%c%c%c%c%c%c",
			md & DESCRIPTOR_OWN  ? 'o' : ' ',
			md & DESCRIPTOR_ERR  ? 'e' : ' ',
			md & DESCRIPTOR_FRAM ? 'f' : ' ',
			md & DESCRIPTOR_OFLO ? 'O' : ' ',
			md & DESCRIPTOR_CRC  ? 'c' : ' ',
			md & DESCRIPTOR_BUFF ? 'b' : ' ',
			md & DESCRIPTOR_STP  ? 's' : ' ',
			md & DESCRIPTOR_ENP  ? 'E' : ' ');
}

static void lance_dump_rmds(struct lance *priv)
{
	int i;
	struct lance_rmd *rmd;
	u32 md;

	lance_for_each_rmd(i, priv->rmds, rmd, md, 0) {
		u32 addr = md & 0xffffff;

		printf("rmd %d:\t buff: 0x%08x, bcnt: 0x%04x, mcnt: 0x%04x ",
				i, addr,
				(unsigned int) rmd->bcnt,
				(unsigned int) rmd->mcnt);
		lance_dump_flags(md);
		printf(" - %c\n", priv->processedrmds[i] ? 'P' : ' ');
	}
}
#else
static void lance_dump_rmds(struct lance *priv){

}
#endif

static int lance_recv(struct udevice *dev, int flags, uchar **packetp)
{
	int i;
	struct lance *priv = dev_get_priv(dev);
	struct lance_rmd *rmd;
	u32 md;

	debug("%s\n", __func__);
	lance_dump_csrs(priv);
	lance_dump_rmds(priv);
	lance_reg_write(priv, 0, CSR0_MISS | CSR0_RINT);

	lance_for_each_rmd(i, priv->rmds, rmd, md, priv->lastrxpkt) {
		u32 addr = md & 0xffffff;

		if (!(md & DESCRIPTOR_OWN) && !priv->processedrmds[i]) {
			int ii = (priv->lastrxpkt + i) % ARRAY_SIZE(priv->rmds);
			debug("Consuming RX descriptor %d\n", ii);
			priv->lastrxpkt = ii;
			*packetp = (void *) addr;
			priv->processedrmds[ii] = true;
			return rmd->mcnt;
		}
	}

	return 0;
}

static void lance_set_rmd(struct lance_rmd *rmd, void *buf, u32 flags)
{
	unsigned int rxbuffaddr = (unsigned int) buf;
	u32 addr = (rxbuffaddr & 0xffffff) |
						 DESCRIPTOR_OWN;
	LANCE_STACK_U32(rmd->addr,addr);
	rmd->bcnt = LANCE_BCNT(LANCE_RXBUFFSZ);
	rmd->mcnt = 0;
}

static int lance_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	int i;
	struct lance *priv = dev_get_priv(dev);
	struct lance_rmd *rmd;
	u32 md;

	debug("%s\n", __func__);

	lance_for_each_rmd(i, priv->rmds, rmd, md, 0) {
		u32 addr = md & 0xffffff;

		if ((u32) packet == addr) {
			debug("Freeing RX descriptor %d\n", i);
			lance_set_rmd(rmd, priv->rxbuf + (LANCE_RXBUFFSZ * i), DESCRIPTOR_OWN);
			priv->processedrmds[i] = false;
			break;
		}
	}

	return 0;
}

static int lance_start(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct lance *priv = dev_get_priv(dev);
	unsigned char *enetaddr = pdata->enetaddr;
	u16 csr0;

	lance_reg_write(priv, 0, CSR0_STOP);
	mdelay(10);
	lance_reg_write(priv, 3, CSR3_BSWP);

	priv->rxbuf = malloc(LANCE_RMDS * LANCE_RXBUFFSZ);
	if (!priv->rxbuf)
		return -ENOMEM;

	debug("%s, initblock %p, rmds %p, tmds %p, rxbuffs %p\n",
			__func__, &priv->initblock, &priv->rmds, &priv->tmds, priv->rxbuf);

	/* init the rx buffers */
	for (int i = 0; i < LANCE_RMDS; i++) {
		lance_set_rmd(&priv->rmds[i], priv->rxbuf + (LANCE_RXBUFFSZ * i), DESCRIPTOR_OWN);
	}

	lance_dump_csrs(priv);

	for (int i = 0; i < ARRAY_SIZE(priv->initblock.padr); i++) {
		priv->initblock.padr[i] =
				enetaddr[(i * 2) + 1] << 8 | enetaddr[i * 2];
	}

	for (int i = 0; i < ARRAY_SIZE(priv->initblock.ladrf); i++) {
		priv->initblock.ladrf[i] = 0xffff;
	}

	unsigned int rmdsaddr = (unsigned int) &priv->rmds;
	u32 rdra = (4 << 29) | (rmdsaddr & 0xffffff);
	LANCE_STACK_U32(priv->initblock.rdra,rdra);

	unsigned int tmdsaddr = (unsigned int) &priv->tmds;
	u32 tdra = (0 << 29) | (tmdsaddr & 0xffffff);
	LANCE_STACK_U32(priv->initblock.tdra,tdra);

	unsigned int initblockaddr = (unsigned int) &priv->initblock;
	lance_reg_write(priv, 1, initblockaddr & 0xfffe);
	lance_reg_write(priv, 2, (initblockaddr >> 16) & 0xff);

	printf("Init LANCE..\n");
	lance_dump_initblock(priv);
	lance_reg_write(priv, 0, CSR0_INIT);
	do {
		csr0 = lance_reg_read(priv, 0);
	} while(!(csr0 & CSR0_IDON));
	printf("Done!\n");
	lance_dump_csrs(priv);

	printf("Starting LANCE..\n");
	csr0 = CSR0_STRT;
	lance_reg_write(priv, 0, csr0);
	printf("Done!\n");
	lance_dump_csrs(priv);

	mdelay(500);
	lance_dump_rmds(priv);

	return 0;
}

static void lance_stop(struct udevice *dev)
{
	struct lance *priv = dev_get_priv(dev);

	debug("%s\n", __func__);
	lance_reg_write(priv, 0, CSR0_STOP);
	lance_dump_csrs(priv);

	free(priv->rxbuf);
	priv->rxbuf = NULL;
}

#ifdef DEBUG
static void lance_dump_tmds(struct lance *priv)
{
	for(int i = 0; i < ARRAY_SIZE(priv->tmds); i++){
		u32 md;
		md = priv->tmds[i].addr[1];
		md <<= 16;
		md |= priv->tmds[i].addr[0];
		u32 addr = md & 0xffffff;

		printf("tmd %d:\t buff: 0x%08x, bcnt: 0x%04x, flags: 0x%04x ",
				i, addr,
				(unsigned int) priv->tmds[i].bcnt,
				(unsigned int) priv->tmds[i].flags);
		lance_dump_flags(md);
		printf("\n");
	}
}
#else
static void lance_dump_tmds(struct lance *priv)
{

}
#endif

static void lance_set_tmd(struct lance_tmd *tmd, void *packet, int len)
{
	unsigned int txbuffaddr = (unsigned int) packet;
	u32 addr;

	tmd->bcnt = LANCE_BCNT(-len);
	tmd->flags = 0;

	addr = (txbuffaddr & 0xffffff);
	addr |= DESCRIPTOR_OWN;
	addr |= DESCRIPTOR_STP;
	addr |= DESCRIPTOR_ENP;
	LANCE_STACK_U32(tmd->addr,addr);
}

static int lance_send(struct udevice *dev, void *packet, int len)
{
	struct lance *priv = dev_get_priv(dev);
	u16 csr0;

	debug("%s\n", __func__);
	lance_dump_tmds(priv);

	/* Clear the tint and transmitter errors */
	lance_reg_write(priv, 0, CSR0_TINT | CSR0_BABL | CSR0_CERR);
	lance_dump_csrs(priv);

	/* Setup the TMD */
	lance_set_tmd(&priv->tmds[0], packet, len);
	/* Kick LANCE */
	lance_reg_write(priv, 0, CSR0_TDMD);

	/* Wait for it to get sent */
	do {
		csr0 = lance_reg_read(priv, 0);
	} while (!(csr0 & CSR0_TINT));

	lance_dump_csrs(priv);
	lance_dump_tmds(priv);

	return 0;
}

static int lance_remove(struct udevice *dev)
{
	lance_stop(dev);

	return 0;
}

static const struct eth_ops lance_ops = {
	.start = lance_start,
	.stop = lance_stop,
	.send = lance_send,
	.recv = lance_recv,
	.free_pkt = lance_free_pkt,
};

static int lance_of_to_plat(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct lance *priv = dev_get_priv(dev);

	pdata->iobase = dev_read_addr(dev);

	priv->dev = dev;

	priv->base = (void *) dev_read_addr(dev);

	return 0;
}

static const struct udevice_id lance_ids[] = {
	{ .compatible = "amd,am7990" },
	{}
};

U_BOOT_DRIVER(lance) = {
	.name = "lance",
	.id = UCLASS_ETH,
	.of_match = lance_ids,
	.of_to_plat = lance_of_to_plat,
	.plat_auto = sizeof(struct eth_pdata),
	.remove = lance_remove,
	.ops = &lance_ops,
	.priv_auto = sizeof(struct lance),
};
