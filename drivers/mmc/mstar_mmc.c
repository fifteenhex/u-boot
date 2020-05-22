// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 - Daniel Palmer
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <mmc.h>
#include <reset.h>
#include <asm/io.h>

struct mstar_mmc_platdata {

};

struct mstar_mmc_priv {

};

static int mstar_mmc_set_ios(struct udevice *dev)
{
	return 1;
}

static int mstar_mmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	return 1;
}

static int mstar_mmc_getcd(struct udevice *dev)
{
	return 1;
}

static const struct dm_mmc_ops mstar_mmc_ops = {
	.send_cmd	= mstar_mmc_send_cmd,
	.set_ios	= mstar_mmc_set_ios,
	.get_cd		= mstar_mmc_getcd,
};

static int mstar_mmc_probe(struct udevice *dev)
{
	return 1;
}

static int mstar_mmc_bind(struct udevice *dev)
{
	return 1;
}

static const struct udevice_id mstar_mmc_ids[] = {
	{
	  .compatible = "mstar,msc313-sdio",
	},
	{ /* sentinel */ }
};

U_BOOT_DRIVER(mstar_mmc_drv) = {
	.name		= "mstar_mmc",
	.id		= UCLASS_MMC,
	.of_match	= mstar_mmc_ids,
	.bind		= mstar_mmc_bind,
	.probe		= mstar_mmc_probe,
	.ops		= &mstar_mmc_ops,
	.platdata_auto_alloc_size = sizeof(struct mstar_mmc_platdata),
	.priv_auto_alloc_size = sizeof(struct mstar_mmc_priv),
};
