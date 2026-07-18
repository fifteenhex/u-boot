// SPDX-License-Identifier: GPL-2.0+
/*
 * mkoldmaccd - hand-crafted bootable Macintosh CD/disk image builder.
 *
 * Emits, with no external ISO/HFS/FAT tooling, the exact bytes the Apple ROM
 * needs to find and boot our own code off a SCSI CD-ROM or hard disk on a
 * classic 68k Macintosh such as the Quadra 700/800:
 *
 *   - block 0            : Apple Driver Descriptor Record ('ER'), declaring a driver
 *   - partition map      : 'PM' entries: self, Apple_Driver43, Apple_HFS (bootable),
 *                          and (hard-disk mode) a trailing DOS_FAT_16 data partition
 *   - Apple_Driver43     : our own minimal Mac disk driver (driver.bin)
 *   - Apple_HFS volume   : boot blocks (our 1024-byte 'LK' block) + HFS MDB
 *   - block 128          : raw SPL image the boot block loads
 *
 * Two output layouts, selected by the block size and --fat:
 *
 *   CD (default, -s 2048): the SPL loads U-Boot proper and the kernel/initramfs
 *   from further raw blocks (payload2..payload4), as before.
 *
 *   hard disk (-s 512 --fat): everything after the SPL lives in a real FAT16
 *   filesystem in a trailing Apple partition, so files can be dropped in by
 *   name.  The SPL loads "u-boot.img" from it; U-Boot proper fatloads the
 *   kernel and initramfs.  The FAT filesystem is generated here in C.
 *
 * Apple on-disk structures are big-endian (m68k); the FAT filesystem is
 * little-endian (its x86 origin).  Structures referenced from "Inside
 * Macintosh: Files/Devices" and the FAT specification.
 *
 * Usage:
 *   mkoldmaccd [-s sector] [--fat] bootblock.bin driver.bin out spl.bin \
 *              [payload2 payload3 payload4]        (CD/raw mode)
 *              [--fatfile NAME=PATH ...]           (--fat mode)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned int SECTOR = 2048;	/* logical (partition-map) block size */

/* ------- big-endian store helpers (Apple structures) ------- */
static void be16(void *p, uint16_t v){ uint8_t*b=p; b[0]=v>>8; b[1]=v; }
static void be32(void *p, uint32_t v){ uint8_t*b=p; b[0]=v>>24; b[1]=v>>16; b[2]=v>>8; b[3]=v; }

/* ------- little-endian store helpers (FAT structures) ------- */
static void le16(void *p, uint16_t v){ uint8_t*b=p; b[0]=v; b[1]=v>>8; }
static void le32(void *p, uint32_t v){ uint8_t*b=p; b[0]=v; b[1]=v>>8; b[2]=v>>16; b[3]=v>>24; }

/* Apple partition-map driver checksum: rotate-left running sum. */
static uint16_t map_checksum(const uint8_t *a, unsigned len)
{
	uint16_t sum = 0;
	len &= 0xFFFF;
	for (unsigned j = 0; j < len; j++) {
		sum += a[j];
		sum = (uint16_t)((sum << 1) | (sum >> 15));
	}
	return sum ? sum : 0xFFFF;
}

#define DD_SIG   0x4552   /* 'ER' */
#define PM_SIG   0x504D   /* 'PM' */
#define HFS_SIG  0x4244   /* 'BD' Master Directory Block */
#define kDriverTypeMacSCSI    0x0001
#define kSCSICDDriverSignature 0x43447672 /* 'CDvr' */

/* partition status bits */
#define P_VALID      0x00000001
#define P_ALLOCATED  0x00000002
#define P_INUSE      0x00000004
#define P_BOOTVALID  0x00000008
#define P_READABLE   0x00000010
#define P_WRITEABLE  0x00000020
#define P_BOOTPIC    0x00000040
#define P_CHAINCOMPAT 0x00000100
#define P_REALDRIVER 0x00000200
#define P_STARTUP    0x80000000

/* a partition-map entry, 512 bytes, field offsets per struct Partition */
static void pmap(uint8_t *e, uint32_t mapcnt, uint32_t start, uint32_t count,
		 const char *name, const char *type, uint32_t status)
{
	memset(e, 0, 512);
	be16(e + 0, PM_SIG);
	be32(e + 4, mapcnt);          /* pmMapBlkCnt */
	be32(e + 8, start);           /* pmPyPartStart */
	be32(e + 12, count);          /* pmPartBlkCnt */
	strncpy((char *)e + 16, name, 31);
	strncpy((char *)e + 48, type, 31);
	be32(e + 88, status);         /* pmPartStatus */
}

/* parse a size like "64M", "512K", "1G" or a plain byte count into bytes. */
static uint64_t parse_size(const char *s)
{
	char *end;
	uint64_t v = strtoull(s, &end, 0);

	switch (*end) {
	case 'g': case 'G': v <<= 30; break;
	case 'm': case 'M': v <<= 20; break;
	case 'k': case 'K': v <<= 10; break;
	case '\0': break;
	default:
		fprintf(stderr, "bad size '%s'\n", s);
		exit(2);
	}
	return v;
}

/* read a whole file into a freshly malloc'd buffer; *lenp gets the size. */
static uint8_t *slurp(const char *path, uint32_t *lenp)
{
	FILE *f = fopen(path, "rb");
	if (!f) { perror(path); exit(1); }
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *b = malloc(n ? n : 1);
	if (!b) { perror("malloc"); exit(1); }
	if (n && fread(b, 1, n, f) != (size_t)n) { perror(path); exit(1); }
	fclose(f);
	*lenp = (uint32_t)n;
	return b;
}

/* ---------------- FAT16 filesystem generation ---------------- */

struct fatfile { const char *name; const uint8_t *data; uint32_t size; };

/* format "u-boot.img" into an 11-byte 8.3 directory name (uppercased, space
 * padded, split on the last dot). */
static void fat_name(uint8_t *out, const char *name)
{
	memset(out, ' ', 11);
	const char *dot = strrchr(name, '.');
	int base_len = dot ? (int)(dot - name) : (int)strlen(name);
	for (int i = 0; i < base_len && i < 8; i++) {
		char c = name[i];
		out[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
	}
	if (dot)
		for (int i = 0; i < 3 && dot[1 + i]; i++) {
			char c = dot[1 + i];
			out[8 + i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
		}
}

/*
 * Build a FAT16 filesystem holding the given files.  Returns a malloc'd buffer
 * and its size in 512-byte sectors via *nsect.  part_lba is the partition's
 * start block, stored in the BPB hidden-sectors field.
 */
static uint8_t *build_fat16(struct fatfile *files, int nfiles,
			    uint32_t part_lba, uint32_t *nsect)
{
	const uint32_t BPS = 512;
	const uint32_t spc = 8;			/* 4 KiB clusters */
	const uint32_t cbytes = BPS * spc;
	const uint32_t reserved = 1;
	const uint32_t nfats = 2;
	const uint32_t root_entries = 512;
	const uint32_t root_sectors = (root_entries * 32 + BPS - 1) / BPS;

	uint32_t file_clusters = 0;
	for (int i = 0; i < nfiles; i++)
		file_clusters += (files[i].size + cbytes - 1) / cbytes;

	/* Keep the count comfortably in the FAT16 range (>= 4085 clusters) so
	 * U-Boot's driver never mistakes it for FAT12; add slack for the files. */
	uint32_t data_clusters = file_clusters + file_clusters / 4 + 16;
	if (data_clusters < 4200)
		data_clusters = 4200;

	uint32_t spf = ((data_clusters + 2) * 2 + BPS - 1) / BPS;
	uint32_t data_sectors = data_clusters * spc;
	uint32_t total = reserved + nfats * spf + root_sectors + data_sectors;

	uint8_t *b = calloc(total, BPS);
	if (!b) { perror("calloc"); exit(1); }

	/* BPB / boot sector (all little-endian) */
	b[0] = 0xEB; b[1] = 0x3C; b[2] = 0x90;
	memcpy(b + 3, "MKOLDMAC", 8);
	le16(b + 0x0B, BPS);
	b[0x0D] = spc;
	le16(b + 0x0E, reserved);
	b[0x10] = nfats;
	le16(b + 0x11, root_entries);
	le16(b + 0x13, total < 65536 ? total : 0);
	b[0x15] = 0xF8;				/* media: fixed disk */
	le16(b + 0x16, spf);
	le16(b + 0x18, 32);			/* sectors/track */
	le16(b + 0x1A, 64);			/* heads */
	le32(b + 0x1C, part_lba);		/* hidden sectors */
	le32(b + 0x20, total >= 65536 ? total : 0);
	b[0x24] = 0x80;				/* drive number */
	b[0x26] = 0x29;				/* extended boot signature */
	le32(b + 0x27, 0x4F4C4D43);		/* volume id 'OLMC' */
	memcpy(b + 0x2B, "OLDMAC     ", 11);	/* volume label */
	memcpy(b + 0x36, "FAT16   ", 8);
	b[0x1FE] = 0x55; b[0x1FF] = 0xAA;

	uint8_t *fat = b + reserved * BPS;
	uint8_t *root = b + (reserved + nfats * spf) * BPS;
	uint8_t *data = root + root_sectors * BPS;

	le16(fat + 0, 0xFFF8);			/* entry 0: media descriptor */
	le16(fat + 2, 0xFFFF);			/* entry 1: end-of-chain */

	uint32_t cluster = 2;
	for (int i = 0; i < nfiles; i++) {
		uint32_t nclu = (files[i].size + cbytes - 1) / cbytes;
		if (nclu == 0)
			nclu = 1;
		uint32_t first = cluster;

		memcpy(data + (first - 2) * cbytes, files[i].data, files[i].size);
		for (uint32_t c = 0; c < nclu; c++) {
			uint32_t cl = first + c;
			le16(fat + cl * 2, c == nclu - 1 ? 0xFFFF : cl + 1);
		}
		cluster += nclu;

		uint8_t *e = root + i * 32;
		fat_name(e, files[i].name);
		e[0x0B] = 0x20;			/* attr: archive */
		le16(e + 0x1A, first);		/* first cluster (low) */
		le32(e + 0x1C, files[i].size);
	}

	memcpy(b + (reserved + spf) * BPS, fat, spf * BPS);	/* mirror FAT */

	*nsect = total;
	return b;
}

/* ---------------- hard-disk (FAT) image ---------------- */

static int build_hd_fat(const uint8_t *bb, const uint8_t *drv, uint32_t drv_len,
			const char *out_path, const char *spl_path,
			struct fatfile *files, int nfiles, uint64_t empty_bytes)
{
	if (SECTOR != 512) {
		fprintf(stderr, "--fat requires -s 512 (hard-disk blocks)\n");
		return 2;
	}

	/* self, driver, HFS boot, FAT, and (optionally) a trailing empty one */
	uint32_t empty_sectors = (uint32_t)((empty_bytes + SECTOR - 1) / SECTOR);
	const uint32_t map_entries = empty_sectors ? 5 : 4;
	const uint32_t drv_blk = map_entries + 1;
	const uint32_t hfs_blk = 64;		/* Apple_HFS boot partition */
	const uint32_t fat_blk = 2048;		/* trailing FAT16 partition */
	uint16_t drv_cksum = map_checksum(drv, drv_len);
	uint32_t drv_blks = (drv_len + SECTOR - 1) / SECTOR;

	/* generate the FAT filesystem first so we know the total disk size */
	uint32_t fat_sectors = 0;
	uint8_t *fatfs = build_fat16(files, nfiles, fat_blk, &fat_sectors);
	uint32_t empty_blk = fat_blk + fat_sectors;	/* empty partition start */
	uint32_t total_blocks = empty_blk + empty_sectors;
	uint32_t hfs_blocks = fat_blk - hfs_blk;

	uint8_t *img = calloc(total_blocks, SECTOR);
	if (!img) { perror("calloc"); return 1; }

	/* block 0: DDR declaring one driver */
	uint8_t *dd = img + 0 * SECTOR;
	be16(dd + 0, DD_SIG);
	be16(dd + 2, SECTOR);              /* sbBlkSize */
	be32(dd + 4, total_blocks);        /* sbBlkCount */
	be16(dd + 8, 1);                   /* sbDevType */
	be16(dd + 10, 1);                  /* sbDevId */
	be16(dd + 16, 1);                  /* sbDrvrCount */
	be32(dd + 18, drv_blk);            /* ddBlock */
	be16(dd + 22, drv_blks);           /* ddSize */
	be16(dd + 24, kDriverTypeMacSCSI); /* ddType */

	/* partition map: self, driver, HFS (bootable), FAT data */
	pmap(img + 1 * SECTOR, map_entries, 1, drv_blk - 1,
	     "Apple", "Apple_partition_map", P_VALID|P_ALLOCATED|P_INUSE|P_READABLE);

	uint8_t *pd = img + 2 * SECTOR;
	pmap(pd, map_entries, drv_blk, hfs_blk - drv_blk, "Macintosh", "Apple_Driver43",
	     P_VALID|P_ALLOCATED|P_INUSE|P_BOOTVALID|P_READABLE|P_WRITEABLE|
	     P_BOOTPIC|P_CHAINCOMPAT|P_REALDRIVER);
	be32(pd + 96, drv_len);            /* pmBootSize */
	be32(pd + 116, drv_cksum);         /* pmBootCksum */
	strncpy((char *)pd + 120, "68000", 15);
	be32(pd + 136, kSCSICDDriverSignature);

	pmap(img + 3 * SECTOR, map_entries, hfs_blk, hfs_blocks, "MacOS", "Apple_HFS",
	     P_VALID|P_ALLOCATED|P_INUSE|P_BOOTVALID|P_READABLE|P_WRITEABLE|
	     P_BOOTPIC|P_STARTUP);

	/*
	 * Trailing FAT16 data partition (not a startup volume; the ROM ignores
	 * its type and boots the HFS partition above).  Its Apple partition type
	 * is "U-Boot" so U-Boot's blk_get_device_part_str() accepts it: that code
	 * requires the partition type string to equal BOOT_PART_TYPE ("U-Boot"),
	 * which every other partition driver fills in as a placeholder but the Mac
	 * driver reports verbatim.  fatload probes the filesystem itself, so the
	 * type string does not otherwise matter.
	 */
	pmap(img + 4 * SECTOR, map_entries, fat_blk, fat_sectors, "boot", "U-Boot",
	     P_VALID|P_ALLOCATED|P_INUSE|P_READABLE|P_WRITEABLE);

	/*
	 * Optional trailing empty partition of a user-requested size, for the OS
	 * to format and use later (e.g. a Linux root filesystem).  Its blocks are
	 * left zeroed; type Apple_UNIX_SVR2 is the conventional Apple type for a
	 * Linux/Unix partition on 68k Macs, which Linux/m68k enumerates as a disk.
	 */
	if (empty_sectors)
		pmap(img + 5 * SECTOR, map_entries, empty_blk, empty_sectors,
		     "spare", "Apple_UNIX_SVR2",
		     P_VALID|P_ALLOCATED|P_INUSE|P_READABLE|P_WRITEABLE);

	/* driver code */
	memcpy(img + drv_blk * SECTOR, drv, drv_len);

	/* HFS volume: boot blocks then a minimal MDB */
	uint8_t *hfs = img + hfs_blk * SECTOR;
	memcpy(hfs, bb, 1024);
	uint8_t *mdb = hfs + 1024;
	be16(mdb + 0, HFS_SIG);
	be16(mdb + 10, 0x0100);
	be16(mdb + 18, 512);
	be16(mdb + 24, (hfs_blocks * SECTOR) / (4 * 1024));
	be32(mdb + 26, 4 * 1024);
	be32(mdb + 30, 4 * 1024);
	be16(mdb + 34, 8);
	be32(mdb + 36, 16);
	mdb[44] = 6; memcpy(mdb + 45, "oldmac", 6);

	/* raw SPL image the boot block loads (absolute block 128) */
	uint32_t spl_len = 0;
	uint8_t *spl = slurp(spl_path, &spl_len);
	if (128 * SECTOR + spl_len > fat_blk * SECTOR) {
		fprintf(stderr, "SPL too large for the pre-FAT region\n");
		return 1;
	}
	memcpy(img + 128 * SECTOR, spl, spl_len);
	free(spl);
	fprintf(stderr, "SPL %u bytes at blk 128\n", spl_len);

	/* splice in the FAT filesystem */
	memcpy(img + fat_blk * SECTOR, fatfs, fat_sectors * SECTOR);
	free(fatfs);

	FILE *f = fopen(out_path, "wb");
	if (!f) { perror(out_path); return 1; }
	fwrite(img, SECTOR, total_blocks, f);
	fclose(f);
	fprintf(stderr,
		"%s: HD image, driver %u bytes (cksum %04x), HFS blk %u, "
		"FAT blk %u (%u sectors, %d files)",
		out_path, drv_len, drv_cksum, hfs_blk, fat_blk, fat_sectors,
		nfiles);
	if (empty_sectors)
		fprintf(stderr, ", empty blk %u (%u sectors)",
			empty_blk, empty_sectors);
	fprintf(stderr, ", total %u blocks\n", total_blocks);
	free(img);
	return 0;
}

int main(int argc, char **argv)
{
	const char *pos[8];
	int npos = 0;
	int use_fat = 0;
	struct fatfile files[8];
	int nfiles = 0;
	uint64_t empty_bytes = 0;

	for (int a = 1; a < argc; a++) {
		if (!strcmp(argv[a], "-s") && a + 1 < argc) {
			SECTOR = (unsigned int)strtoul(argv[++a], NULL, 0);
		} else if (!strcmp(argv[a], "--fat")) {
			use_fat = 1;
		} else if (!strcmp(argv[a], "--empty") && a + 1 < argc) {
			empty_bytes = parse_size(argv[++a]);
		} else if (!strcmp(argv[a], "--fatfile") && a + 1 < argc) {
			char *spec = argv[++a];
			char *eq = strchr(spec, '=');
			if (!eq) { fprintf(stderr, "bad --fatfile %s\n", spec); return 2; }
			*eq = '\0';
			if (nfiles >= 8) { fprintf(stderr, "too many fat files\n"); return 2; }
			files[nfiles].name = spec;
			files[nfiles].data = slurp(eq + 1, &files[nfiles].size);
			nfiles++;
		} else if (npos < 8) {
			pos[npos++] = argv[a];
		}
	}

	if (npos < 3) {
		fprintf(stderr,
			"usage: %s [-s sector] [--fat] bootblock.bin driver.bin out "
			"[spl.bin] [payload2 payload3 payload4] [--fatfile NAME=PATH ...]"
			" [--empty SIZE]\n"
			"  --empty SIZE  add an empty partition of SIZE (e.g. 64M, 1G) "
			"after the FAT partition (--fat only)\n",
			argv[0]);
		return 2;
	}

	const char *bb_path = pos[0], *drv_path = pos[1], *out_path = pos[2];
	const char *pl_path  = npos > 3 ? pos[3] : NULL;
	const char *pl2_path = npos > 4 ? pos[4] : NULL;
	const char *pl3_path = npos > 5 ? pos[5] : NULL;
	const char *pl4_path = npos > 6 ? pos[6] : NULL;

	uint8_t bb[1024]; memset(bb, 0, sizeof bb);
	FILE *f = fopen(bb_path, "rb");
	if (!f) { perror(bb_path); return 1; }
	if (fread(bb, 1, sizeof bb, f) == 0) { perror(bb_path); return 1; }
	fclose(f);

	uint8_t *drv = calloc(64, SECTOR); unsigned drv_len = 0;
	f = fopen(drv_path, "rb");
	if (!f) { perror(drv_path); return 1; }
	drv_len = fread(drv, 1, 64 * SECTOR, f); fclose(f);
	uint16_t drv_cksum = map_checksum(drv, drv_len);
	uint32_t drv_blks = (drv_len + SECTOR - 1) / SECTOR;

	if (use_fat)
		return build_hd_fat(bb, drv, drv_len, out_path, pl_path,
				    files, nfiles, empty_bytes);

	/* ---- CD / raw-payload layout (unchanged) ---- */

	const uint32_t map_entries = 3;
	const uint32_t drv_blk = 4;
	const uint32_t hfs_blk = 64;
	const uint32_t total_blocks = 32768;	/* 64 MiB: room for kernel + initramfs */
	const uint32_t hfs_blocks = total_blocks - hfs_blk;

	uint8_t *img = calloc(total_blocks, SECTOR);
	if (!img) { perror("calloc"); return 1; }

	uint8_t *dd = img + 0*SECTOR;
	be16(dd + 0, DD_SIG);
	be16(dd + 2, SECTOR);              /* sbBlkSize */
	be32(dd + 4, total_blocks);        /* sbBlkCount */
	be16(dd + 8, 1);                   /* sbDevType */
	be16(dd + 10, 1);                  /* sbDevId */
	be16(dd + 16, 1);                  /* sbDrvrCount */
	be32(dd + 18, drv_blk);            /* ddBlock (in device blocks) */
	be16(dd + 22, drv_blks);           /* ddSize */
	be16(dd + 24, kDriverTypeMacSCSI); /* ddType */

	pmap(img + 1*SECTOR, map_entries, 1, drv_blk - 1,
	     "Apple", "Apple_partition_map", P_VALID|P_ALLOCATED|P_INUSE|P_READABLE);

	uint8_t *pd = img + 2*SECTOR;
	pmap(pd, map_entries, drv_blk, hfs_blk - drv_blk, "Macintosh", "Apple_Driver43",
	     P_VALID|P_ALLOCATED|P_INUSE|P_BOOTVALID|P_READABLE|P_WRITEABLE|
	     P_BOOTPIC|P_CHAINCOMPAT|P_REALDRIVER);
	be32(pd + 96, drv_len);            /* pmBootSize */
	be32(pd + 100, 0);                 /* pmBootAddr */
	be32(pd + 116, drv_cksum);         /* pmBootCksum */
	strncpy((char *)pd + 120, "68000", 15);  /* pmProcessor */
	be32(pd + 136, kSCSICDDriverSignature);  /* pmPad[0]: driver signature */

	pmap(img + 3*SECTOR, map_entries, hfs_blk, hfs_blocks, "MacOS", "Apple_HFS",
	     P_VALID|P_ALLOCATED|P_INUSE|P_BOOTVALID|P_READABLE|P_WRITEABLE|
	     P_BOOTPIC|P_STARTUP);

	memcpy(img + drv_blk*SECTOR, drv, drv_len);

	uint8_t *hfs = img + hfs_blk*SECTOR;
	memcpy(hfs, bb, 1024);
	uint8_t *mdb = hfs + 1024;
	be16(mdb + 0, HFS_SIG);                       /* drSigWord 'BD' */
	be16(mdb + 10, 0x0100);                       /* drAtrb */
	be16(mdb + 18, 512);                          /* drVBMSt */
	be16(mdb + 24, (hfs_blocks*SECTOR)/(4*1024)); /* drNmAlBlks */
	be32(mdb + 26, 4*1024);                       /* drAlBlkSiz */
	be32(mdb + 30, 4*1024);                       /* drClpSiz */
	be16(mdb + 34, 8);                            /* drAlBlSt */
	be32(mdb + 36, 16);                           /* drNxtCNID */
	mdb[44] = 6; memcpy(mdb + 45, "oldmac", 6);   /* drVN */

	const uint32_t payload_blk = 128;	/* boot block reads this */
	const uint32_t payload2_blk = 2048;	/* SPL reads this (U-Boot proper) */
	const uint32_t payload3_blk = 4096;	/* U-Boot reads this (e.g. a kernel) */
	const uint32_t payload4_blk = 8192;	/* U-Boot reads this (e.g. an initramfs) */
	if (pl_path) {
		FILE *pf = fopen(pl_path, "rb");
		if (!pf) { perror(pl_path); return 1; }
		size_t pn = fread(img + payload_blk*SECTOR, 1,
				  (payload2_blk - payload_blk)*SECTOR, pf);
		fclose(pf);
		fprintf(stderr, "payload %zu bytes at blk %u\n", pn, payload_blk);
	}
	if (pl2_path) {
		FILE *pf = fopen(pl2_path, "rb");
		if (!pf) { perror(pl2_path); return 1; }
		size_t pn = fread(img + payload2_blk*SECTOR, 1,
				  (payload3_blk - payload2_blk)*SECTOR, pf);
		fclose(pf);
		fprintf(stderr, "payload2 %zu bytes at blk %u\n", pn, payload2_blk);
	}
	if (pl3_path) {
		FILE *pf = fopen(pl3_path, "rb");
		if (!pf) { perror(pl3_path); return 1; }
		size_t pn = fread(img + payload3_blk*SECTOR, 1,
				  (payload4_blk - payload3_blk)*SECTOR, pf);
		fclose(pf);
		fprintf(stderr, "payload3 %zu bytes at blk %u (%zu blocks)\n",
			pn, payload3_blk, (pn + SECTOR - 1) / SECTOR);
	}
	uint32_t initrd_bytes = 0;
	if (pl4_path) {
		FILE *pf = fopen(pl4_path, "rb");
		if (!pf) { perror(pl4_path); return 1; }
		size_t pn = fread(img + payload4_blk*SECTOR, 1,
				  (total_blocks - payload4_blk)*SECTOR, pf);
		fclose(pf);
		initrd_bytes = (uint32_t)pn;
		fprintf(stderr, "payload4 %zu bytes at blk %u (%zu blocks)\n",
			pn, payload4_blk, (pn + SECTOR - 1) / SECTOR);
	}

	/*
	 * Boot descriptor at a fixed block: lets U-Boot's boot script read the
	 * exact initramfs byte size (legacy-lz4 needs it precisely) and block
	 * count at runtime, so swapping the initramfs needs no U-Boot rebuild.
	 * Layout (big-endian): magic 'OMBD', initramfs bytes, initramfs blocks.
	 */
	{
		const uint32_t desc_blk = 96;
		uint8_t *desc = img + desc_blk * SECTOR;
		uint32_t blks = initrd_bytes ?
				(initrd_bytes + SECTOR - 1) / SECTOR : 1;

		be32(desc + 0, 0x4F4D4244);	/* 'OMBD' */
		be32(desc + 4, initrd_bytes);
		be32(desc + 8, blks);
	}

	f = fopen(out_path, "wb");
	if (!f) { perror(out_path); return 1; }
	fwrite(img, SECTOR, total_blocks, f);
	fclose(f);
	fprintf(stderr, "%s: driver %u bytes (%u blks, cksum %04x), HFS at blk %u\n",
		out_path, drv_len, drv_blks, drv_cksum, hfs_blk);
	free(img); free(drv);
	return 0;
}
