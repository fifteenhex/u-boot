/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025, Kuan-Wei Chiu <visitorckw@gmail.com>
 *
 * Definitions for the m68k bootinfo interface.
 */

#ifndef _ASM_M68K_BOOTINFO_H
#define _ASM_M68K_BOOTINFO_H

#ifndef __ASSEMBLY__

struct bi_record {
	unsigned short tag;        /* tag ID */
	unsigned short size;       /* size of record (in bytes) */
	unsigned long data[0];     /* data */
};

#endif /* __ASSEMBLY__ */

/* Bootinfo Tag IDs */
#define BI_LAST            0x0000
#define BI_MACHTYPE        0x0001
#define BI_CPUTYPE         0x0002
#define BI_FPUTYPE         0x0003
#define BI_MMUTYPE         0x0004
#define BI_MEMCHUNK        0x0005
#define BI_RAMDISK         0x0006
#define BI_COMMAND_LINE    0x0007

/* QEMU virt specific tags */
#define BI_VIRT_QEMU_VERSION    0x8000
#define BI_VIRT_GF_PIC_BASE     0x8001
#define BI_VIRT_GF_RTC_BASE     0x8002
#define BI_VIRT_GF_TTY_BASE     0x8003
#define BI_VIRT_VIRTIO_BASE     0x8004
#define BI_VIRT_CTRL_BASE       0x8005

/*
 * Macintosh specific tags.  These share the machine-specific 0x8000+ range with
 * the virt tags above; only one machine type is active in a given boot.
 */
#define BI_MAC_MODEL            0x8000  /* Mac Gestalt model id */
#define BI_MAC_VADDR            0x8001  /* Mac video base address */
#define BI_MAC_VDEPTH           0x8002  /* Mac video depth */
#define BI_MAC_VROW             0x8003  /* Mac video rowbytes */
#define BI_MAC_VDIM             0x8004  /* Mac video dimensions (h << 16 | w) */
#define BI_MAC_VLOGICAL         0x8005  /* Mac video logical base */
#define BI_MAC_SCCBASE          0x8006  /* Mac SCC base address */
#define BI_MAC_BTIME            0x8007  /* Mac boot time */
#define BI_MAC_GMTBIAS          0x8008  /* Mac GMT timezone offset */
#define BI_MAC_MEMSIZE          0x8009  /* Mac RAM size (MiB, sanity check) */
#define BI_MAC_CPUID            0x800a  /* Mac CPU type (sanity check) */
#define BI_MAC_ROMBASE          0x800b  /* Mac system ROM base address */

/* Macintosh Gestalt model numbers (BI_MAC_MODEL) */
#define MAC_MODEL_Q700          22
#define MAC_MODEL_Q800          35

/*
 * Fixed physical address at which the oldmac boot block leaves the Mac bootinfo
 * record list for U-Boot (and the SPL) to read.  It sits in the 0x500000..
 * 0x700000 RAM gap the boot blocks preserve.  Keep in sync with MAC_BOOTINFO in
 * board/apple/oldmac/macboot/macbootinfo.inc.
 */
#define MAC_BOOTINFO_ADDR       0x00501000

#endif /* _ASM_M68K_BOOTINFO_H */
