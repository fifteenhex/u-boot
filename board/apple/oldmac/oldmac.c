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
#include <dm/platdata.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Zilog 8530 SCC, channel A (modem port = QEMU -serial).  ctrl and data are
 * filled in from the detected model (oldmac_apply_model()); the default is the
 * Quadra-class base so the pre-DM debug console works before detection.  The
 * canonical hardware address is 0x50F0C020; on QEMU q800 the whole IO region is
 * aliased so 0x50F0xxxx and 0x5000xxxx reach the same registers.
 */
#define QUADRA_SCC_BASE		0x50f0c020

static struct scc_serial_plat oldmac_scc_plat = {
	.ctrl = QUADRA_SCC_BASE + 2,
	.data = QUADRA_SCC_BASE + 6,
};

U_BOOT_DRVINFO(oldmac_scc) = {
	.name = "serial_scc",
	.plat = &oldmac_scc_plat,
};

/* AMD 53C9x ESP SCSI controller (base from the model via board_esp_base()) */
U_BOOT_DRVINFO(oldmac_esp) = {
	.name = "esp_scsi",
};

/* On-board DP8393x SONIC Ethernet.  The driver probe gates itself on the
 * detected model via oldmac_sonic_base(), so this is inert on models without
 * a SONIC.  Only bound when the Ethernet driver is built in (not in the SPL). */
#if CONFIG_IS_ENABLED(DM_ETH)
U_BOOT_DRVINFO(oldmac_sonic) = {
	.name = "sonic_eth",
};
#endif

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

/*
 * Per-model capability table.  Peripherals that are not present on every
 * classic Mac (e.g. the on-board SONIC Ethernet, whose base address is also
 * model-specific) are described here and gated on the model detected from the
 * ROM bootinfo, so drivers only come up on machines that actually have them.
 * Add a row here to bring up another model later.
 */
struct oldmac_model_info {
	ulong		model;		/* Gestalt machine type */
	const char	*name;
	ulong		scc_base;	/* Z8530 SCC; chA ctrl = base+2, data = base+6 */
	ulong		scsi_base;	/* 53C9x ESP; 0 if not ESP-based (e.g. IIsi) */
	ulong		sonic_base;	/* on-board DP8393x SONIC, 0 if none */
	ulong		sonic_prom;	/* SONIC MAC-address PROM, 0 if none */
};

/*
 * Real-hardware (0x50F0xxxx) base addresses; QEMU q800 aliases the whole IO
 * region so these also work under emulation.  Quadra-class machines share the
 * SCC/ESP/SONIC layout.
 */
static const struct oldmac_model_info oldmac_model_table[] = {
	{ MAC_MODEL_Q800, "Quadra 800",
	  0x50f0c020, 0x50f10000, 0x50f0a000, 0x50f08000 },
};

static const struct oldmac_model_info *oldmac_cur_model(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(oldmac_model_table); i++)
		if (oldmac_model_table[i].model == oldmac.model)
			return &oldmac_model_table[i];

	return NULL;
}

static const char *oldmac_model_name(ulong model)
{
	const struct oldmac_model_info *m = oldmac_cur_model();

	return m ? m->name : "unknown";
}

/* SONIC Ethernet: base address for the detected model, or 0 if it has none. */
ulong oldmac_sonic_base(void)
{
	const struct oldmac_model_info *m = oldmac_cur_model();

	return m ? m->sonic_base : 0;
}

ulong oldmac_sonic_prom(void)
{
	const struct oldmac_model_info *m = oldmac_cur_model();

	return m ? m->sonic_prom : 0;
}

/* 53C9x ESP SCSI base for the detected model; the ESP driver's weak
 * board_esp_base() hook resolves to this. */
phys_addr_t board_esp_base(void)
{
	const struct oldmac_model_info *m = oldmac_cur_model();

	return m ? m->scsi_base : 0;
}

/* Point the SCC serial platdata at the detected model's registers. */
static void oldmac_apply_model(void)
{
	const struct oldmac_model_info *m = oldmac_cur_model();

	if (m && m->scc_base) {
		oldmac_scc_plat.ctrl = m->scc_base + 2;
		oldmac_scc_plat.data = m->scc_base + 6;
	}
}

/*
 * The Mac bootinfo is a list of {tag, size, data[]} records the boot block
 * leaves at the fixed MAC_BOOTINFO_ADDR, terminated by BI_LAST.  Reading it from
 * a fixed address (rather than just after _end) means the ROM-provided values
 * reach U-Boot whether it is entered directly by the boot block or via the SPL.
 */
static void parse_bootinfo(void)
{
	struct bi_record *rec = (struct bi_record *)MAC_BOOTINFO_ADDR;
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
	oldmac_apply_model();

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#include <spl.h>

/* Minimal SPL board_init_f: gd is already reserved by start.S; the SPL
 * board_init_r() (which start.S calls next) loads U-Boot proper.  Take the RAM
 * size from the ROM-provided bootinfo the boot block left for us rather than
 * hard-coding it. */
void board_init_f(ulong bootflag)
{
	parse_bootinfo();
	oldmac_apply_model();
	if (!gd->ram_size)
		gd->ram_size = SZ_16M;
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
