// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <asm/io.h>
#include <clk-uclass.h>
#include <common.h>
#include <div64.h>
#include <dm.h>
#include <errno.h>
#include <fdt_support.h>
#include <timer.h>

#include <z8536.h>

#define Z8536_PORT_CONTROL	0x3

struct z8536_timer_priv {
	void *base;
#ifndef CONFIG_SPL_BUILD
	struct clk osc, sysclk;
#endif
};

static int hmmm = 1;

#define z8536_timer_delay() do { __asm__ __volatile__ (" nop; nop"); } while (0)

static u8 z8536_timer_read_reg(struct z8536_timer_priv *priv, u8 which)
{
	writeb(which, priv->base + Z8536_PORT_CONTROL);
	z8536_timer_delay();
	return readb(priv->base + Z8536_PORT_CONTROL);
}

static void z8536_timer_write_reg(struct z8536_timer_priv *priv, u8 which, u8 val)
{
	writeb(which, priv->base + Z8536_PORT_CONTROL);
	z8536_timer_delay();
	writeb(val, priv->base + Z8536_PORT_CONTROL);

if(hmmm)
	printf("wrote 0x%02x to 0x%02x, read back 0x%02x\n",
			(unsigned int) val,
			(unsigned int) which,
			(unsigned int) z8536_timer_read_reg(priv, which));
}

static u64 z8536_timer_get_count(struct udevice *dev)
{
	struct z8536_timer_priv *priv = dev_get_priv(dev);

	u8 lsb1, msb1, lsb2, msb2, val;
	u32 count;

	val = Z8536_CT_CMD_RCC | Z8536_CT_CMDSTAT_GCB;
	z8536_timer_write_reg(priv, Z8536_CT1_CMDSTAT_REG, val);
	z8536_timer_write_reg(priv, Z8536_CT2_CMDSTAT_REG, val);

	msb1 = z8536_timer_read_reg(priv, Z8536_CT1_VAL_MSB_REG);
	lsb1 = z8536_timer_read_reg(priv,Z8536_CT1_VAL_LSB_REG);

	msb2 = z8536_timer_read_reg(priv, Z8536_CT2_VAL_MSB_REG);
	lsb2 = z8536_timer_read_reg(priv,Z8536_CT2_VAL_LSB_REG);

	count = ~((msb2 << 24) | (lsb2 << 16) | (msb1 << 8) | lsb1);

#if 0
	printf("count 0x%08x\n", (unsigned int) count);
#endif

	return count;
}

static void z8536_timer_reset(struct z8536_timer_priv *priv)
{
	/* Put state machine into known state */
	readb(priv->base + Z8536_PORT_CONTROL);
	writeb(priv->base + Z8536_PORT_CONTROL, 0);
	readb(priv->base + Z8536_PORT_CONTROL);

	/* Do reset */
	z8536_timer_write_reg(priv, Z8536_INT_CTRL_REG, Z8536_INT_CTRL_RESET);
	z8536_timer_write_reg(priv, Z8536_INT_CTRL_REG, 0);
}

static int z8536_timer_probe(struct udevice *dev)
{
	struct z8536_timer_priv *priv = dev_get_priv(dev);
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	int ret;
	u8 val;

	priv->base = dev_read_addr_ptr(dev);
	/* E17/E27 manual says 5mhz clk, cio manual says the count happens at pclk/2? */
	uc_priv->clock_rate = 5000000 / 2;

	z8536_timer_reset(priv);

	/* Set the reload values for the counters we'll use */
	z8536_timer_write_reg(priv, Z8536_CT1_RELOAD_MSB_REG, 0xff);
	z8536_timer_write_reg(priv, Z8536_CT1_RELOAD_LSB_REG, 0xff);
	z8536_timer_write_reg(priv, Z8536_CT2_RELOAD_MSB_REG, 0xff);
	z8536_timer_write_reg(priv, Z8536_CT2_RELOAD_LSB_REG, 0xff);

	val = Z8536_CT_MODE_CSC;
	z8536_timer_write_reg(priv, Z8536_CT1_MODE_REG, val);

	/*
	 * Link counter 1 and counter 2, datasheet says this needs
	 * to be done before enabling, not sure what that means really
	 * but do the link part first just in case.
	 */
	val = Z8536_CFG_CTRL_LC_CLK;
	z8536_timer_write_reg(priv, Z8536_CFG_CTRL_REG, val);
	/* Enable counter 1 and counter 2 */
	val |= Z8536_CFG_CTRL_CT1E | Z8536_CFG_CTRL_CT2E;
	z8536_timer_write_reg(priv, Z8536_CFG_CTRL_REG, val);

	/* Start the two counters */
	val = Z8536_CT_CMDSTAT_GCB | Z8536_CT_CMD_TCB;
	z8536_timer_write_reg(priv, Z8536_CT1_CMDSTAT_REG, val);
	while (!(z8536_timer_read_reg(priv, Z8536_CT1_CMDSTAT_REG) & Z8536_CT_STAT_CIP)) {
	}
	z8536_timer_write_reg(priv, Z8536_CT2_CMDSTAT_REG, val);
	while (!(z8536_timer_read_reg(priv, Z8536_CT2_CMDSTAT_REG) & Z8536_CT_STAT_CIP)) {
	}

	hmmm = 0;

	return 0;
}

static const struct timer_ops z8536_timer_ops = {
	.get_count = z8536_timer_get_count,
};

static const struct udevice_id z8536_timer_ids[] = {
	{ .compatible = "zilog,z8536", },
	{ }
};

U_BOOT_DRIVER(dragonball_timer) = {
	.name = "z8536_timer",
	.id = UCLASS_TIMER,
	.of_match = of_match_ptr(z8536_timer_ids),
	.probe = z8536_timer_probe,
	.ops = &z8536_timer_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto	= sizeof(struct z8536_timer_priv),
};
