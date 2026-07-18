#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Assemble the oldmac Mac boot block + SCSI boot driver and wrap them, together
# with the SPL and U-Boot proper, into a bootable Macintosh image using the
# mkoldmaccd host tool.  Two layouts:
#
#   CD (default): the boot block loads the SPL off the CD; the SPL then loads
#   U-Boot proper (and the kernel/initramfs) from further raw LBAs on the CD.
#
#   hard disk (--fat, SECTOR=512): the boot block loads the SPL, then everything
#   after it - U-Boot proper, kernel, initramfs - lives in a real FAT16
#   filesystem in a trailing Apple partition, loaded by name.
#
# Usage:
#   mkcd.sh [--fat] AS OBJCOPY TOOL SECTOR SPL_BIN BOOTBLOCK_S DRIVER_S OUT \
#           UBOOT [KERNEL] [INITRD]
#
# CD mode:  UBOOT = u-boot.bin (raw, read by the SPL); KERNEL/INITRD raw LBAs.
# --fat:    UBOOT = u-boot.bin (loaded from FAT as "u-boot.bin"); KERNEL and
#           INITRD become the FAT files "vmlinux" and "initrd".
set -e

FAT=0
if [ "$1" = "--fat" ]; then FAT=1; shift; fi

AS="$1"; OBJCOPY="$2"; TOOL="$3"; SECTOR="$4"
SPL_BIN="$5"; BOOTBLOCK_S="$6"; DRIVER_S="$7"; OUT="$8"; UBOOT="$9"
KERNEL="${10}"; INITRD="${11}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Size the boot block's SPL load to the actual SPL image (copy length rounded up
# to a longword, and the device-block count needed to hold it).
spl_bytes=$(wc -c < "$SPL_BIN")
spl_size=$(( (spl_bytes + 3) / 4 * 4 ))
spl_blocks=$(( (spl_bytes + SECTOR - 1) / SECTOR ))

# -I so the boot block's `.include "macbootinfo.inc"` is found.  SECTOR makes the
# boot block and driver read the right block size (2048 CD, 512 HD).
"$AS" -mcpu=68040 -I "$(dirname "$BOOTBLOCK_S")" \
	--defsym SPL_SIZE=$spl_size \
	--defsym PAYLOAD_BLOCKS=$spl_blocks \
	--defsym SECTOR=$SECTOR \
	"$BOOTBLOCK_S" -o "$tmp/bootblock.o"
"$OBJCOPY" -O binary "$tmp/bootblock.o" "$tmp/bootblock.bin"

"$AS" -mcpu=68040 --defsym SECTOR=$SECTOR "$DRIVER_S" -o "$tmp/driver.o"
"$OBJCOPY" -O binary "$tmp/driver.o" "$tmp/driver.bin"

if [ "$FAT" = 1 ]; then
	# Hard-disk image: SPL is a raw payload the boot block loads; U-Boot
	# proper, the kernel and the initramfs are files in the FAT partition.
	# OLDMAC_SPARE (e.g. 64M) adds an empty partition after the FAT one.
	set -- --fat -s "$SECTOR" "$tmp/bootblock.bin" "$tmp/driver.bin" "$OUT" \
		"$SPL_BIN" --fatfile "u-boot.bin=$UBOOT"
	[ -n "$KERNEL" ] && set -- "$@" --fatfile "vmlinux=$KERNEL"
	[ -n "$INITRD" ] && set -- "$@" --fatfile "initrd=$INITRD"
	[ -n "$OLDMAC_SPARE" ] && set -- "$@" --empty "$OLDMAC_SPARE"
	"$TOOL" "$@"
else
	# CD: payload at the first LBA = the SPL (read by the boot block); the
	# second = U-Boot proper (read by the SPL); optional third/fourth = a
	# Linux kernel and initramfs at further raw LBAs.
	"$TOOL" -s "$SECTOR" "$tmp/bootblock.bin" "$tmp/driver.bin" "$OUT" \
		"$SPL_BIN" "$UBOOT" $KERNEL $INITRD
fi
