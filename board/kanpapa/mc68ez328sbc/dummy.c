#include <init.h>
#include <asm/global_data.h>
#include <asm/spl.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <serial.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_UART;
}

int print_cpuinfo(void)
{
	return 0;
}

void cpu_init_f(void)
{

}

size_t bootinfo_memsz_f(void);
int dram_init(void)
{
	/* Correct size will be in the DT but this is still too early. */
	//gd->ram_size = bootinfo_memsz_f();
	gd->ram_size = 0x800000;

	return 0;
}

void reset_cpu(void)
{
	while (true){

	}
}

extern void bootinfo_fix_fdt(void *fdt);

int board_fix_fdt(void *fdt)
{
	bootinfo_fix_fdt(fdt);

	return 0;
}

extern int dragonball_pll_beastmode(struct udevice *dev);

int board_early_init_r(void)
{
	struct udevice *devp;
	int node, ret;

	/* Lets get things cooking, bump up the cpu clock... */
	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "motorola,mc68ez328-pll");
	ret = uclass_get_device_by_of_offset(UCLASS_CLK, node, &devp);
	if (!ret) {
		dragonball_pll_beastmode(devp);
		/* Redo baud rate calculation since sysclk has changed */
		serial_setbrg();
	}

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
