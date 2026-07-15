// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * NuBus slot enumeration for classic 68k Macintosh machines.
 *
 * Each NuBus card carries a declaration ROM at the very top of its slot
 * address space.  The ROM ends in a 20-byte "format block" that identifies the
 * valid byte lanes, a test pattern, and the offset to the card's sResource
 * directory.  This mirrors the format parsed by Linux drivers/nubus/nubus.c and
 * documented in "Designing Cards and Drivers for the Macintosh Family".
 *
 * Empty slots fault when touched, so all ROM reads go through nubus_read_safe()
 * (see nubus_buserr.S), which catches the bus error and reports the slot empty.
 */

#include <command.h>
#include <stdio.h>
#include <linux/types.h>

#define NUBUS_TEST_PATTERN	0x5A932BC7
#define FORMAT_BLOCK_SIZE	20

/* sResource IDs we decode (Slot Manager standard resources) */
#define RID_TYPE		0x01	/* category/type/DrSW/DrHW words */
#define RID_NAME		0x02	/* null-terminated card name */
#define RID_BOARD		0x01	/* the board sResource in the root dir */

/* provided by nubus_buserr.S */
int nubus_read_safe(void *addr, unsigned char *out);
void nubus_buserr_begin(void);
void nubus_buserr_end(void);

/* First byte *after* a slot's 16 MiB address space (top of the decl ROM). */
static u8 *nubus_slot_top(int slot)
{
	return (u8 *)(0xF1000000UL + ((ulong)slot << 24));
}

/* A byte lane is "useful" if its low address bits are flagged in the map. */
static int not_useful(u8 *p, int map)
{
	return !(map & (1 << ((ulong)p & 3)));
}

/* Assemble @len useful bytes big-endian, skipping dead lanes; note faults. */
static ulong get_rom(u8 **pp, int len, int map, int *err)
{
	u8 *p = *pp;
	ulong v = 0;
	u8 b;

	while (len--) {
		v <<= 8;
		while (not_useful(p, map))
			p++;
		if (nubus_read_safe(p, &b)) {
			*err = 1;
			b = 0xff;
		}
		v |= b;
		p++;
	}
	*pp = p;
	return v;
}

static void rewind_lanes(u8 **pp, int len, int map)
{
	u8 *p = *pp;

	while (len--)
		do {
			p--;
		} while (not_useful(p, map));
	*pp = p;
}

static void advance_lanes(u8 **pp, int len, int map)
{
	u8 *p = *pp;

	while (len--) {
		while (not_useful(p, map))
			p++;
		p++;
	}
	*pp = p;
}

/* Step a ROM pointer by a lane count that may be negative (an sOffset). */
static void move_lanes(u8 **pp, long len, int map)
{
	if (len > 0)
		advance_lanes(pp, len, map);
	else if (len < 0)
		rewind_lanes(pp, -len, map);
}

/* Sign-extend a 24-bit sResource offset to 32 bits. */
static long expand32(long v)
{
	if (v & 0x00800000)
		v |= 0xFF000000;
	return v;
}

struct nubus_dirent {
	u8 *base;
	u8 type;
	ulong data;	/* 24-bit: an sOffset or inline data */
};

/* Read one directory entry; returns -1 at the end-of-list marker or on fault. */
static int nubus_readdir(u8 **pp, int map, struct nubus_dirent *e)
{
	int err = 0;
	ulong resid;

	e->base = *pp;
	resid = get_rom(pp, 4, map, &err);
	if (err || (resid & 0xff000000) == 0xff000000)
		return -1;
	e->type = resid >> 24;
	e->data = resid & 0xffffff;
	return 0;
}

/* Resolve an entry's sOffset to the ROM address it points at. */
static u8 *nubus_dirptr(const struct nubus_dirent *e, int map)
{
	u8 *p = e->base;

	move_lanes(&p, expand32(e->data), map);
	return p;
}

static void nubus_read_str(u8 *p, int map, char *dst, int max)
{
	int i = 0, err;
	u8 c;

	while (i < max - 1) {
		err = 0;
		c = get_rom(&p, 1, map, &err);
		if (err || !c)
			break;
		dst[i++] = c;
	}
	dst[i] = '\0';
}

/* Determine which byte lanes a slot's decl ROM uses; 0 means no card. */
static int nubus_probe_slot(int slot)
{
	u8 *rp = nubus_slot_top(slot);
	u8 dp;
	int i;

	/* The last byte of the format block is two complementary nybbles that
	 * name the valid lanes; walk back up to four bytes to find it. */
	for (i = 4; i; i--) {
		rp--;
		if (nubus_read_safe(rp, &dp))
			continue;
		if ((((dp >> 4) ^ dp) & 0x0f) != 0x0f)
			continue;
		if (not_useful(rp, dp))
			continue;
		return dp;
	}
	return 0;
}

/* Dump the board sResource sub-directory (name + type words). */
static void nubus_dump_board(u8 *dir, int map)
{
	struct nubus_dirent e;
	char name[64];

	while (nubus_readdir(&dir, map, &e) == 0) {
		switch (e.type) {
		case RID_NAME:
			nubus_read_str(nubus_dirptr(&e, map), map, name,
				       sizeof(name));
			printf("    name:  %s\n", name);
			break;
		case RID_TYPE: {
			u8 *tp = nubus_dirptr(&e, map);
			int err = 0;
			ulong cat  = get_rom(&tp, 2, map, &err);
			ulong typ  = get_rom(&tp, 2, map, &err);
			ulong drsw = get_rom(&tp, 2, map, &err);
			ulong drhw = get_rom(&tp, 2, map, &err);

			printf("    type:  category %lu, type %lu, "
			       "DrSW %lu, DrHW %lu\n", cat, typ, drsw, drhw);
			break;
		}
		default:
			printf("    rsrc 0x%02x\n", e.type);
			break;
		}
	}
}

static int nubus_scan(void)
{
	int slot, found = 0;

	nubus_buserr_begin();

	for (slot = 9; slot <= 14; slot++) {
		int map = nubus_probe_slot(slot);
		u8 *fblock, *rp;
		long doffset;
		ulong test;
		u8 rev, format;
		int err = 0;

		if (!map)
			continue;

		rp = nubus_slot_top(slot);
		rewind_lanes(&rp, FORMAT_BLOCK_SIZE, map);
		fblock = rp;

		doffset = get_rom(&rp, 4, map, &err);
		(void)get_rom(&rp, 4, map, &err);	/* rom_length */
		(void)get_rom(&rp, 4, map, &err);	/* crc */
		rev = get_rom(&rp, 1, map, &err);
		format = get_rom(&rp, 1, map, &err);
		test = get_rom(&rp, 4, map, &err);

		printf("Slot %X: card at %p (lanes 0x%02x)\n",
		       slot, nubus_slot_top(slot), map);
		if (err || test != NUBUS_TEST_PATTERN) {
			printf("    bad declaration ROM (test pattern 0x%08lx)\n",
			       test);
			continue;
		}
		printf("    format %u, rev %u\n", format, rev);
		found++;

		/* Walk the root sResource directory; the first entry is the
		 * board sResource whose sub-directory names the card. */
		{
			u8 *dir = fblock;
			struct nubus_dirent e;

			move_lanes(&dir, expand32(doffset), map);
			while (nubus_readdir(&dir, map, &e) == 0) {
				if (e.type == RID_BOARD)
					nubus_dump_board(nubus_dirptr(&e, map),
							 map);
				else
					printf("    rsrc 0x%02x\n", e.type);
			}
		}
	}

	nubus_buserr_end();

	if (!found)
		printf("No NuBus cards found.\n");
	return found;
}

static int do_nubus(struct cmd_tbl *cmdtp, int flag, int argc,
		    char *const argv[])
{
	nubus_scan();
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(nubus, 1, 1, do_nubus,
	   "probe NuBus slots and list declaration ROMs",
	   "");
