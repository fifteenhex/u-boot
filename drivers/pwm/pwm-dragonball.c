// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <clk.h>
#include <div64.h>
#include <dm.h>
#include <pwm.h>
#include <asm/global_data.h>
#include <linux/io.h>
#include <linux/log2.h>
#include <linux/bitfield.h>

#define REG_PWMC	0x0
#define REG_PWMS	0x2
#define REG_PWMP	0x4
#define REG_PWMCNT	0x5

struct pwm_dragonball_priv {
	void __iomem *base;
};

static int pwm_dragonball_set_config(struct udevice *dev, uint channel,
				 uint period_ns, uint duty_ns)
{
	struct pwm_dragonball_priv *priv = dev_get_priv(dev);

	return 0;
}

static int pwm_dragonball_set_enable(struct udevice *dev, uint channel, bool enable)
{
	struct pwm_dragonball_priv *priv = dev_get_priv(dev);

	return 0;
}

static int pwm_dragonball_of_to_plat(struct udevice *dev)
{
	struct pwm_dragonball_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr_ptr(dev);

	return 0;
}

static int pwm_dragonball_probe(struct udevice *dev)
{
	struct pwm_dragonball_priv *priv = dev_get_priv(dev);
	struct clk clk;
	int ret = 0;

	return 0;
}

static const struct pwm_ops pwm_dragonball_ops = {
	.set_config	= pwm_dragonball_set_config,
	.set_enable	= pwm_dragonball_set_enable,
};

static const struct udevice_id pwm_dragonball_ids[] = {
	{ .compatible = "motorola,mc68ez328-pwm" },
	{ }
};

U_BOOT_DRIVER(pwm_dragonball) = {
	.name	= "pwm_dragonball",
	.id	= UCLASS_PWM,
	.of_match = pwm_dragonball_ids,
	.ops	= &pwm_dragonball_ops,
	.of_to_plat     = pwm_dragonball_of_to_plat,
	.probe		= pwm_dragonball_probe,
	.priv_auto	= sizeof(struct pwm_dragonball_priv),
};
