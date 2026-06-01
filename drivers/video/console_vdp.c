// SPDX-License-Identifier: GPL-2.0+
/*
 *
 */

#include <asm/io.h>
#include <asm/vdp.h>
#include <charset.h>
#include <dm.h>
#include <video.h>
#include <video_console.h>
#include "vidconsole_internal.h"

static char vdp_console_cache[VDP_PLANE_AB_HEIGHT][VDP_PLANE_AB_WIDTH] = { 0 };

static int console_set_row(struct udevice *dev, uint row, int clr)
{
	//printf("%s:%d\n", __func__, __LINE__);
	/* Since we don't have normal colours assume that "clr" is just
	 * the colour needed to clear anything that is on the line.
	 *
	 * All of the code seems to use the background colour when calling
	 * this so that assumption seems correct.
	 */
	vdp_set_tiles(VRAM_PLANE_B, (row * VDP_PLANE_AB_WIDTH), 0, VDP_PLANE_AB_WIDTH);
	memset(&vdp_console_cache[row], 0, VDP_PLANE_AB_WIDTH);

	return 0;
}

static inline void vdp_putc(u16 tile, const char ch)
{
	vdp_set_tile(VRAM_PLANE_B, tile, ch);
}

static int console_move_rows(struct udevice *dev, uint rowdst, uint rowsrc, uint count)
{
	unsigned row = 0, col = 0;
	memmove(&vdp_console_cache[rowdst][0], &vdp_console_cache[rowsrc][0],
		count * VDP_PLANE_AB_WIDTH);

	for (row = 0; row < VDP_PLANE_AB_HEIGHT; row++) {
		for(col = 0; col < VDP_PLANE_AB_WIDTH; col++)
			vdp_putc((row * VDP_PLANE_AB_WIDTH) + col, vdp_console_cache[row][col]);
	}

	return 0;
}


static int console_putc_xy(struct udevice *dev, uint x_frac, uint y, int cp)
{
	//printf("%s:%d %d %d\n", __func__, __LINE__, (unsigned) x_frac, (unsigned) y);

	unsigned row = 0;
	unsigned col = 0;

	if (x_frac)
		col = x_frac / VDP_TILE_WIDTH;

	/* Return EAGAIN to trigger a new line? */
	if (col >= VDP_PLANE_AB_WIDTH)
		return -EAGAIN;

	if (y)
		row = y / 8;

	vdp_putc((row * VDP_PLANE_AB_WIDTH) + col, (char) cp);
	vdp_console_cache[row][col] = (char) cp;

	/* width of one tile */
	return VDP_TILE_WIDTH;
}

static int console_set_cursor_visible(struct udevice *dev, bool visible,
				      uint x, uint y, uint index)
{
	//printf("%s:%d\n", __func__, __LINE__);

	return 0;
}

struct vidconsole_ops console_ops = {
	.putc_xy		= console_putc_xy,
	.move_rows		= console_move_rows,
	.set_row		= console_set_row,
	.get_font_size		= console_simple_get_font_size,
	.get_font		= console_simple_get_font,
	.select_font		= console_simple_select_font,
	.set_cursor_visible	= console_set_cursor_visible,
};

U_BOOT_DRIVER(vidconsole_vdp) = {
	.name		= "vdp_console",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};
