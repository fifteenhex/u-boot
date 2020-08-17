// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <config.h>
#include <common.h>
#include <phy.h>
#include <dm.h>
#include <fdt_support.h>
#include <syscon.h>
#include <chenxingv7.h>
#include <malloc.h>
#include <regmap.h>
#include <linux/err.h>

#include "emac.h"
#include "msc313.h"

DECLARE_GLOBAL_DATA_PTR;

#define MSC313_PHY_MASK 0xffffffff

struct mstar_phy_priv {
	struct regmap *phyana;
	struct msc313e_fields msc313e_fields;
};

static void msc313e_powerup(struct mstar_phy_priv *priv){
	printf("Doing phy power up\n");

	regmap_field_write(priv->msc313e_fields.anarst, 1);
	mdelay(100);
	regmap_field_write(priv->msc313e_fields.anarst, 0);
	mdelay(100);

	/* "Power-on LDO" */
	regmap_write(priv->phyana, REG_LDO, 0x0000);
	/* "Power-on SADC" */
	regmap_field_write(priv->msc313e_fields.sadcpd, 0);
        /* "Power-on ADCPL" */
	regmap_field_write(priv->msc313e_fields.adcplpd, 0);
        /* "Power-on REF" */
	regmap_field_write(priv->msc313e_fields.refpd, 0);
        /* "Power-on TX" */
	regmap_field_write(priv->msc313e_fields.txpd1, 0);
        /* "Power-on TX" */
	regmap_field_write(priv->msc313e_fields.txpd2, 0);
        /* "CLKO_ADC_SEL" */
	regmap_field_write(priv->msc313e_fields.clkoadcsel, 1);
	/* "reg_adc_clk_select" */
	regmap_field_write(priv->msc313e_fields.adcclksel, 1);
	/* "100gat" */
	regmap_field_write(priv->msc313e_fields.hundredgat, 0);
	/* "200gat" */
	regmap_field_write(priv->msc313e_fields.twohundredgat, 0);
}

static int mstar_phy_config(struct phy_device *phydev)
{
	struct mstar_phy_priv *priv;
	ofnode phy_node, phyana_node;
	u32 phandle;
	int ret;

	printf("phy power up\n");

	phy_node = phy_get_ofnode(phydev);
	if (!ofnode_valid(phy_node)){
		ret = -EINVAL;
		goto out;
	}

	ret = ofnode_read_u32(phy_node, "mstar,phyana", &phandle);
	if (ret)
		goto out;

	phyana_node = ofnode_get_by_phandle(phandle);
	if (!ofnode_valid(phyana_node)) {
		ret = -ENODEV;
		goto out;
	}

	priv = malloc(sizeof(*priv));
	if (!priv){
		ret = -ENOMEM;
		goto out;
	}

	phydev->priv = priv;
	priv->phyana = syscon_node_to_regmap(phyana_node);
	if(IS_ERR(priv->phyana)){
		ret = PTR_ERR(priv->phyana);
		goto out;
	}

	priv->msc313e_fields.anarst = devm_regmap_field_alloc(phydev->dev, priv->phyana, anarst);
	priv->msc313e_fields.sadcpd = devm_regmap_field_alloc(phydev->dev, priv->phyana, sadcpd);
	priv->msc313e_fields.adcplpd = devm_regmap_field_alloc(phydev->dev, priv->phyana, adcplpd);
	priv->msc313e_fields.refpd = devm_regmap_field_alloc(phydev->dev, priv->phyana, refpd);
	priv->msc313e_fields.txpd1 = devm_regmap_field_alloc(phydev->dev, priv->phyana, txpd1);
	priv->msc313e_fields.txpd2 = devm_regmap_field_alloc(phydev->dev, priv->phyana, txpd2);
	priv->msc313e_fields.clkoadcsel = devm_regmap_field_alloc(phydev->dev, priv->phyana, clkoadcsel);
	priv->msc313e_fields.adcclksel = devm_regmap_field_alloc(phydev->dev, priv->phyana, adcclksel);
	priv->msc313e_fields.hundredgat = devm_regmap_field_alloc(phydev->dev, priv->phyana, hundredgat);
	priv->msc313e_fields.twohundredgat = devm_regmap_field_alloc(phydev->dev, priv->phyana, twohundredgat);

	switch(mstar_chiptype()){
		case CHIPTYPE_MSC313:
			emacphypowerup_msc313();
			break;
		case CHIPTYPE_MSC313E:
		case CHIPTYPE_MSC313DC:
		case CHIPTYPE_SSD202D:
			msc313e_powerup(priv);
			break;
		default:
			break;
	}

	genphy_config_aneg(phydev);
	genphy_restart_aneg(phydev);

out:
	return ret;
}

static struct phy_driver msc313_phy_driver = {
	.uid		= MSC313_PHY_ID,
	.mask		= 0xffffffff,
	.name		= "MStar/SigmaStar MSC313 ethernet PHY",
	.features	= PHY_BASIC_FEATURES,
	.config		= mstar_phy_config,
	.startup	= genphy_startup,
	.shutdown	= genphy_shutdown,
};

static struct phy_driver msc313e_phy_driver = {
	.uid		= MSC313E_PHY_ID,
	.mask		= 0xffffffff,
	.name		= "MStar/SigmaStar MSC313e ethernet PHY",
	.features	= PHY_BASIC_FEATURES,
	.config		= mstar_phy_config,
	.startup	= genphy_startup,
	.shutdown	= genphy_shutdown,
};

int phy_mstar_init(void)
{
	phy_register(&msc313_phy_driver);
	phy_register(&msc313e_phy_driver);
	return 0;
}
