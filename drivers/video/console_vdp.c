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

static u8 vdp_cursor_x = 0;
static u8 vdp_cursor_y = 0;
static u8 vdp_cursor = 0;

static int console_set_row(struct udevice *dev, uint row, int clr)
{
	return 0;
}

static int console_move_rows(struct udevice *dev, uint rowdst, uint rowsrc, uint count)
{
	return 0;
}

void vdp_putc(const char ch)
{
	vdp_set_tile(VRAM_PLANE_B, vdp_cursor++, ch);

}

static int console_putc_xy(struct udevice *dev, uint x_frac, uint y, int cp)
{
	vdp_putc((char) cp);

	return 0;
}

static int console_set_cursor_visible(struct udevice *dev, bool visible,
				      uint x, uint y, uint index)
{
	return 0;
}

struct vidconsole_ops console_ops = {
	.putc_xy	= console_putc_xy,
	.move_rows	= console_move_rows,
	.set_row	= console_set_row,
	.get_font_size	= console_simple_get_font_size,
	.get_font	= console_simple_get_font,
	.select_font	= console_simple_select_font,
	.set_cursor_visible	= console_set_cursor_visible,
};

U_BOOT_DRIVER(vidconsole_vdp) = {
	.name		= "vdpconsole",
	.id		= UCLASS_VIDEO_CONSOLE,
	.ops		= &console_ops,
	.probe		= console_probe,
	.priv_auto	= sizeof(struct console_simple_priv),
};
