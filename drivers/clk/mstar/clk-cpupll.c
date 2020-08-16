// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <clk-uclass.h>
#include <asm/io.h>
#include <regmap.h>
#include <syscon.h>

#include <chenxingv7.h>

#define CPUPLL				0x1f206400
#define CPUPLL_44			0x44
#define CPUPLL_48			0x48
#define CPUPLL_CURFREQ_L		0x180
#define CPUPLL_CURFREQ_H		0x184
#define CPUPLL_LPF_LOW_L		0x140
#define CPUPLL_LPF_LOW_H		0x144
#define CPUPLL_LPF_HIGH_BOTTOM		0x148
#define CPUPLL_LPF_HIGH_TOP		0x14c
#define CPUPLL_LPF_TOGGLE		0x150
#define CPUPLL_LPF_MYSTERYTWO		0x154
#define CPUPLL_LPF_UPDATE_COUNT		0x15c
#define CPUPLL_LPF_MYSTERYONE		0x160
#define CPUPLL_LPF_TRANSITIONCTRL	0x164
#define CPUPLL_LPF_LOCK			0x174
#define CPUPLL_188			0x188

#define L3BRIDGE_04			0x04
#define L3BRIDGE_04_CLK_MIU2X_SEL	BIT(7)

struct mstar_cpupll_priv {
	struct regmap *regmap;
	struct regmap *l3bridge;
	struct clk clk;
};

static void mstar_cpu_clk_readback(void)
{
	uint16_t readback;

	mstar_writew(0x0001, L3BRIDGE + L3BRIDGE_2C); //test out sel?
	mstar_writew(0x0004, 0x1f203ddc);
	mstar_writew(0x4004, 0x1f203dd4);
	mstar_writew(0x0001, 0x1f203dd8);

	mstar_writew(0x0000, 0x1f203dc0);
	mstar_writew(0x8000, 0x1f203dc0);

	mstar_delay(100);

	readback = readw(0x1f203dc4);

	printf("readback: %04x\n", readback);

}

#define FREQ_400 0x0067AE14
#define FREQ_800 0x0043b3d5
#define FREQ_1000 0x002978d4
#define BUMPFREQ FREQ_800

void mstar_bump_cpufreq(void)
{
	uint16_t temp1, temp2;

	temp1 = readw(CPUPLL + CPUPLL_CURFREQ_L);
	temp2 = readw(CPUPLL + CPUPLL_CURFREQ_H);
	mstar_writew(temp1, CPUPLL + CPUPLL_LPF_LOW_L);
	mstar_writew(temp2, CPUPLL + CPUPLL_LPF_LOW_H);

	mstar_writew(BUMPFREQ & 0xFFFF, CPUPLL + CPUPLL_LPF_HIGH_BOTTOM);
	mstar_writew((BUMPFREQ >> 16) & 0xFFFF, CPUPLL + CPUPLL_LPF_HIGH_TOP);


	mstar_writew(0x1, CPUPLL + CPUPLL_LPF_MYSTERYONE);
	mstar_writew(0x6, CPUPLL + CPUPLL_LPF_MYSTERYTWO);
	mstar_writew(0x8, CPUPLL + CPUPLL_LPF_UPDATE_COUNT);
	mstar_writew(BIT(12), CPUPLL + CPUPLL_LPF_TRANSITIONCTRL);

	mstar_writew(0, CPUPLL + CPUPLL_LPF_TOGGLE);
	mstar_writew(1, CPUPLL + CPUPLL_LPF_TOGGLE);

	while (!(readw(CPUPLL + CPUPLL_LPF_LOCK))) {
		printf("waiting for cpupll lock\n");
	}

	mstar_cpu_clk_readback();
}

static int mstar_cpupll_enable(struct clk *clk)
{
	struct mstar_cpupll_priv *priv = dev_get_priv(clk->dev);
	uint temp;

	regmap_write(priv->regmap, CPUPLL_48, 0x0088);
	regmap_read(priv->regmap, CPUPLL_44, &temp);
	regmap_write(priv->regmap, CPUPLL_44, temp | 0x0100);

	regmap_write(priv->regmap, CPUPLL_CURFREQ_L, 0xAE14);
	regmap_write(priv->regmap, CPUPLL_CURFREQ_H, 0x0067);

	regmap_write(priv->regmap, CPUPLL_188, 1);
	regmap_write(priv->regmap, CPUPLL_44, temp & ~0x0100);

	mdelay(1000);

	mstar_writew(0x0001, 0x1f2041f0);

	regmap_read(priv->l3bridge, L3BRIDGE_04, &temp);
	regmap_write(priv->l3bridge, L3BRIDGE_04, temp | L3BRIDGE_04_CLK_MIU2X_SEL);
	mdelay(1000);

	return 0;
}

static int mstar_cpupll_disable(struct clk *clk)
{
	return 0;
}

const struct clk_ops mstar_cpupll_ops = {
	.enable = mstar_cpupll_enable,
	.disable = mstar_cpupll_disable,
};

static int mstar_cpupll_probe(struct udevice *dev)
{
	struct mstar_cpupll_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem_index(dev_ofnode(dev), &priv->regmap, 0);
	if(ret)
		goto out;

	priv->l3bridge = syscon_regmap_lookup_by_phandle(dev, "mstar,l3bridge");
	if(!priv->l3bridge){
		ret = -ENODEV;
		goto out;
	}

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if(ret)
		goto out;

	clk_enable(&priv->clk);

out:
	return ret;
}

static const struct udevice_id mstar_cpupll_ids[] = {
	{ .compatible = "mstar,cpupll", },
	{ }
};

U_BOOT_DRIVER(mstar_cpupll) = {
	.name = "mstar_cpupll",
	.id = UCLASS_CLK,
	.of_match = mstar_cpupll_ids,
	.probe = mstar_cpupll_probe,
	.priv_auto_alloc_size = sizeof(struct mstar_cpupll_priv),
	.ops = &mstar_cpupll_ops,
	.flags = DM_FLAG_PRE_RELOC,
};
