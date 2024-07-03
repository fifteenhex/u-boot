#include <init.h>
#include <asm/global_data.h>
#include <asm/spl.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <fdtdec.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <serial.h>
#include <asm/bootinfo.h>
#include <asm/bootinfo-vme.h>

DECLARE_GLOBAL_DATA_PTR;

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_SATA;
}

int print_cpuinfo(void)
{
	return 0;
}

void cpu_init_f(void)
{

}

int dram_init(void)
{
	/* Correct size will be in the DT but this is still too early. */
	gd->ram_size = 0x1000000;

	return 0;
}

void reset_cpu(void)
{
	while (true){

	}
}

int board_fix_fdt(void *fdt)
{
	return 0;
}

int board_early_init_r(void)
{
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
	icache_enable();
	dcache_enable();

	return 0;
}

struct bi_record* m68k_get_mach(struct bi_record *r)
{
	/* Set the machine */
	r->tag = BI_MACHTYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = MACH_MVME147;

	/* VME board type */
	r = ((void *) r) + r->size;
	r->tag = BI_VME_TYPE;
	r->size = sizeof(*r) + 4;
	r->data[0] = VME_TYPE_MVME147;

	return r;
}
