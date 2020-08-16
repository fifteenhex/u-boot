// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <clk-uclass.h>
#include <regmap.h>
#include <chenxingv7.h>
#include <dt-bindings/clock/mstar.h>

#define MAX_OUTPUTS 2

struct mstar_clkgen_mux_priv {
	struct regmap *regmap;
	struct clk clk;
	struct clk_bulk parents;
	int num_outputs;
	u32 gate_shifts[MAX_OUTPUTS];
	u32 mux_shifts[MAX_OUTPUTS];
	u32 mux_widths[MAX_OUTPUTS];
	u32 deglitches[MAX_OUTPUTS];
	u32 parent_offset[MAX_OUTPUTS];
	struct clk *parent[MAX_OUTPUTS];
};

static ulong mstar_clkgen_mux_get_rate(struct clk *clk)
{
	struct mstar_clkgen_mux_priv *priv = dev_get_priv(clk->dev);

	return clk_get_rate(priv->parent[clk->id]);
}

static int mstar_clkgen_set_parent(struct clk *clk, struct clk *parent)
{
	struct mstar_clkgen_mux_priv *priv = dev_get_priv(clk->dev);
	int clkidx;
	u32 muxshift = priv->mux_shifts[clk->id], muxwidth = priv->mux_widths[clk->id];
	uint mask = GENMASK(muxshift + muxwidth, muxshift);

	for(clkidx = 0; clkidx < priv->parents.count; clkidx++){
		printf("checking %p %p\n", priv->parents.clks[clkidx].dev, parent->dev);
		if(priv->parents.clks[clkidx].dev == parent->dev){
			printk("found parent at %d\n", clkidx);
			goto setparent;
		}
	}

	return -EINVAL;

setparent:
	clk_enable(&priv->parents.clks[clkidx]);
	priv->parent[clk->id] = &priv->parents.clks[clkidx];
	regmap_update_bits(priv->regmap, 0, mask, clkidx << muxshift);

	return 0;
}

static int mstar_clkgen_mux_enable(struct clk *clk)
{
	struct mstar_clkgen_mux_priv *priv = dev_get_priv(clk->dev);
	uint mask = BIT(priv->gate_shifts[clk->id]);

	regmap_update_bits(priv->regmap, 0, mask, 0);

	return 0;
}

static int mstar_clkgen_mux_disable(struct clk *clk)
{
	struct mstar_clkgen_mux_priv *priv = dev_get_priv(clk->dev);
	uint mask = BIT(priv->gate_shifts[clk->id]);

	regmap_update_bits(priv->regmap, 0, mask, mask);

	return 0;
}

const struct clk_ops mstar_clkgen_mux_ops = {
	.set_parent = mstar_clkgen_set_parent,
	.enable = mstar_clkgen_mux_enable,
	.disable = mstar_clkgen_mux_disable,
	.get_rate = mstar_clkgen_mux_get_rate,
};

#define CLKGEN_UART_UART0MUX_MASK	(BIT(3) | BIT(2))

static int mstar_clkgen_mux_probe(struct udevice *dev)
{
	struct mstar_clkgen_mux_priv *priv = dev_get_priv(dev);
	int ret, i;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if(ret)
		goto out;

	ret = clk_get_bulk(dev, &priv->parents);
	if(ret)
		goto out;

	priv->num_outputs = ofnode_read_string_count(dev->node,
			MSTAR_CLKGEN_MUX_OUTPUT_NAMES);

	if(priv->num_outputs <= 0 || priv->num_outputs > MAX_OUTPUTS){
		ret = -EINVAL;
		goto out;
	}

	ret = ofnode_read_u32_array(dev->node, MSTAR_CLKGEN_MUX_GATE_SHIFTS,
				  priv->gate_shifts, priv->num_outputs);
	if(ret)
		goto out;

	ret = ofnode_read_u32_array(dev->node, MSTAR_CLKGEN_MUX_MUX_SHIFTS,
				  priv->mux_shifts, priv->num_outputs);
	if(ret)
		goto out;

	ret = ofnode_read_u32_array(dev->node, MSTAR_CLKGEN_MUX_MUX_WIDTHS,
				  priv->mux_widths, priv->num_outputs);
	if(ret)
		goto out;

	memset(priv->deglitches, 0xff, sizeof(priv->deglitches));

	ofnode_read_u32_array(dev->node, MSTAR_CLKGEN_MUX_DEGLITCHES,
				  priv->deglitches, priv->num_outputs);

	for(i = 0; i < priv->num_outputs; i++){
		priv->parent[i] = &priv->parents.clks[0];
		priv->parent_offset[i] = 0;
	}

	/*uint uartclkgen;
	regmap_read(priv->regmap, 0, &uartclkgen);
	uartclkgen &= ~CLKGEN_UART_UART0MUX_MASK;
	regmap_write(priv->regmap, 0, uartclkgen);*/

out:
	return ret;
}

static const struct udevice_id mstar_clkgen_mux_ids[] = {
	{ .compatible = "mstar,clkgen_mux", },
	{ }
};

U_BOOT_DRIVER(mstar_clkgen_mux) = {
	.name = "mstar_clkgen_mux",
	.id = UCLASS_CLK,
	.of_match = mstar_clkgen_mux_ids,
	.probe = mstar_clkgen_mux_probe,
	.priv_auto_alloc_size = sizeof(struct mstar_clkgen_mux_priv),
	.ops = &mstar_clkgen_mux_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
