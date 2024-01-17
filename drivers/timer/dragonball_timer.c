// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <asm/io.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdt_support.h>
#include <timer.h>

#define REG_TCTL					0x0
#define REG_TCTL_TEN				BIT(0)
#define REG_TCTL_IRQEN				BIT(4)
#define REG_TCTL_FRR				BIT(8)
#define REG_TCTL_CLKSOURCE_SHIFT	1
#define REG_TCTL_CLKSOURCE_MASK		0x7
#define REG_TPRER	0x2
#define REG_TCMP	0x4
#define REG_TCR		0x6
#define REG_TCN		0x8
#define REG_TSTAT	0xa

#define CLKSOURCE_SYSCLK		0x1
#define CLKSOURCE_SYSCLKDIV16	0x2
#define CLKSOURCE_TIN			0x3
#define CLKSOURCE_32K			0x4

#define INTC_TIMERIRQ	1

struct dragonball_timer_priv {
	void *base;
	u16 last;
	u32 offset;
#ifndef CONFIG_SPL_BUILD
	struct clk osc, sysclk;
#endif
};

static u64 dragonball_timer_get_count(struct udevice *dev)
{
	struct dragonball_timer_priv *priv = dev_get_priv(dev);
	u16 cnt;

	/*
	 * The counter is only 16 bits wide, make it look like
	 * it's 32 bits.
	 *
	 * If the current count is lower than the last then the
	 * counter has wrapped around at least once.
	 */
	cnt = readw(priv->base + REG_TCN);
	if (cnt < priv->last)
		priv->offset += (1 << 16);
	priv->last = cnt;

	return priv->offset + cnt;
}

static int dragonball_timer_probe(struct udevice *dev)
{
	struct dragonball_timer_priv *priv = dev_get_priv(dev);
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	unsigned int prescaler = 128;
	int ret;

	priv->base = dev_read_addr_ptr(dev);

#ifdef CONFIG_SPL_BUILD
#else
	ret = clk_get_by_name(dev, "osc", &priv->osc);
//	if (ret)
//		return ret;

	ret = clk_get_by_name_optional(dev, "sysclk", &priv->sysclk);
//	if (ret)
//		return ret;

	ret = clk_prepare_enable(&priv->osc);
//	if (ret)
//		return ret;

//	uc_priv->clock_rate = clk_get_rate(&priv->osc);
#endif

	priv->last = 0;
	priv->offset = 0;
	writew(prescaler - 1, priv->base + REG_TPRER);
	uc_priv->clock_rate = 32768 / prescaler;

	writew((CLKSOURCE_32K << REG_TCTL_CLKSOURCE_SHIFT)
			| REG_TCTL_TEN | REG_TCTL_FRR,
			priv->base + REG_TCTL);

	return 0;
}

extern int dragonball_intc_unmask(struct udevice *dev, unsigned int which);
extern int dragonball_intc_mask(struct udevice *dev, unsigned int which);

void dragonball_timer_arm_timer_wakeup(struct udevice *timerdev, struct udevice *intcdev)
{
	struct dragonball_timer_priv *priv = dev_get_priv(timerdev);
	u16 tcn;
	u16 tctl;

	/* Set TCMP to about 60ms in the future */
	tcn = readw(priv->base + REG_TCN);
	writew(tcn + 0xf, priv->base + REG_TCMP);

	dragonball_intc_unmask(intcdev, INTC_TIMERIRQ);
	tctl = readw(priv->base + REG_TCTL);
	tctl |= REG_TCTL_IRQEN;
	writew(tctl, priv->base + REG_TCTL);
}

void dragonball_timer_disarm_timer_wakeup(struct udevice *timerdev, struct udevice *intcdev)
{
	struct dragonball_timer_priv *priv = dev_get_priv(timerdev);
	u16 tctl, tstat;

	/* Mask the interrupt */
	tctl = readw(priv->base + REG_TCTL);
	tctl &= ~REG_TCTL_IRQEN;
	writew(tctl, priv->base + REG_TCTL);

	/* Clear the interrupt, must read the register first */
	tstat = readw(priv->base + REG_TSTAT);
	writew(0, priv->base + REG_TSTAT);

	/* Tell intc to mask it too */
	dragonball_intc_mask(intcdev, INTC_TIMERIRQ);
}

static const struct timer_ops dragonball_timer_ops = {
	.get_count = dragonball_timer_get_count,
};

static const struct udevice_id dragonball_timer_ids[] = {
	{ .compatible = "motorola,mc68ez328-timer", },
	{ }
};

U_BOOT_DRIVER(dragonball_timer) = {
	.name = "dragonball_timer",
	.id = UCLASS_TIMER,
	.of_match = of_match_ptr(dragonball_timer_ids),
	.probe = dragonball_timer_probe,
	.ops = &dragonball_timer_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct dragonball_timer_priv),
};
