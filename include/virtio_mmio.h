/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __VIRTIO_MMIO_H__
#define __VIRTIO_MMIO_H__

#include <linux/types.h>

#define VIRTIO_MMIO_VENDOR_QEMU	0x554d4551

struct virtio_mmio_plat {
	phys_addr_t base;
};

#endif /* __VIRTIO_MMIO_H__ */
