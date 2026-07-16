// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SPL loader: load U-Boot proper from a raw SCSI medium.
 *
 * Scans the SCSI bus and reads the U-Boot image from a fixed sector.  When
 * U-Boot lives on the same CD we booted from, prefer the CD-ROM so a hard disk
 * on the bus (which becomes the lower device number) doesn't get read instead.
 * Modelled on common/spl/spl_sata.c.
 */

#include <spl.h>
#include <scsi.h>
#include <blk.h>
#include <part.h>
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
	void *scratch = (void *)SPL_SCSI_SCRATCH;
	struct blk_desc *bd;
	unsigned long count;
	int ret, i;

	/* bring up the SCSI bus */
	ret = scsi_scan(false);
	if (ret)
		return ret;

	/*
	 * Prefer the CD-ROM (the medium we booted from, 2048-byte blocks); fall
	 * back to the first device.  This keeps us off any hard disk that scans
	 * ahead of the CD.
	 */
	bd = NULL;
	for (i = 0; ; i++) {
		struct blk_desc *d = blk_get_devnum_by_uclass_id(UCLASS_SCSI, i);

		if (!d)
			break;
		if (!bd)
			bd = d;			/* fallback: first device */
		if (d->type == DEV_TYPE_CDROM) {
			bd = d;
			break;
		}
	}
	if (!bd)
		return -ENODEV;

	/*
	 * Read the first block into the scratch buffer and parse the (raw)
	 * image header from there.  Reading a whole block avoids overrunning a
	 * fixed-size header buffer on media with a large block size, and
	 * CONFIG_TEXT_BASE is 0 here so spl_get_load_buffer() would wrap.
	 */
	if (blk_dread(bd, sector, 1, scratch) != 1)
		return -EIO;

	ret = spl_parse_image_header(spl_image, bootdev, scratch);
	if (ret)
		return ret;

	/*
	 * The pseudo-DMA transfer misbehaves when the destination is address 0
	 * (U-Boot's link/vector base), so read into the scratch buffer and copy
	 * the image down to its load address.
	 */
	count = DIV_ROUND_UP(spl_image->size, bd->blksz);
	if (blk_dread(bd, sector, count, scratch) != count)
		return -EIO;
	memcpy((void *)spl_image->load_addr, scratch, spl_image->size);

	return 0;
}
SPL_LOAD_IMAGE_METHOD("SCSI", 0, BOOT_DEVICE_SCSI, spl_scsi_load_image);
