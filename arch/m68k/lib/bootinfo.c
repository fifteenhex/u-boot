// SPDX-License-Identifier: GPL-2.0
/*
 *  Based on the following files from Linux:
 *  arch/m68k/kernel/setup_mm.c - Copyright (C) 1995  Hamish Macdonald
 *  arch/m68k/virt/config.c
 */

#include <asm/global_data.h>
#include <asm/sections.h>
#include <asm/byteorder.h>
#include <asm/bootinfo.h>
#include <asm/bootinfo-virt.h>
#include <asm/virt.h>
#include <linux/printk.h>

DECLARE_GLOBAL_DATA_PTR;

static struct m68k_mem_info m68k_ramdisk;
static char m68k_command_line[256 /*COMMAND_LINE_SIZE*/];

#define MACH_IS_VIRT (m68k_machtype == MACH_VIRT)

struct m68k_mem_info {
	unsigned long addr;		/* physical address of memory chunk */
	unsigned long size;		/* length of memory chunk (in bytes) */
};

/*
 * Parse a virtual-m68k-specific record in the bootinfo
 */

static int virt_parse_bootinfo(const struct bi_record *record, void *fdt)
{
	int unknown = 0;
	const void *data = record->data;
	struct virt_booter_data virt_bi_data;

	switch (be16_to_cpu(record->tag)) {
	case BI_VIRT_QEMU_VERSION:
		virt_bi_data.qemu_version = be32_to_cpup(data);
		break;
	case BI_VIRT_GF_PIC_BASE:
		virt_bi_data.pic.mmio = be32_to_cpup(data);
		data += 4;
		virt_bi_data.pic.irq = be32_to_cpup(data);
		break;
	case BI_VIRT_GF_RTC_BASE:
		virt_bi_data.rtc.mmio = be32_to_cpup(data);
		data += 4;
		virt_bi_data.rtc.irq = be32_to_cpup(data);
		break;
	case BI_VIRT_GF_TTY_BASE:
		virt_bi_data.tty.mmio = be32_to_cpup(data);
		data += 4;
		virt_bi_data.tty.irq = be32_to_cpup(data);
		break;
	case BI_VIRT_CTRL_BASE:
		virt_bi_data.ctrl.mmio = be32_to_cpup(data);
		data += 4;
		virt_bi_data.ctrl.irq = be32_to_cpup(data);
		break;
	case BI_VIRT_VIRTIO_BASE:
		virt_bi_data.virtio.mmio = be32_to_cpup(data);
		data += 4;
		virt_bi_data.virtio.irq = be32_to_cpup(data);
		{
			int i, last_virtio = -1, ret;
			fdt32_t newvalue[] = {
					0,
					cpu_to_fdt32(0x200),
			};
			for (i = 0; i < 128; i++) {
				/* Fill in the FDT */
				last_virtio = fdt_node_offset_by_compatible(fdt, last_virtio, "virtio,mmio");

				if (last_virtio < 0)
					break;

				newvalue[0] = virt_bi_data.virtio.mmio + (0x200 * i);

				ret = fdt_setprop_inplace(fdt, last_virtio, "reg", &newvalue, sizeof(newvalue));
				if (ret)
					panic("Couldn't set reg value");
			}
		}
		break;
	default:
		unknown = 1;
		break;
	}
	return unknown;
}

static void fix_cputype(unsigned long cputype, void *fdt)
{
	int node, intc_node;
	int ret;
	char *newvalue;
	char *intc_68010 = "motorola,mc68010-intc-vect";

	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "motorola,mc68000");
	if (node < 0)
		panic("Failed to find cpu node\n");

	intc_node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "motorola,mc68000-intc-vect");
	if (intc_node < 0)
		panic("Failed to find intc node\n");

	switch(cputype) {
	case CPU_68000:
		return;
	case CPU_68010:
		newvalue = "motorola,mc68010";
		break;
	case CPU_68020:
		newvalue = "motorola,mc68020";
		break;
	case CPU_68030:
		newvalue = "motorola,mc68030";
		break;
	case CPU_68040:
		newvalue = "motorola,mc68040";
		break;
	case CPU_68060:
		newvalue = "motorola,mc68060";
		break;
	default:
		panic("Unknown CPU type: %lu\n", cputype);
		break;
	}

	ret = fdt_setprop_inplace(fdt, node, "compatible", newvalue, strlen(newvalue)+1);
	if (ret)
		panic("Failed to update cpu compatible: %d\n", ret);

	fdt_setprop_inplace(fdt, intc_node, "compatible", intc_68010, strlen(intc_68010)+1);
}

static void m68k_parse_bootinfo(const struct bi_record *record, void *fdt)
{
	const struct bi_record *first_record = record;
	uint16_t tag;
	int m68k_num_memory = 0;

	while ((tag = be16_to_cpu(record->tag)) != BI_LAST) {
		int unknown = 0;
		const void *data = record->data;
		uint16_t size = be16_to_cpu(record->size);
		unsigned long m68k_machtype, cputype;

		switch (tag) {
		case BI_MACHTYPE:
			m68k_machtype = be32_to_cpup(record->data);
			break;

		case BI_CPUTYPE:
			cputype = be32_to_cpup(record->data);
			fix_cputype(cputype, fdt);
		case BI_FPUTYPE:
		case BI_MMUTYPE:
			break;

		case BI_MEMCHUNK:
			{
				int ret;
				int mem_node = fdt_path_offset(fdt, "/memory");
				const struct mem_info *m = data;
				unsigned long addr = be32_to_cpu(m->addr);
				unsigned long size = be32_to_cpu(m->size);
				fdt32_t newvalue[] = {
						cpu_to_fdt32(addr),
						cpu_to_fdt32(size),
				};

				if (mem_node < 0)
					break;

				if (m68k_num_memory)
					break;

				m68k_num_memory++;

				ret = fdt_setprop_inplace(fdt, mem_node, "reg",
						&newvalue, sizeof(newvalue));
				if (ret)
					panic("Couldn't fix up memory\n");
			}
			break;

		case BI_RAMDISK:
			{
				const struct mem_info *m = data;
				m68k_ramdisk.addr = be32_to_cpu(m->addr);
				m68k_ramdisk.size = be32_to_cpu(m->size);
			}
			break;

		case BI_COMMAND_LINE:
			//strscpy(m68k_command_line, data,
			//	sizeof(m68k_command_line));
			break;

		case BI_RNG_SEED: {
#if 0
			u16 len = be16_to_cpup(data);
			add_bootloader_randomness(data + 2, len);
			/*
			 * Zero the data to preserve forward secrecy, and zero the
			 * length to prevent kexec from using it.
			 */
			memzero_explicit((void *)data, len + 2);
#endif
			break;
		}

		default:
			if (MACH_IS_VIRT)
				unknown = virt_parse_bootinfo(record, fdt);
			else
				unknown = 1;
		}
		if (unknown)
			pr_warn("%s: unknown tag 0x%04x ignored\n", __func__,
				tag);
		record = (struct bi_record *)((unsigned long)record + size);
	}

#if 0
	save_bootinfo(first_record);
#endif
}

size_t bootinfo_memsz_f(void)
{
	const struct bi_record *bi = (const struct bi_record *) _end;
	uint16_t tag;

	while ((tag = be16_to_cpu(bi->tag)) != BI_LAST) {
		uint16_t n = be16_to_cpu(bi->size);

		if (tag == BI_MEMCHUNK) {
			const struct mem_info *m = bi->data;
			return be32_to_cpu(m->size);
		}

		bi = (struct bi_record *)((unsigned long)bi + n);
	}

	printf("Did not find memory size!\n");

	return 0;
}

size_t sizeof_bootinfo(void)
{
	const struct bi_record *bi = (const struct bi_record *) _end;

	size_t size = sizeof(bi->tag);

	while (be16_to_cpu(bi->tag) != BI_LAST) {
		uint16_t n = be16_to_cpu(bi->size);
		/* No size, probably not valid bootinfo */
		if (!n)
			return 0;

		size += n;

		/* Avoid infinite loops over bad bootinfo */
		if(size > 512)
			return 0;

		bi = (struct bi_record *)((unsigned long)bi + n);
	}

	return size;
}

size_t save_bootinfo(void *dst)
{
	const struct bi_record *bi = (const struct bi_record *) _end;

	const void *start = bi;
	size_t size = sizeof(bi->tag);

	while (be16_to_cpu(bi->tag) != BI_LAST) {
		uint16_t n = be16_to_cpu(bi->size);
		size += n;
		bi = (struct bi_record *)((unsigned long)bi + n);
	}

	debug("Saving %zu bytes of bootinfo\n", size);
	memcpy(dst, start, size);

	return size;
};

extern struct bi_record* m68k_get_mach(struct bi_record *r);
static struct bi_record* m68k_bootinfo_030(struct bi_record *r)
{
	/* Set the CPU */
	r = ((void *) r) + r->size;
	r->tag = BI_CPUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = CPU_68030;

	/* Set the MMU */
	r = ((void *) r) + r->size;
	r->tag = BI_MMUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = MMU_68030;

	/* Set the FPU */
	r = ((void *) r) + r->size;
	r->tag = BI_FPUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = FPU_68882;

	return r;
}

static struct bi_record* m68k_bootinfo_040(struct bi_record *r)
{
	/* Set the CPU */
	r = ((void *) r) + r->size;
	r->tag = BI_CPUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = CPU_68040;

	/* Set the MMU */
	r = ((void *) r) + r->size;
	r->tag = BI_MMUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = MMU_68040;

	/* Set the FPU */
	r = ((void *) r) + r->size;
	r->tag = BI_FPUTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = FPU_68040;

	return r;
}
void m68k_create_bootinfo(void *dst)
{
	struct bi_record *r = (struct bi_record *) dst;

	r = m68k_get_mach(r);

#ifdef CONFIG_MC68030
	r = m68k_bootinfo_030(r);
#elif CONFIG_MC68040
	r = m68k_bootinfo_040(r);
#else
#error
#endif

	/* Memory info */
	r = ((void *) r) + r->size;
	r->tag = BI_MEMCHUNK;
	r->size = sizeof(*r) + 8;
	r->data[0] = 0x0;
	r->data[1] = gd->ram_size;

	/* initramfs */
	r = ((void *) r) + r->size;
	r->tag = BI_RAMDISK;
	r->size = sizeof(*r) + 8;
	r->data[0] = 0xb00000;
	r->data[1] = 0x300000;

	/* devicetree */
	r = ((void *) r) + r->size;
	r->tag = BI_FDT;
	r->size = sizeof(*r) + 4;
	r->data[0] = gd->fdt_blob;

	/* Terminator */
	r = ((void *) r) + r->size;
	r->tag = BI_LAST;
	r->size = sizeof(*r);
}

void bootinfo_fix_fdt(void *fdt)
{
	m68k_parse_bootinfo(gd->bootinfo, fdt);
};
