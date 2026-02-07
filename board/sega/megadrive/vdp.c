/*
 *
 */

#include <asm/io.h>
#include <asm/vdp.h>
#include <command.h>
#include <linux/bitops.h>
#include <string.h>
#include <vsprintf.h>

#define VDP_DATA	0xc00000
#define VDP_CTRL	0xc00004
#define VDP_CTRL_CD0	BIT(30)
#define VDP_CTRL_CD1	BIT(31)
#define VDP_CTRL_CD2	BIT(4)
#define VDP_CTRL_CD3	BIT(5)
#define VDP_CTRL_CD4	BIT(6)
#define VDP_CTRL_CD5	BIT(7))

#define VDP_REG_MODE1			0x00
#define VDP_MODE1_M4			BIT(2)
#define VDP_REG_MODE2			0x01
#define VDP_MODE2_M5			BIT(2)
#define VDP_MODE2_DE			BIT(6)
#define VDP_REG_PLANE_A_ADDR		0x02
#define VDP_REG_WINDOW_ADDR		0x03
#define VDP_REG_PLANE_B_ADDR		0x04
#define VDP_REG_SPRITE_ADDR		0x05
#define VDP_REG_SPRITE_PAT		0x06
#define VDP_REG_BG_COLOR		0x07
#define VDP_REG_H_INT_COUNTER		0x0A
#define VDP_REG_MODE_SET_3		0x0B
#define VDP_REG_MODE_SET_4		0x0C
#define VDP_REG_HSCROLL_ADDR		0x0D
#define VDP_REG_INC			0x0F
#define VDP_REG_PLANE_SIZE		0x10
#define VDP_REG_WINDOW_H_POS		0x11
#define VDP_REG_WINDOW_V_POS		0x12
#define VDP_REG_DMA_LEN_L		0x13
#define VDP_REG_DMA_LEN_H		0x14
#define VDP_REG_DMA_SRC_L		0x15
#define VDP_REG_DMA_SRC_M		0x16
#define VDP_REG_DMA_SRC_H		0x17

#define WORDS_PER_TILE			16

static inline void vdp_register_set(u8 reg, u8 value)
{
	u16 _reg = reg;
	u16 tmp = (0x8000 | (_reg << 8) | value);
	writew(tmp, VDP_CTRL);
}

static inline void vdp_cram_set_addr(u16 addr) {
	u32 _addr = addr;
	u32 v = (_addr & 0x3fff) << 16;
	v |= (VDP_CTRL_CD1 | VDP_CTRL_CD0);

	writel(v, VDP_CTRL);
}

static inline void vdp_vram_set_addr(u16 addr)
{
	u32 _addr = addr;
	u32 v;

	v = (_addr & 0x3fff) << 16;
	v |= (_addr & 0xc000) >> 14;

	/* write */
	v |=  VDP_CTRL_CD0;

	writel(v, VDP_CTRL);
}

#define VDP_VRAM_WRITE(addr)    ((0x4000 | (((addr) & 0x3FFF) << 0)) | (((addr) & 0xC000) >> 14))

static inline void vdp_data_write(u16 value)
{
	writew(value, VDP_DATA);
}

static inline void vdp_palette_upload(u8 index, const u16 *pallete) {
	u16 cram_addr = index * 32;
	int i;

	vdp_cram_set_addr(cram_addr);

	for (i = 0; i < 16; i++)
		vdp_data_write(pallete[i]);
}

static inline u16 vdp_status(void)
{
	return readw(VDP_CTRL);
}

static const u16 palette[16] = {
	0x0000, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff,
	0xffff, 0xffff, 0xffff, 0xffff,
};

void vdp_set_tile(u16 plane_addr, u8 plane_index, u8 tile_index)
{
	uint16_t tmp;

	tmp = tile_index;

	vdp_vram_set_addr(plane_addr + (plane_index * 2));
	vdp_data_write(tmp);
}

static inline void vdp_upload_tile(unsigned int index, const u16 *src)
{
	int i;

	vdp_vram_set_addr(32 * index);
	for (i = 0; i < WORDS_PER_TILE; i++)
		vdp_data_write(*src++);
}

static inline void vdp_fontch_to_tile(const u8 *src, u16 *dst)
{
	int i, j;

	for (i = 0; i < 8; i++)
	{
		uint32_t row = 0;
		/* The font is 1bpp so check each bit */
		for (j = 0; j < 8; j++) {
			if ((src[i] >> j) & 1) {
				/* The tile is 4bpp so shift left by 4 */
				row |= (0xf << (j * 4));
			}
		}
		//printf("0x%08x\n", (unsigned) row);

		/* split the words and jam um in dst */
		dst[i * 2] = (row >> 16) & 0xffff;
		dst[(i * 2) + 1] = row & 0xffff;
	}

}

extern const u8 vdp_font[];

static void vdp_puts(const char *str)
{
	int i;

	for (i = 0; *str; i++) {
		char ch = *str++;
		vdp_set_tile(VRAM_PLANE_B, i, ch);
	}
}

void vdp_init(void)
{
	uint16_t tmp_tile[WORDS_PER_TILE];
	int i;

	vdp_register_set(VDP_REG_INC, 2);

	vdp_palette_upload(0, palette);
	vdp_palette_upload(1, palette);
	vdp_palette_upload(2, palette);
	vdp_palette_upload(3, palette);

	/* Setup the addresses for the planes etc */
	vdp_register_set(VDP_REG_PLANE_A_ADDR, VRAM_PLANE_A >> 10);
	vdp_register_set(VDP_REG_PLANE_B_ADDR, VRAM_PLANE_B >> 13);
	vdp_register_set(VDP_REG_WINDOW_ADDR, VRAM_WINDOW >> 10);
	vdp_register_set(VDP_REG_SPRITE_ADDR, VRAM_SPRITE_TABLE >> 9);
	vdp_register_set(VDP_REG_HSCROLL_ADDR, VRAM_HSCROLL >> 10);


	vdp_register_set(VDP_REG_MODE1, VDP_MODE1_M4);
	vdp_register_set(VDP_REG_MODE2, VDP_MODE2_M5 | VDP_MODE2_DE);
	vdp_register_set(VDP_REG_BG_COLOR, 0x00);

	for (i = 0; i < 255; i++) {
		vdp_fontch_to_tile(vdp_font + (i * 8), tmp_tile);
		vdp_upload_tile(i, tmp_tile);
	}
}

enum vdp_cmd_target {
	VDP_VRAM,
	VDP_CRAM,
	VDP_UNKNOWN,
};

static enum vdp_cmd_target vdp_string_to_target(const char* str)
{
	if (strcmp(str, "vram") == 0) {
		return VDP_VRAM;
	}
	else if (strcmp(str, "cram") == 0) {
		return VDP_CRAM;
	}

	return VDP_UNKNOWN;
};

static int do_vdp(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	const char *action, *target, *addrstr, *valstr;
	ulong addr;
	ulong value;

	if (argc < 2) {
		printf("Usage: %s <argument>\n", argv[0]);
		return CMD_RET_USAGE;

	}

	action = argv[1];
	target = argv[2];
	addrstr = argv[3];
	valstr = argv[4];

	if(strcmp(action, "set") == 0) {
		printf("set\n");
		addr = simple_strtoul(addrstr, NULL, 0) & 0xffff;
		value = simple_strtoul(valstr, NULL, 0) & 0xffff;
		switch(vdp_string_to_target(argv[2])) {
		case VDP_VRAM:
			printf("vram 0x%04x, 0x%04x\n", (unsigned int) addr, (unsigned int) value);
			vdp_vram_set_addr(addr);
			vdp_data_write(value);
			break;
		case VDP_CRAM:
			printf("cram 0x%04x, 0x%04x\n", (unsigned int) addr, (unsigned int) value);
			vdp_cram_set_addr(addr);
			vdp_data_write(value);
			break;
		case VDP_UNKNOWN:
			printf("dunno about that buddy\n");
			break;
		}
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	vdp,
	5,
	0,
	do_vdp,
	"VDP poking",
	"poke the vdp\n"    /* Long help text */
	"mycommand <arg1> [arg2] ...\n"
	"    arg1 - first argument\n"
	"    arg2 - optional second argument"
);
