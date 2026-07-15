// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Board support for classic 68k Apple Macintosh ("oldmac").
 *
 * The first supported model is the Quadra 800 (Motorola 68040), which is also
 * what QEMU's `q800` machine emulates and is used as the development/test
 * vehicle; the true target is real hardware.
 */

#include <config.h>
#include <command.h>
#include <cpu_func.h>
#include <dm.h>
#include <linux/errno.h>
#include <init.h>
#include <serial.h>
#include <serial_scc.h>
#include <time.h>
#include <video.h>
#include <asm/bootinfo.h>
#include <asm/global_data.h>
#include <asm-generic/sections.h>
#include <dm/platdata.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/* Quadra 800 Zilog 8530 SCC, channel A (modem port = QEMU -serial) */
#define OLDMAC_SCC_CTRL_A	0x5000c022
#define OLDMAC_SCC_DATA_A	0x5000c026

static struct scc_serial_plat oldmac_scc_plat = {
	.ctrl = OLDMAC_SCC_CTRL_A,
	.data = OLDMAC_SCC_DATA_A,
};

U_BOOT_DRVINFO(oldmac_scc) = {
	.name = "serial_scc",
	.plat = &oldmac_scc_plat,
};

/* AMD 53C9x ESP SCSI controller (base filled in by the driver for q800) */
U_BOOT_DRVINFO(oldmac_esp) = {
	.name = "esp_scsi",
};

/* Machine description discovered from the Mac bootinfo (built by the ROM boot
 * chain, or by QEMU's q800 -kernel path). */
static struct {
	ulong model;		/* Gestalt model id */
	ulong rombase;
	ulong video_addr;
	ulong video_depth;
	ulong video_width;
	ulong video_height;
	ulong video_rowbytes;
} oldmac;

static const char *oldmac_model_name(ulong model)
{
	switch (model) {
	case MAC_MODEL_Q800:
		return "Quadra 800";
	default:
		return "unknown";
	}
}

/*
 * The Mac bootinfo is a list of {tag, size, data[]} records placed right after
 * the loaded image (_end), terminated by BI_LAST, exactly as the virt board and
 * QEMU's q800 -kernel path produce it.
 */
static void parse_bootinfo(void)
{
	struct bi_record *rec = (struct bi_record *)ALIGN((ulong)&_end, 2);
	int loops = 0;

	if (rec->tag != BI_MACHTYPE)
		return;

	while (rec->tag != BI_LAST) {
		if (++loops > 1024)
			return;

		switch (rec->tag) {
		case BI_MEMCHUNK:
			gd->ram_size = rec->data[1];	/* data[0]=base, [1]=size */
			break;
		case BI_MAC_MODEL:
			oldmac.model = rec->data[0];
			break;
		case BI_MAC_ROMBASE:
			oldmac.rombase = rec->data[0];
			break;
		case BI_MAC_VADDR:
			oldmac.video_addr = rec->data[0];
			break;
		case BI_MAC_VDEPTH:
			oldmac.video_depth = rec->data[0];
			break;
		case BI_MAC_VDIM:
			oldmac.video_width = rec->data[0] & 0xffff;
			oldmac.video_height = rec->data[0] >> 16;
			break;
		case BI_MAC_VROW:
			oldmac.video_rowbytes = rec->data[0];
			break;
		}
		rec = (struct bi_record *)((ulong)rec + rec->size);
	}
}

int board_early_init_f(void)
{
	parse_bootinfo();

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#include <spl.h>

/* Minimal SPL board_init_f: gd is already reserved by start.S; the SPL
 * board_init_r() (which start.S calls next) loads U-Boot proper. */
void board_init_f(ulong bootflag)
{
	gd->ram_size = SZ_128M;
}

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_SCSI;
}
#endif

int dram_init(void)
{
	/* Set by parse_bootinfo() from the Mac bootinfo; fall back to 16 MiB. */
	if (!gd->ram_size)
		gd->ram_size = SZ_16M;

	return 0;
}

#if CONFIG_IS_ENABLED(VIDEO)
/*
 * Video driver for the Mac framebuffer discovered from the ROM/bootinfo.  The
 * framebuffer already lives in NuBus card address space (e.g. 0xf90xxxxx on the
 * Quadra 800), so we point U-Boot's video uclass straight at it rather than
 * reserving RAM.  At 8bpp the console relies on the CLUT the ROM/QEMU loaded,
 * whose index 0 and 255 map to opposite ends of the greyscale ramp, so text
 * renders legibly without us reprogramming the (model-specific) DAC.
 */
static int oldmac_video_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);

	if (!oldmac.video_addr || !oldmac.video_width || !oldmac.video_height)
		return -ENODEV;

	plat->base = oldmac.video_addr;
	plat->size = 0;		/* external framebuffer; nothing to reserve */

	uc_priv->xsize = oldmac.video_width;
	uc_priv->ysize = oldmac.video_height;
	if (oldmac.video_rowbytes)
		uc_priv->line_length = oldmac.video_rowbytes;

	switch (oldmac.video_depth) {
	case 8:
		uc_priv->bpix = VIDEO_BPP8;
		break;
	case 16:
		uc_priv->bpix = VIDEO_BPP16;
		break;
	case 32:
		uc_priv->bpix = VIDEO_BPP32;
		break;
	default:
		printf("oldmac video: unsupported depth %lu\n",
		       oldmac.video_depth);
		return -EINVAL;
	}

	video_set_flush_dcache(dev, true);

	return 0;
}

U_BOOT_DRIVER(oldmac_video) = {
	.name	= "oldmac_video",
	.id	= UCLASS_VIDEO,
	.probe	= oldmac_video_probe,
};

U_BOOT_DRVINFO(oldmac_video) = {
	.name = "oldmac_video",
};
#endif /* VIDEO */

int checkboard(void)
{
	printf("Board: Apple Macintosh (%s)\n", oldmac_model_name(oldmac.model));
	if (oldmac.video_addr)
		printf("Video: %lux%lux%lu at %08lx\n", oldmac.video_width,
		       oldmac.video_height, oldmac.video_depth, oldmac.video_addr);

	return 0;
}

/*
 * TODO(oldmac): bring-up stubs.  Replace with a real VIA/RTC timer driver and a
 * proper machine reset.  These exist only so u-boot proper links and boots far
 * enough to validate the SCC console; the crude monotonic counter is not a real
 * timebase.
 */
unsigned long notrace timer_read_counter(void)
{
	static unsigned long t;

	return t += 1000;
}

ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}

void reset_cpu(void)
{
	while (1)
		;
}

int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	reset_cpu();

	return 0;
}
