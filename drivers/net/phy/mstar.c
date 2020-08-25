// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <config.h>
#include <common.h>
#include <phy.h>
#include <dm.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

#define MSC313_PHY_ID   0xdeadbeef
#define MSC313E_PHY_ID  0xdeadb33f
#define MSC313_PHY_MASK 0xffffffff

struct mstar_phy_priv {

};

int mstar_phy_probe(struct phy_device *phydev)
{
	struct mstar_phy_priv *priv;
	int ofnode = phydev->addr;
	u32 val;

	printf("yo!\n");

	priv = malloc(sizeof(*priv));
	if (!priv)
		return -ENOMEM;
	memset(priv, 0, sizeof(*priv));

	return 0;
}

int mstar_phy_startup(struct phy_device *phydev)
{
	struct mstar_phy_priv *priv = phydev->priv;

	return 0;
}

int mstar_phy_shutdown(struct phy_device *phydev)
{
	return 0;
}

static struct phy_driver msc313_phy_driver = {
	.uid		= MSC313_PHY_ID,
	.mask		= 0xffffffff,
	.name		= "MStar/SigmaStar PHY",
	.features	= SUPPORTED_MII,
	.probe		= mstar_phy_probe,
	.startup	= mstar_phy_startup,
	.shutdown	= mstar_phy_shutdown,
};

static struct phy_driver msc313e_phy_driver = {
	.uid		= MSC313E_PHY_ID,
	.mask		= 0xffffffff,
	.name		= "MStar/SigmaStar PHY",
	.features	= SUPPORTED_MII,
	.probe		= mstar_phy_probe,
	.startup	= mstar_phy_startup,
	.shutdown	= mstar_phy_shutdown,
};

int phy_mstar_init(void)
{
	phy_register(&msc313_phy_driver);
	phy_register(&msc313e_phy_driver);
	return 0;
}
