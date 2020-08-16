#include <common.h>
#include <dm.h>
#include <ram.h>

DECLARE_GLOBAL_DATA_PTR;

int print_cpuinfo(void) {
	return 0;
}

int dram_init(void) {
	gd->ram_size = 0x4000000;
	return 0;
}

void reset_cpu(ulong addr) {
	// magic from the vendor sdk
	volatile u16* resetreg = (u16*) 0x1f001cb8;
	*resetreg = 0x00FF;
	*resetreg = 0x0079;
	while(true){

	}
}
