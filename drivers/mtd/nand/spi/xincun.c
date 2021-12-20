// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Daniel Palmer <daniel@thingy.jp>
 *
 */

#ifndef __UBOOT__
#include <linux/device.h>
#endif
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_XINCUN			0x9C

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 2, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

#ifndef CONFIG_SPL_BUILD
static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD(false, 0, NULL, 0));
#endif

static int xcsp1aawhnt_ooblayout_ecc(struct mtd_info *mtd, int section,
					struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	/* ECC is not user accessible */
	region->offset = 0;
	region->length = 0;

	return 0;
}

static int xcsp1aawhnt_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	/*
	 * No ECC data is stored in the accessible OOB so the full 16 bytes
	 * of each spare region is available to the user. Apparently also
	 * covered by the internal ECC.
	 */
	if (section) {
		region->offset = 16 * section;
		region->length = 16;
	} else {
		/* First byte in spare0 area is used for bad block marker */
		region->offset = 1;
		region->length = 15;
	}

	return 0;
}

static const struct mtd_ooblayout_ops xcsp1aawhnt_ooblayout = {
	.ecc = xcsp1aawhnt_ooblayout_ecc,
	.rfree = xcsp1aawhnt_ooblayout_free,
};

static int xcsp1aawhnt_ecc_get_status(struct spinand_device *spinand,
					u8 status)
{
	return 0;
}

static const struct spinand_info xincun_spinand_table[] = {
	SPINAND_INFO("XCSP1AAWH-NT", 0x01,
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&xcsp1aawhnt_ooblayout,
				     xcsp1aawhnt_ecc_get_status)),
};

static int xincun_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_XINCUN)
		return 0;

	ret = spinand_match_and_init(spinand, xincun_spinand_table,
				     ARRAY_SIZE(xincun_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops xincun_spinand_manuf_ops = {
	.detect = xincun_spinand_detect,
};

const struct spinand_manufacturer xincun_spinand_manufacturer = {
	.id = SPINAND_MFR_XINCUN,
	.name = "XINCUN",
	.ops = &xincun_spinand_manuf_ops,
};
