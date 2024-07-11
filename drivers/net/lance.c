// SPDX-License-Identifier: GPL-2.0-or-later
/*
 */
//#define DEBUG
//#define DEBUG_VERBOSE

#include <dm.h>
#include <dm/device_compat.h>
#include <net.h>
#include <linux/delay.h>
#include <asm/io.h>

static void lance_dump_flags(u32 md);

#define LANCE_TMDS	1
#define LANCE_RMDS	16

#define LANCE_RXBUFFSZ 1518

#define REG_CSR0	0
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

#define REG_CSR3	3
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

#define MODE_PROM		BIT(15)

#ifdef CONFIG_LANCE_LANCE
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
#else
struct lance_tmd;
#endif

#ifdef CONFIG_LANCE_ILACC
#define REG_CSR4			4
#define CSR4_BACON_86X86	(0b00 << 6)
#define CSR4_BACON_680X0	(0b01 << 6)

struct lance_32_initblock {
	u32 mode;
	u32 padr[2];
	u16 ladrf[4];
	u32 rdra;
	u32 tdra;
	u32 _pad;
};

struct lance_32_rmd {
	u32 addr;
	u32 bcnt;
	u32 mcnt;
	u32 _pad;
};

struct lance_32_tmd {
	u32 addr;
	u32 bcnt;
	u32 flags;
	u32 _pad;
};
#else
struct lance_32_tmd;
#endif

struct lance_data {
	bool thirtytwobit;
};

struct lance_dma_data {
	/*
	 * Structures that are feed into
	 * the chip, alignment etc matters.
	 */
	union {
#ifdef CONFIG_LANCE_LANCE
		struct lance_initblock initblock_24;
#endif

#if CONFIG_LANCE_ILACC
		struct lance_32_initblock initblock_32;
#endif
	} initblock __attribute__((aligned(4)));

	union {
#ifdef CONFIG_LANCE_LANCE
		struct lance_rmd rmds_24[LANCE_RMDS];
#endif

#ifdef CONFIG_LANCE_ILACC
		struct lance_32_rmd rmds_32[LANCE_RMDS];
#endif
	} rmds __attribute__((aligned(16)));;

	union {
#ifdef CONFIG_LANCE_LANCE
		struct lance_tmd tmds_24[LANCE_TMDS];
#endif

#ifdef CONFIG_LANCE_ILACC
		struct lance_32_tmd tmds_32[LANCE_TMDS];
#endif
	} tmds __attribute__((aligned(16)));
};

struct lance {
	/* Driver stuff */
	struct udevice *dev;
	void __iomem *base;
	const struct lance_data *data;
	u32 rdp_off, rap_off;

	struct lance_dma_data* dma_data;

	/* RX tracking */
	void *rxbuf;
	bool processedrmds[LANCE_RMDS];
	unsigned lastrxpkt;
};

#if defined(CONFIG_LANCE_ILACC) && !defined(CONFIG_LANCE_LANCE)
#define lance_isthirytwobit(_priv) true
#elif !defined(CONFIG_LANCE_ILACC) && defined(CONFIG_LANCE_LANCE)
#define lance_isthirytwobit(_priv) false
#else
#define lance_isthirytwobit(_priv) (_priv->data->thirtytwobit)
#endif

static u16 lance_reg_read(struct lance *priv, unsigned int which)
{
	writew(which, priv->base + priv->rap_off);
	return readw(priv->base + priv->rdp_off);
}

static void lance_reg_write(struct lance *priv, unsigned int which, u16 value)
{
#ifdef DEBUG
	printf("write: %d = 0x%04x\n", which, (unsigned int) value);
#endif
	writew(which, priv->base + priv->rap_off);
	writew(value, priv->base + priv->rdp_off);
}

static void lance_dump_initblock(struct lance *priv)
{
	u32 mode, rdra, tdra;

	if (lance_isthirytwobit(priv)) {
#ifdef CONFIG_LANCE_ILACC
		struct lance_32_initblock *initblock = &priv->dma_data->initblock.initblock_32;

		mode = initblock->mode;
		rdra = initblock->rdra;
		tdra = initblock->tdra;
#endif
	} else {
#ifdef CONFIG_LANCE_LANCE
		struct lance_initblock *initblock = &priv->dma_data->initblock.initblock_24;

		mode = initblock->mode;
		rdra = initblock->rdra[1];
		rdra <<= 16;
		rdra |= initblock->rdra[0];

		tdra = initblock->tdra[1];
		tdra <<= 16;
		tdra |= initblock->tdra[0];
#endif
	}

	printf("mode: 0x%04x, rdra: 0x%08x, tdra: 0x%08x\n",
			(unsigned int) mode, rdra, tdra);
}

#if defined(DEBUG) && defined(DEBUG_VERBOSE)
static void lance_dump_csrs(struct lance *priv)
{
	const int num = lance_isthirytwobit(priv) ? 58 : 4;

	for (int i = 0; i < num; i++){
		printf("csr %d: 0x%04x\n", i, (unsigned int) lance_reg_read(priv, i));
	}
}
#else
#define lance_dump_csrs(p)
#endif

#ifdef CONFIG_LANCE_LANCE
const struct lance_data lance_am7990_data = {
	.thirtytwobit = false,
};

#define lance_24_initblock(_priv) &priv->dma_data->initblock.initblock_24
#define lance_24_nrmd(_priv) (ARRAY_SIZE(_priv->dma_data->rmds.rmds_24))
#define lance_24_rmd(_priv, _which) &_priv->dma_data->rmds.rmds_24[_which]
#define lance_24_ntmd(_priv) (ARRAY_SIZE(_priv->dma_data->tmds.tmds_24))
#define lance_24_tmd(_priv, _which) &_priv->dma_data->tmds.tmds_24[_which]

static inline u32 lance_unpack_md(const struct lance_rmd *rmd)
{
	u32 md;

	md = rmd->addr[1];
	md <<= 16;
	md |= rmd->addr[0];

	return md;
}

#define lance_for_each_rmd(_idx, _rmds, __rmd, __md, __off) \
	for(_idx = 0, __rmd = lance_24_rmd(priv, __off), __md = lance_unpack_md(rmd);\
		_idx < lance_24_nrmd(priv);\
		_idx++, __rmd = lance_24_rmd(priv,(_idx + __off) % lance_24_nrmd(priv)), __md = lance_unpack_md(__rmd))

#define LANCE_STACK_U32(_x,_v)	\
	_x[0] = _v & 0xffff;		\
	_x[1] = (_v >> 16) & 0xffff

static void lance_set_rmd(struct lance_rmd *rmd, void *buf, u32 flags)
{
	unsigned int rxbuffaddr = (unsigned int) buf;
	u32 addr = (rxbuffaddr & 0xffffff) |
						 DESCRIPTOR_OWN;
	LANCE_STACK_U32(rmd->addr,addr);
	rmd->bcnt = LANCE_RXBUFFSZ;
	rmd->mcnt = 0;
}

static void lance_24_set_tmd(struct lance_tmd *tmd, void *packet, int len, u32 flags)
{
	unsigned int txbuffaddr = (unsigned int) packet;
	u32 addr;

	tmd->bcnt = -len;
	tmd->flags = 0;

	addr = (txbuffaddr & 0xffffff);
	addr |= flags;
	LANCE_STACK_U32(tmd->addr,addr);
}

static void lance_24_data_init(struct lance *priv, const unsigned char *enetaddr)
{
	struct lance_initblock *initblock = lance_24_initblock(priv);

	/* init the rx buffers */
	for (int i = 0; i < LANCE_RMDS; i++) {
		struct lance_rmd *rmd = lance_24_rmd(priv, i);
		lance_set_rmd(rmd, priv->rxbuf + (LANCE_RXBUFFSZ * i), DESCRIPTOR_OWN);
	}

	memset(initblock, 0, sizeof(*initblock));

	for (int i = 0; i < ARRAY_SIZE(initblock->padr); i++) {
		initblock->padr[i] =
				enetaddr[(i * 2) + 1] << 8 | enetaddr[i * 2];
	}

	for (int i = 0; i < ARRAY_SIZE(initblock->ladrf); i++) {
		initblock->ladrf[i] = 0xffff;
	}

	unsigned int rmdsaddr = (unsigned int) lance_24_rmd(priv, 0);
	u32 rdra = (4 << 29) | (rmdsaddr & 0xffffff);
	LANCE_STACK_U32(initblock->rdra,rdra);

	unsigned int tmdsaddr = (unsigned int) lance_24_tmd(priv, 0);
	u32 tdra = (0 << 29) | (tmdsaddr & 0xffffff);
	LANCE_STACK_U32(initblock->tdra,tdra);
}

#ifdef DEBUG
static void lance_24_dump_rmds(struct lance *priv)
{
	int i;
	struct lance_rmd *rmd;
	u32 md;

	lance_for_each_rmd(i, priv->rmds, rmd, md, 0) {
		u32 addr = md & 0xffffff;

		printf("rmd %d (%p):\t buff: 0x%08x, bcnt: 0x%04x, mcnt: 0x%04x ",
				i, rmd, addr,
				(unsigned int) rmd->bcnt,
				(unsigned int) rmd->mcnt);
		lance_dump_flags(md);
		printf(" - %c\n", priv->processedrmds[i] ? 'P' : ' ');
	}
}

static void lance_24_dump_tmds(struct lance *priv)
{
	for(int i = 0; i < lance_24_ntmd(priv); i++){
		const struct lance_tmd *tmd = lance_24_tmd(priv, i);
		u32 md;
		md = tmd->addr[1];
		md <<= 16;
		md |= tmd->addr[0];
		u32 addr = md & 0xffffff;

		printf("tmd %d (%p):\t buff: 0x%08x, bcnt: 0x%04x, flags: 0x%04x ",
				i, tmd, addr,
				(unsigned int) tmd->bcnt,
				(unsigned int) tmd->flags);
		lance_dump_flags(md);
		printf("\n");
	}
}
#endif

static void lance_24_prestart_config(struct lance *priv)
{
	lance_reg_write(priv, 3, CSR3_BSWP);
}

static int lance_24_check_rmds(struct lance *priv, uchar **packetp)
{
	struct lance_rmd *rmd;
	u32 md;
	int i;

	lance_for_each_rmd(i, priv->rmds, rmd, md, priv->lastrxpkt) {
		u32 addr = md & 0xffffff;

		if (!(md & DESCRIPTOR_OWN) && !priv->processedrmds[i]) {
			int ii = (priv->lastrxpkt + i) % lance_24_nrmd(priv);
			debug("Consuming RX descriptor %d\n", ii);
			priv->lastrxpkt = ii;
			*packetp = (void *) addr;
			priv->processedrmds[ii] = true;
			return rmd->mcnt;
		}
	}

	return -EAGAIN;
}

static void lance_24_free_rmd(struct lance *priv, void *packet)
{
	struct lance_rmd *rmd;
	u32 md;
	int i;

	lance_for_each_rmd(i, priv->rmds, rmd, md, 0) {
		u32 addr = md & 0xffffff;

		if ((u32) packet == addr) {
			debug("Freeing RX descriptor %d\n", i);
			lance_set_rmd(rmd, priv->rxbuf + (LANCE_RXBUFFSZ * i), DESCRIPTOR_OWN);
			priv->processedrmds[i] = false;
			break;
		}
	}
}
#else
#define lance_24_data_init(p, ea)
#define lance_24_dump_rmds(p)
#define lance_24_dump_tmds(p)
#define lance_24_prestart_config(p)
#define lance_24_check_rmds(p, pp) 0
#define lance_24_free_rmd(p, pkt)
#define lance_24_tmd(p, w) NULL
#define lance_24_set_tmd(t, p, l, f)
#endif

#ifdef CONFIG_LANCE_ILACC
const struct lance_data lance_am79900_data = {
	.thirtytwobit = true,
};

#define lance_32_initblock(_priv) &priv->dma_data->initblock.initblock_32
#define lance_32_ntmd(_priv) (ARRAY_SIZE(_priv->dma_data->tmds.tmds_32))
#define lance_32_rmd(_priv, _which) &_priv->dma_data->rmds.rmds_32[_which]
#define lance_32_nrmd(_priv) (ARRAY_SIZE(_priv->dma_data->rmds.rmds_32))
#define lance_32_tmd(_priv, _which) &_priv->dma_data->tmds.tmds_32[_which]
#define RLEN_SHIFT 20

static void lance_32_set_rmd(struct lance_32_rmd *rmd, void *buf, u32 flags)
{
	rmd->addr = (u32) buf;
	rmd->bcnt = flags;
	rmd->mcnt = 0;
	rmd->_pad = 0;

	/* Do this last */
	rmd->bcnt |= (0xf << 12) | (-LANCE_RXBUFFSZ & 0xfff);
}

static void lance_32_set_tmd(struct lance_32_tmd *tmd, void *packet, int len, u32 flags)
{
	tmd->addr = (u32) packet;
	tmd->flags = 0;
	tmd->_pad = 0;

	/* Do this last */
	tmd->bcnt = flags | (0xf << 12) | (-len & 0xfff) ;
}

static void lance_32_data_init(struct lance *priv, const unsigned char *enetaddr)
{
	struct lance_32_initblock *initblock = lance_32_initblock(priv);

	/* init the rx buffers */
	for (int i = 0; i < lance_32_nrmd(priv); i++) {
		struct lance_32_rmd *rmd = lance_32_rmd(priv, i);
		void *buff = priv->rxbuf + (LANCE_RXBUFFSZ * i);
		lance_32_set_rmd(rmd, buff, DESCRIPTOR_OWN);
	}

	for (int i = 0; i < lance_32_ntmd(priv); i++){
		struct lance_32_tmd *tmd = lance_32_tmd(priv, i);
		lance_32_set_tmd(tmd, NULL, 0, (DESCRIPTOR_STP | DESCRIPTOR_ENP));
	}

	memset(initblock, 0, sizeof(*initblock));

	// MMMM! why this no work?
	initblock->padr[0] = enetaddr[0] << 8 | enetaddr[1];
	initblock->padr[1] = enetaddr[2] << 24 | enetaddr[3] << 16 | enetaddr[4] << 8 | enetaddr[5];

	for (int i = 0; i < ARRAY_SIZE(initblock->ladrf); i++) {
		initblock->ladrf[i] = 0xffff;
	}

	initblock->mode = (0x4 << RLEN_SHIFT) | MODE_PROM;
	initblock->rdra =  (u32) lance_32_rmd(priv, 0);
	initblock->tdra =  (u32) lance_32_tmd(priv, 0);
}

static void lance_32_dump_rmds(struct lance *priv)
{
	printf("-- rmds --\n");
	for (int i = 0; i < lance_32_nrmd(priv); i++)
	{
		struct lance_32_rmd *rmd = lance_32_rmd(priv, i);

		printf("rmd %d (%p):\t buff: 0x%08x, bcnt: 0x%08x, mcnt: 0x%08x\n",
				i,
				rmd,
				rmd->addr,
				(unsigned int) rmd->bcnt,
				(unsigned int) rmd->mcnt);
	}
}

static void lance_32_dump_tmds(struct lance *priv)
{
	printf("-- tmds --\n");
	for (int i = 0; i < lance_32_ntmd(priv); i++)
	{
		const struct lance_32_tmd *tmd = lance_32_tmd(priv, i);

		printf("tmd %d (%p):\t buff: 0x%08x, bnct: 0x%08x, 0x%08x\n",
				i,
				tmd,
				tmd->addr,
				tmd->bcnt,
				tmd->flags);
	}
}

static void lance_32_prestart_config(struct lance *priv)
{
	lance_reg_write(priv, REG_CSR3, CSR3_ACON);
	lance_reg_write(priv, REG_CSR4, CSR4_BACON_680X0);
}

static int lance_32_check_rmds(struct lance *priv, uchar **packetp)
{
	for (int i = 0; i < lance_32_nrmd(priv); i++) {
		const int ii = (priv->lastrxpkt + i) % lance_32_nrmd(priv);
		const struct lance_32_rmd *rmd = lance_32_rmd(priv, ii);
		if (!(rmd->bcnt & DESCRIPTOR_OWN) && !priv->processedrmds[i]) {
			debug("Consuming RX descriptor %d\n", ii);
			priv->lastrxpkt = ii;
			*packetp = (void *) rmd->addr;
			priv->processedrmds[ii] = true;
			return (rmd->mcnt & 0xfff);
		}
	}

	return -EAGAIN;
}

static void lance_32_free_rmd(struct lance *priv, void *packet)
{
	for (int i = 0; i < lance_32_nrmd(priv); i++) {
		const struct lance_32_rmd *rmd = lance_32_rmd(priv, i);

		if ((u32) packet == (u32) rmd->addr) {
			debug("Freeing RX descriptor %d\n", i);
			lance_32_set_rmd(rmd, rmd->addr, DESCRIPTOR_OWN);
			priv->processedrmds[i] = false;
			break;
		}
	}
}
#else
#define lance_32_data_init(p, ea)
#define lance_32_dump_rmds(p)
#define lance_32_dump_tmds(p)
#define lance_32_prestart_config(p)
#define lance_32_check_rmds(p, pp) 0
#define lance_32_free_rmd(p, pkt)
#define lance_32_tmd(p, w) NULL
#define lance_32_set_tmd(t, p, l, f)
#endif

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
	if (lance_isthirytwobit(priv))
		lance_32_dump_rmds(priv);
	else
		lance_24_dump_rmds(priv);
}

static void lance_dump_tmds(struct lance *priv)
{
	if (lance_isthirytwobit(priv))
		lance_32_dump_tmds(priv);
	else
		lance_24_dump_tmds(priv);
}
#else
#define lance_dump_rmds(p)
#define lance_dump_tmds(p)
#endif

static int lance_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct lance *priv = dev_get_priv(dev);

	debug("%s\n", __func__);
	lance_dump_csrs(priv);
	lance_dump_rmds(priv);
	lance_reg_write(priv, 0, CSR0_MISS | CSR0_RINT);

	if (lance_isthirytwobit(priv))
		return lance_32_check_rmds(priv, packetp);
	else {
		return lance_24_check_rmds(priv, packetp);
	}

	return -EINVAL;
}

static int lance_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	struct lance *priv = dev_get_priv(dev);

	debug("%s\n", __func__);

	if (lance_isthirytwobit(priv))
		lance_32_free_rmd(priv, packet);
	else
		lance_24_free_rmd(priv, packet);

	return 0;
}

static u32 lance_dma_mask(struct lance *priv, u32 val)
{
	if (lance_isthirytwobit(priv))
		return val;
	else
		return val & 0xfffffe;
}

static int lance_start(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct lance *priv = dev_get_priv(dev);
	unsigned char *enetaddr = pdata->enetaddr;
	bool thirtytwobit = lance_isthirytwobit(priv);
	ulong now;
	u16 csr0;
	int ret = 0;

	lance_dump_csrs(priv);
	lance_reg_write(priv, 0, CSR0_STOP);
	mdelay(10);

	/* Allocate the RMD/TMD buffers */
	priv->dma_data = memalign(16, sizeof(*priv->dma_data));
	if (!priv->dma_data)
		return -ENOMEM;

	priv->rxbuf = calloc(LANCE_RXBUFFSZ, LANCE_RMDS);
	if (!priv->rxbuf)
		return -ENOMEM;

	/* Reset the processed RMDs state */
	memset(priv->processedrmds, 0, sizeof(priv->processedrmds));
	priv->lastrxpkt = 0;

	/* Do the configuration of the bus settings etc */
	if (thirtytwobit)
		lance_32_prestart_config(priv);
	else
		lance_24_prestart_config(priv);

	debug("%s, initblock %p, rmds %p, tmds %p, rxbuffs %p\n",
			__func__,
			&priv->dma_data->initblock,
			&priv->dma_data->rmds,
			&priv->dma_data->tmds,
			priv->rxbuf);

	if (thirtytwobit) {
		lance_32_data_init(priv, enetaddr);
	} else
		lance_24_data_init(priv, enetaddr);

	lance_dump_csrs(priv);
	lance_dump_rmds(priv);
	lance_dump_tmds(priv);

	u32 initblockaddr = lance_dma_mask(priv, (u32) &priv->dma_data->initblock);
	lance_reg_write(priv, 1, initblockaddr & 0xfffe);
	lance_reg_write(priv, 2, (initblockaddr >> 16) & 0xffff);
	lance_dump_csrs(priv);

	printf("Init LANCE..\n");
	lance_dump_initblock(priv);
	lance_reg_write(priv, REG_CSR0, CSR0_INIT);
	now = get_timer(0);
	do {
		if (get_timer(now) > 10000) {
			ret = -ETIMEDOUT;
			goto err;
		}

		csr0 = lance_reg_read(priv, 0);

		if (csr0 & CSR0_ERR) {
			ret = -EINVAL;
			goto err;
		}
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

err:
	lance_reg_write(priv, 0, CSR0_STOP);
	printf("init error, csr0: 0x%02x\n", (unsigned int) csr0);
	lance_dump_csrs(priv);
	return ret;
}

static void lance_stop(struct udevice *dev)
{
	struct lance *priv = dev_get_priv(dev);

	debug("%s\n", __func__);
	lance_reg_write(priv, 0, CSR0_STOP);
	lance_dump_csrs(priv);

	free(priv->rxbuf);
	priv->dma_data = NULL;

	free(priv->dma_data);
	priv->rxbuf = NULL;
}

static int lance_send(struct udevice *dev, void *packet, int len)
{
	struct lance *priv = dev_get_priv(dev);
	ulong now;
	u16 csr0;
	/* packets smaller than 60 bytes don't transmit correctly? */
	len = max(60, len);

	debug("%s:%d - len %d\n", __func__, __LINE__, len);
	lance_dump_tmds(priv);

	/* Clear the tint and transmitter errors */
	lance_reg_write(priv, REG_CSR0, CSR0_TINT | CSR0_BABL | CSR0_CERR);
	lance_dump_csrs(priv);

	/* Setup the TMD */
	const u32 tmdflags = (DESCRIPTOR_OWN | DESCRIPTOR_STP | DESCRIPTOR_ENP);
	if (lance_isthirytwobit(priv)) {
		struct lance_32_tmd *tmd = lance_32_tmd(priv, 0);
		lance_32_set_tmd(tmd, packet, len, tmdflags);
	}
	else {
		struct lance_tmd *tmd = lance_24_tmd(priv, 0);
		lance_24_set_tmd(tmd, packet, len, tmdflags);
	}

	lance_dump_tmds(priv);
	/* Kick LANCE */
	lance_reg_write(priv, REG_CSR0, CSR0_TDMD);

	/* Wait for it to get sent */
	now = get_timer(0);
	do {
		csr0 = lance_reg_read(priv, 0);

		if (get_timer(now) > 10000) {
			printf("send timed out\n");
			return -ETIMEDOUT;
		}
	} while (!(csr0 & CSR0_TINT));

	lance_dump_csrs(priv);
	lance_dump_tmds(priv);

	return len;
}

static int lance_remove(struct udevice *dev)
{
	lance_stop(dev);

	return 0;
}

static const struct eth_ops lance_ops = {
	.start    = lance_start,
	.stop     = lance_stop,
	.send     = lance_send,
	.recv     = lance_recv,
	.free_pkt = lance_free_pkt,
};

static int lance_of_to_plat(struct udevice *dev)
{
	const struct lance_data *ddata = (const struct lance_data *) dev_get_driver_data(dev);
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct lance *priv = dev_get_priv(dev);
	int ret;

	pdata->iobase = dev_read_addr(dev);

	priv->dev = dev;
	priv->base = (void *) dev_read_addr(dev);
	priv->data = ddata;

	ret = dev_read_u32(dev, "rdp-offset", &priv->rdp_off);
	if (ret)
		return ret;

	ret = dev_read_u32(dev, "rap-offset", &priv->rap_off);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id lance_ids[] = {
#ifdef CONFIG_LANCE_LANCE
	{
		.compatible = "amd,am7990",
		.data = (ulong) &lance_am7990_data,
	},
#endif
#ifdef CONFIG_LANCE_ILACC
	{
		.compatible = "amd,am79900",
		.data = (ulong) &lance_am79900_data,
	},
#endif
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
