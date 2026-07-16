// SPDX-License-Identifier: GPL-2.0+
/*
 * mkoldmaccd - hand-crafted bootable Macintosh CD/disk image builder.
 *
 * Emits, with no external ISO/HFS tooling, the exact bytes the Apple ROM needs
 * to find and boot our own code off a SCSI CD-ROM (or disk) on a classic 68k
 * Macintosh such as the Quadra 800:
 *
 *   - block 0            : Apple Driver Descriptor Record ('ER'), declaring a driver
 *   - partition map      : 'PM' entries: self, Apple_Driver43, Apple_HFS (bootable)
 *   - Apple_Driver43     : our own minimal Mac disk driver (driver.bin)
 *   - Apple_HFS volume   : boot blocks (our 1024-byte 'LK' block) + HFS MDB
 *
 * Structures/fields referenced from "Inside Macintosh: Files/Devices".  All
 * on-disk fields are big-endian (m68k).
 *
 * Empirically, the ROM only boots media whose DDR declares a loadable driver;
 * it then loads the driver and uses it to read the volume.
 *
 * Usage: mkoldmaccd [-s sector] bootblock.bin driver.bin out.iso [payload.bin]
 *   -s sector   logical block size (2048 for CD-ROM, 512 for hard disk)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned int SECTOR = 2048;	/* logical (partition-map) block size */

/* ------- big-endian store helpers ------- */
static void be16(void *p, uint16_t v){ uint8_t*b=p; b[0]=v>>8; b[1]=v; }
static void be32(void *p, uint32_t v){ uint8_t*b=p; b[0]=v>>24; b[1]=v>>16; b[2]=v>>8; b[3]=v; }

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

int main(int argc, char **argv)
{
	const char *bb_path, *drv_path, *out_path, *pl_path = NULL;
	int a = 1;

	if (a + 1 < argc && !strcmp(argv[a], "-s")) {
		SECTOR = (unsigned int)strtoul(argv[a + 1], NULL, 0);
		a += 2;
	}
	if (argc - a < 3) {
		fprintf(stderr,
			"usage: %s [-s sector] bootblock.bin driver.bin out.iso [payload.bin]\n",
			argv[0]);
		return 2;
	}
	bb_path  = argv[a++];
	drv_path = argv[a++];
	out_path = argv[a++];
	if (a < argc)
		pl_path = argv[a++];

	uint8_t bb[1024]; memset(bb, 0, sizeof bb);
	FILE *f = fopen(bb_path, "rb");
	if (!f) { perror(bb_path); return 1; }
	if (fread(bb, 1, sizeof bb, f) == 0) { perror(bb_path); return 1; }
	fclose(f);

	uint8_t *drv = calloc(64, SECTOR); unsigned drv_len = 0;
	f = fopen(drv_path, "rb");
	if (!f) { perror(drv_path); return 1; }
	drv_len = fread(drv, 1, 64*SECTOR, f); fclose(f);
	uint16_t drv_cksum = map_checksum(drv, drv_len);
	uint32_t drv_blks = (drv_len + SECTOR - 1) / SECTOR;

	/* ---- layout in logical blocks ----
	 *  blk 0        : DDR (declares driver at blk drv_blk)
	 *  blk 1..3     : partition map (self, Apple_Driver43, Apple_HFS)
	 *  blk drv_blk  : driver code
	 *  blk hfs_blk  : Apple_HFS: boot blocks + MDB
	 *  blk 128      : raw payload (u-boot.bin or the SPL), read by the boot block
	 */
	const uint32_t map_entries = 3;
	const uint32_t drv_blk = 4;
	const uint32_t hfs_blk = 64;
	const uint32_t total_blocks = 16384;
	const uint32_t hfs_blocks = total_blocks - hfs_blk;

	uint8_t *img = calloc(total_blocks, SECTOR);
	if (!img) { perror("calloc"); return 1; }

	/* block 0: DDR declaring one driver */
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

	/* partition map */
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

	/* driver code */
	memcpy(img + drv_blk*SECTOR, drv, drv_len);

	/* HFS volume: boot blocks then a minimal MDB */
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

	/* raw payload at the fixed block the boot block reads */
	const uint32_t payload_blk = 128;
	if (pl_path) {
		FILE *pf = fopen(pl_path, "rb");
		if (!pf) { perror(pl_path); return 1; }
		size_t pn = fread(img + payload_blk*SECTOR, 1,
				  (total_blocks - payload_blk)*SECTOR, pf);
		fclose(pf);
		fprintf(stderr, "payload %zu bytes at blk %u\n", pn, payload_blk);
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
