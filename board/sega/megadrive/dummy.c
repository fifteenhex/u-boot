#include <init.h>
#include <asm/global_data.h>
#include <asm/spl.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <serial.h>

#include "vdp.h"

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
	gd->ram_size = 0x300000;

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
	return 0;
}

int board_early_init_r(void)
{
	vdp_init();

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
