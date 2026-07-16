// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver-model timer for classic 68k Macintosh machines: VIA1 6522 Timer 1,
 * free-running at the classic-Mac VIA clock (783360 Hz).  The 6522 registers are
 * byte-wide and spaced 0x200 apart; Timer 1 counts down from a 0xFFFF latch and
 * auto-reloads, so we read the 16-bit counter, derive an up-counter, and extend
 * it to 64 bits in software (each reload makes the up-counter step backwards).
 */

#include <dm.h>
#include <timer.h>
#include <asm/io.h>
#include <linux/errno.h>

#define VIA_TIMER_HZ	783360
#define VIA_RS		0x200
#define VIA_T1CL	4	/* Timer 1 counter low */
#define VIA_T1CH	5	/* Timer 1 counter high */
#define VIA_T1LL	6	/* Timer 1 latch low */
#define VIA_T1LH	7	/* Timer 1 latch high */
#define VIA_ACR		11	/* auxiliary control register */
#define VIA_IER		14	/* interrupt enable register */
#define ACR_T1_CONT	0x40	/* T1 continuous (free-running) mode */
#define ACR_T1_PB7	0x80	/* T1 drives PB7 (unwanted) */

/* board-provided: VIA1 base for the detected model */
ulong oldmac_via_base(void);

struct via_timer_priv {
	void	*base;
	u16	last_up;
	u64	high;
};

static inline u8 via_rd(struct via_timer_priv *p, unsigned int reg)
{
	return readb(p->base + reg * VIA_RS);
}

static inline void via_wr(struct via_timer_priv *p, unsigned int reg, u8 val)
{
	writeb(val, p->base + reg * VIA_RS);
}

/* Glitch-free read of the 16-bit down-counter. */
static u16 via_read_t1(struct via_timer_priv *p)
{
	u8 hi1, lo, hi2;

	do {
		hi1 = via_rd(p, VIA_T1CH);
		lo  = via_rd(p, VIA_T1CL);
		hi2 = via_rd(p, VIA_T1CH);
	} while (hi1 != hi2);

	return ((u16)hi2 << 8) | lo;
}

static u64 via_timer_get_count(struct udevice *dev)
{
	struct via_timer_priv *p = dev_get_priv(dev);
	u16 up = 0xffff - via_read_t1(p);	/* T1 counts down; derive up-count */

	if (up < p->last_up)			/* reloaded -> one period elapsed */
		p->high += 0x10000;
	p->last_up = up;

	return p->high + up;
}

static int via_timer_probe(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct via_timer_priv *p = dev_get_priv(dev);

	p->base = (void *)oldmac_via_base();
	if (!p->base)
		return -ENODEV;

	uc_priv->clock_rate = VIA_TIMER_HZ;

	/* free-running, no PB7 toggle; poll (no interrupt); latch 0xFFFF; start */
	via_wr(p, VIA_ACR, (via_rd(p, VIA_ACR) & ~ACR_T1_PB7) | ACR_T1_CONT);
	via_wr(p, VIA_IER, 0x40);		/* bit7 clear -> disable T1 IRQ */
	via_wr(p, VIA_T1LL, 0xff);
	via_wr(p, VIA_T1LH, 0xff);
	via_wr(p, VIA_T1CH, 0xff);		/* load counter from latch and start */
	p->last_up = 0;
	p->high = 0;

	return 0;
}

static const struct timer_ops via_timer_ops = {
	.get_count	= via_timer_get_count,
};

U_BOOT_DRIVER(oldmac_via_timer) = {
	.name		= "oldmac_via_timer",
	.id		= UCLASS_TIMER,
	.probe		= via_timer_probe,
	.ops		= &via_timer_ops,
	.priv_auto	= sizeof(struct via_timer_priv),
	.flags		= DM_FLAG_PRE_RELOC,
};

U_BOOT_DRVINFO(oldmac_via_timer) = {
	.name = "oldmac_via_timer",
};
