#ifndef _VDP_H
#define _VDP_H

#define VRAM_WINDOW			0xB000
#define VRAM_PLANE_A			0xC000
#define VRAM_PLANE_B			0xE000
#define VRAM_SPRITE_TABLE		0xF000
#define VRAM_HSCROLL			0xF800

#ifndef __ASSEMBLY__
void vdp_init(void);
void vdp_set_tile(u16 plane_addr, u8 plane_index, u8 tile_index);
#endif


#endif /* _VDP_H */
