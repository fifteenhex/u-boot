// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <config.h>
#include <common.h>
#include <phy.h>
#include <dm.h>
#include <fdt_support.h>

DECLARE_GLOBAL_DATA_PTR;

struct mstar_phy_priv {

};

int mstar_phy_probe(struct phy_device *phydev)
{
	struct mstar_phy_priv *priv;
	int ofnode = phydev->addr;
	u32 val;

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

static struct phy_driver mstar_phy_driver = {
	.uid		= PHY_FIXED_ID,
	.mask		= 0xffffffff,
	.name		= "MStar/SigmaStar PHY",
	.features	= PHY_GBIT_FEATURES | SUPPORTED_MII,
	.probe		= mstar_phy_probe,
	.startup	= mstar_phy_startup,
	.shutdown	= mstar_phy_shutdown,
};
