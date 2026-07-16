#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Assemble the oldmac Mac boot block + SCSI boot driver and wrap them, together
# with a raw payload (U-Boot proper or the SPL), into a bootable Macintosh CD
# image using the mkoldmaccd host tool.
#
# The boot block is linked against the payload's final layout, which is only
# known after the payload ELF is built, so its build-specific addresses are
# extracted from nm here and injected with --defsym (this is what makes `make`
# able to emit a matching image).
#
# Usage:
#   mkcd.sh AS OBJCOPY NM TOOL SECTOR PAYLOAD_ELF PAYLOAD_BIN BOOTBLOCK_S \
#           DRIVER_S OUT [PAYLOAD2_BIN]
#
# PAYLOAD2_BIN, if given, is placed at a second fixed LBA for the SPL to load
# (e.g. U-Boot proper when the SPL loads it off this same CD).
set -e

AS="$1"; OBJCOPY="$2"; NM="$3"; TOOL="$4"; SECTOR="$5"
PAYLOAD_ELF="$6"; PAYLOAD_BIN="$7"; BOOTBLOCK_S="$8"; DRIVER_S="$9"; OUT="${10}"
PAYLOAD2_BIN="${11}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Extract the U-Boot layout the boot block links against.  The SPL boot block
# ignores these and uses its own .ifndef defaults (fixed SPL text base).
img_end=$("$NM" "$PAYLOAD_ELF" | awk '/ __image_copy_end$/{print $1}')
entry=$(  "$NM" "$PAYLOAD_ELF" | awk '/ _start$/{print $1}')
bss_end=$("$NM" "$PAYLOAD_ELF" | awk '/ _end$/{print $1}')

# Number of CD blocks (SECTOR bytes) needed to hold the payload, so the boot
# block reads the whole (growing) image rather than a fixed-size prefix.
payload_bytes=$(wc -c < "$PAYLOAD_BIN")
blocks=$(( (payload_bytes + SECTOR - 1) / SECTOR ))

"$AS" -mcpu=68040 \
	--defsym UBOOT_SIZE=0x${img_end:-0} \
	--defsym UBOOT_ENTRY=0x${entry:-400} \
	--defsym BOOTINFO_DEST=0x${bss_end:-0} \
	--defsym UBOOT_BLOCKS=$blocks \
	"$BOOTBLOCK_S" -o "$tmp/bootblock.o"
"$OBJCOPY" -O binary "$tmp/bootblock.o" "$tmp/bootblock.bin"

"$AS" -mcpu=68040 "$DRIVER_S" -o "$tmp/driver.o"
"$OBJCOPY" -O binary "$tmp/driver.o" "$tmp/driver.bin"

"$TOOL" -s "$SECTOR" "$tmp/bootblock.bin" "$tmp/driver.bin" "$OUT" \
	"$PAYLOAD_BIN" $PAYLOAD2_BIN
