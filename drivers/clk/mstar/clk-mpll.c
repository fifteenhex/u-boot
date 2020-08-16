// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <clk.h>
#include <clk-uclass.h>
#include <regmap.h>
#include <syscon.h>
#include <chenxingv7.h>
#include <linux/err.h>
#include <dt-bindings/clock/mstar.h>

#define REG_TEST	0x0
#define REG_POWER	0x4
#define REG_CONFIG1	0x8
#define REG_CONFIG2	0xc
#define REG_STATUS	0x10

struct reg_field config1_loop_div_first = REG_FIELD(REG_CONFIG1, 8, 9);
struct reg_field config1_input_div_first = REG_FIELD(REG_CONFIG1, 4, 5);
struct reg_field config2_output_div_first = REG_FIELD(REG_CONFIG2, 12, 13);
struct reg_field config2_loop_div_second = REG_FIELD(REG_CONFIG2, 0, 7);
struct reg_field status_mpll_lock = REG_FIELD(REG_STATUS, 12, 12);

static const unsigned dividers[] = {
		1, 2, 4, 8
};

// premultiplied by 100
static const ulong output_dividers[] = {
		0,
		125,
		150,
		200,
		250,
		300,
		350,
		348,
		500,
};


struct mstar_mpll_priv {
	struct clk clk;
	struct regmap *regmap;
	struct regmap *pmsleep;
	struct regmap_field *input_div;
	struct regmap_field *output_div;
	struct regmap_field *loop_div_first;
	struct regmap_field *loop_div_second;
	struct regmap_field *plllock;
};

static ulong mstar_mpll_get_rate(struct clk *clk)
{
	struct mstar_mpll_priv *priv = dev_get_priv(clk->dev);
	uint input_div, output_div, loop_first, loop_second;
	ulong parent_rate = clk_get_rate(&priv->clk);
	ulong output_rate;

	regmap_field_read(priv->input_div, &input_div);
	regmap_field_read(priv->output_div, &output_div);
	regmap_field_read(priv->loop_div_first, &loop_first);
	regmap_field_read(priv->loop_div_second, &loop_second);

	output_rate = parent_rate / dividers[input_div];
	output_rate *= loop_first * loop_second;
	output_rate /= output_div;

	output_rate = (output_rate / output_dividers[clk->id]) * 100;

	return output_rate;
}

extern uint mplldbg;
extern uint mpllregs[];

static int mstar_mpll_enable(struct clk *clk)
{
	struct mstar_mpll_priv *priv = dev_get_priv(clk->dev);
	uint power;
	uint lock;

	mplldbg = 0x2;

	regmap_read(priv->regmap, REG_POWER, &power);
	if(power == 0){
		printf("mpll is already running\n");
		goto out;
	}

	volatile u32 *mpllreg = (u32*)0x1f206000;
	int reg = 2;
	*(mpllreg + reg) = 0x0;
	mpllregs[0] = *(mpllreg + reg);
	*(mpllreg + reg) = 0xffff;
	mpllregs[1] = *(mpllreg + reg);
	*(mpllreg + reg) = 0x0100;
	mpllregs[2] = *(mpllreg + reg);



	// this might be power control for the pll?
	regmap_write(priv->pmsleep, PMSLEEP_F4, 0);
	// vendor code has a delay here
	mdelay(10);

	// clear all of the power down bits?
	regmap_write(priv->regmap, REG_POWER, 0);
	// vendor code has a delay
	mdelay(10);

out:

	return 0;
}

static int mstar_mpll_disable(struct clk *clk)
{
	return 0;
}

const struct clk_ops mstar_mpll_ops = {
	.get_rate = mstar_mpll_get_rate,
	.enable = mstar_mpll_enable,
	.disable = mstar_mpll_disable,
};

uint mpll_registers[5] = { 0 };
uint mpll_dbg = 0;

static int mstar_mpll_probe(struct udevice *dev)
{
	struct mstar_mpll_priv *priv = dev_get_priv(dev);
	int ret;

	printf("mpll here!\n");

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if(ret)
		goto out;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	priv->pmsleep = syscon_regmap_lookup_by_phandle(dev, "mstar,pmsleep");
	if(IS_ERR(priv->pmsleep)){
		ret = PTR_ERR(priv->pmsleep);
		goto out;
	}

	priv->input_div = devm_regmap_field_alloc(dev,priv->regmap, config1_input_div_first);
	priv->output_div = devm_regmap_field_alloc(dev,priv->regmap, config2_output_div_first);
	priv->loop_div_first = devm_regmap_field_alloc(dev,priv->regmap, config1_loop_div_first);
	priv->loop_div_second = devm_regmap_field_alloc(dev,priv->regmap, config2_loop_div_second);
	priv->plllock = devm_regmap_field_alloc(dev,priv->regmap, status_mpll_lock);

	printf("mpll here xxxx!\n");

out:
	return ret;
}

static const struct udevice_id mstar_mpll_ids[] = {
	{ .compatible = "mstar,mpll", },
	{ }
};

U_BOOT_DRIVER(mstar_mpll) = {
	.name = "mstar_mpll",
	.id = UCLASS_CLK,
	.of_match = mstar_mpll_ids,
	.probe = mstar_mpll_probe,
	.priv_auto = sizeof(struct mstar_mpll_priv),
	.ops = &mstar_mpll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
