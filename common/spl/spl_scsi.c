// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SPL loader: load U-Boot proper from a raw SCSI disk.
 *
 * Scans the SCSI bus, then reads the U-Boot image from a fixed sector of the
 * first SCSI block device (no filesystem).  Modelled on common/spl/spl_sata.c.
 */

#include <spl.h>
#include <scsi.h>
#include <blk.h>
#include <image.h>
#include <errno.h>
#include <linux/string.h>

#define SPL_SCSI_SCRATCH 0x00800000

#ifndef CONFIG_SPL_SCSI_RAW_U_BOOT_SECTOR
#define CONFIG_SPL_SCSI_RAW_U_BOOT_SECTOR 0
#endif

static int spl_scsi_load_image(struct spl_image_info *spl_image,
			       struct spl_boot_device *bootdev)
{
	const unsigned long sector = CONFIG_SPL_SCSI_RAW_U_BOOT_SECTOR;
	static u8 hdr_buf[512] __aligned(ARCH_DMA_MINALIGN);
	struct legacy_img_hdr *header = (void *)hdr_buf;
	struct blk_desc *bd;
	unsigned long count;
	int ret;

	/* bring up the SCSI bus and grab the first block device */
	ret = scsi_scan(false);
	if (ret)
		return ret;
	bd = blk_get_devnum_by_uclass_id(UCLASS_SCSI, 0);
	if (!bd)
		return -ENODEV;

	/* Note: CONFIG_TEXT_BASE is 0 here, so spl_get_load_buffer() would
	 * return a wrapped address; use a real scratch buffer for the header. */
	if (blk_dread(bd, sector, 1, header) != 1)
		return -EIO;

	ret = spl_parse_image_header(spl_image, bootdev, header);
	if (ret)
		return ret;

	/*
	 * The pseudo-DMA transfer misbehaves when the destination is address 0
	 * (U-Boot's link/vector base), so read into a scratch buffer and copy the
	 * image down to its load address.
	 */
	count = DIV_ROUND_UP(spl_image->size, bd->blksz);
	if (blk_dread(bd, sector, count, (void *)SPL_SCSI_SCRATCH) != count)
		return -EIO;
	memcpy((void *)spl_image->load_addr, (void *)SPL_SCSI_SCRATCH,
	       spl_image->size);

	return 0;
}
SPL_LOAD_IMAGE_METHOD("SCSI", 0, BOOT_DEVICE_SCSI, spl_scsi_load_image);
