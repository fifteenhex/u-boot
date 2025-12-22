#include <cache_mc68030.h>
#include <cache_mc68040.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

static inline bool is_040(void)
{
	int cpunode = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "motorola,mc68040");

	if (cpunode < 0)
		return false;

	return true;
}

static inline bool is_030(void)
{
	int cpunode = fdt_node_offset_by_compatible(gd->fdt_blob, -1, "motorola,mc68030");

	if (cpunode < 0)
		return false;

	printf("can't you see I'm an 030?!\n");

	return true;
}

void icache_enable(void)
{
	if (is_030())
		icache_enable_mc68030();
	else if (is_040()) {

	}
}

void icache_disable(void)
{
	if (is_030())
		icache_disable_mc68030();
	else if(is_040())
		icache_disable_mc68040();
}

int icache_status(void)
{
	if (is_030())
		return icache_status_mc68030();
	else if (is_040())
		return icache_status_mc68040();
	else
		return 0;
}

void dcache_enable(void)
{
	if (is_030())
		dcache_enable_mc68030();
	else if(is_040())
		dcache_enable_mc68040();
}

void dcache_disable(void)
{
	if (is_030())
		dcache_disable_mc68030();
	else if (is_040())
		dcache_disable_mc68040();
}

int dcache_status(void)
{
	if(is_030())
		return dcache_status_mc68030();
	else if (is_040())
		return dcache_status_mc68040();
	else
		return 0;
}

void flush_cache(unsigned long addr, unsigned long size)
{

}

void flush_dcache_range(unsigned long start, unsigned long stop)
{

}
