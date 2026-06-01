#ifndef _VDP_H
#define _VDP_H

#define VRAM_WINDOW			0xB000
#define VRAM_PLANE_A			0xC000
#define VRAM_PLANE_B			0xE000
#define VRAM_SPRITE_TABLE		0xF000
#define VRAM_HSCROLL			0xF800

#define VDP_PLANE_AB_WIDTH  32
#define VDP_PLANE_AB_HEIGHT 28
#define VDP_TILE_WIDTH 8

#ifndef __ASSEMBLY__
void vdp_set_tile(u16 plane_addr, u16 plane_index, u8 tile_index);
void vdp_set_tiles(u16 plane_addr, u16 plane_index, u8 tile_index, u16 num);
#endif


#endif /* _VDP_H */
