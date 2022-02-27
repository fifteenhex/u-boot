#include <dm/uclass.h>
#include <linux/mtd/mtd.h>

static struct udevice *dev;
static struct mtd_info *mtd;

int spl_spinand_peb_size(void)
{
	return mtd->erasesize;
}

int spl_spinand_init(void)
{
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MTD, DM_DRIVER_GET(spinand), &dev);
	if (ret){
		//printf("Failed to probe spi-nand: %d\n", ret);
		return ret;
	}

	mtd = dev_get_uclass_priv(dev);

	return 0;
}

int spl_spinand_read_block(int block, int offset, int len, void *dst)
{
	loff_t nandoff = (spl_spinand_peb_size() * block) + offset;
	int ret, retries;
	size_t rlen;

	for(retries = 0; retries < 10; retries++) {
		ret = mtd_read(mtd, nandoff, len, &rlen, dst);
		if (!ret)
			break;
		/*
		 * EINVAL should mean the way we are reading past the end
		 * of the device and not a timeout or something, so if EINVAL
		 * comes back don't retry. This happens when the UBI partition
		 * is configured to use the remaining size of the device instead
		 * of a fixed size.
		 */
		if (ret == -EINVAL)
			break;

		printf("mtd read ret = %d, rlen %d, retry %d\n", ret, (unsigned) rlen, retries);
	}
	return ret;
}
