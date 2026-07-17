#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Assemble the oldmac Mac boot block + SCSI boot driver and wrap them, together
# with the SPL and U-Boot proper, into a bootable Macintosh CD image using the
# mkoldmaccd host tool.
#
# The boot block loads the SPL off the CD; the SPL then loads U-Boot proper from
# a second LBA on the same CD.  The boot block's SPL read length is derived from
# the freshly built SPL here so it always matches the build.
#
# Usage:
#   mkcd.sh AS OBJCOPY TOOL SECTOR SPL_BIN BOOTBLOCK_S DRIVER_S OUT UBOOT_BIN \
#           [KERNEL] [INITRD]
#
# The optional KERNEL (a raw ELF vmlinux) is placed at a third LBA and the
# optional INITRD (a raw initramfs, e.g. cpio.lz4) at a fourth, which U-Boot
# proper `scsi read`s and hands to Linux (bootelf + BI_RAMDISK) off the same CD.
set -e

AS="$1"; OBJCOPY="$2"; TOOL="$3"; SECTOR="$4"
SPL_BIN="$5"; BOOTBLOCK_S="$6"; DRIVER_S="$7"; OUT="$8"; UBOOT_BIN="$9"
KERNEL="${10}"; INITRD="${11}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Size the boot block's SPL load to the actual SPL image (copy length rounded up
# to a longword, and the CD-block count needed to hold it).
spl_bytes=$(wc -c < "$SPL_BIN")
spl_size=$(( (spl_bytes + 3) / 4 * 4 ))
spl_blocks=$(( (spl_bytes + SECTOR - 1) / SECTOR ))

# -I so the boot block's `.include "macbootinfo.inc"` is found.
"$AS" -mcpu=68040 -I "$(dirname "$BOOTBLOCK_S")" \
	--defsym SPL_SIZE=$spl_size \
	--defsym PAYLOAD_BLOCKS=$spl_blocks \
	"$BOOTBLOCK_S" -o "$tmp/bootblock.o"
"$OBJCOPY" -O binary "$tmp/bootblock.o" "$tmp/bootblock.bin"

"$AS" -mcpu=68040 "$DRIVER_S" -o "$tmp/driver.o"
"$OBJCOPY" -O binary "$tmp/driver.o" "$tmp/driver.bin"

# Payload at the first LBA = the SPL (read by the boot block); the second =
# U-Boot proper (read by the SPL); the optional third/fourth = a Linux kernel
# and initramfs.  mkoldmaccd needs the kernel present to place the initramfs, so
# only pass the initramfs when a kernel was given too.
"$TOOL" -s "$SECTOR" "$tmp/bootblock.bin" "$tmp/driver.bin" "$OUT" \
	"$SPL_BIN" "$UBOOT_BIN" $KERNEL $INITRD
