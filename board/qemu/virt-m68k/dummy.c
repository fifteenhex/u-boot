#include <init.h>
#include <asm/global_data.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <fdtdec.h>

DECLARE_GLOBAL_DATA_PTR;

int print_cpuinfo(void)
{
	return 0;
}

void cpu_init_f(void)
{

}

static const void *get_memory_reg_prop(const void *fdt, int *lenp)
{
	int offset;

	offset = fdt_path_offset(fdt, "/memory");
	if (offset < 0)
		return NULL;

	return fdt_getprop(fdt, offset, "reg", lenp);
}

size_t bootinfo_memsz_f(void);
int dram_init(void)
{
	/* Correct size will be in the DT but this is still too early. */
	gd->ram_size = bootinfo_memsz_f();

	return 0;
}

extern void qemu_virt_ctrl_halt(struct udevice *dev);
void reset_cpu(void)
{
	struct udevice *virt_ctrl = gd->virt_ctrl;

	if (virt_ctrl) {
		printf("Halting via virt-ctrl\n");
		qemu_virt_ctrl_halt(virt_ctrl);
	}
	else
		printf("No virt-ctrl to trigger halt\n");

	while (true){

	}
}

extern void bootinfo_fix_fdt(void *fdt);

int board_fix_fdt(void *fdt)
{
	bootinfo_fix_fdt(fdt);

	return 0;
}

int board_early_init_r(void)
{
	struct udevice *devp;
	int node, ret;

	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "qemu,virt-ctrl");

	ret = uclass_get_device_by_of_offset(UCLASS_MISC, node, &devp);
	if (ret)
		return ret;

	gd->virt_ctrl = devp;

	return 0;
}

int get_clocks(void)
{
	return 0;
}

int interrupt_init(void)
{
	return 0;
}


int cpu_init_r(void)
{
	return 0;
}
