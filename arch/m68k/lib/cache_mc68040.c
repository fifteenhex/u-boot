#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/bitops.h>

#include "cache_mc68040.h"

#define MC68040_CACR_IE	BIT(15)
#define MC68040_CACR_DE	BIT(31)

#define REGGETTER(_which, _inst)								\
static inline u32 mc68040_get_##_which(void)					\
{																\
	u32 val;													\
																\
	asm volatile (#_inst " %%" #_which ",%0\n\t" : "=d" (val));	\
																\
	return val;													\
}

#define REGSETTER(_which, _inst)									\
static inline void mc68040_set_##_which(u32 val)					\
{																	\
	asm volatile (#_inst " %0,%%" #_which "\n\t" : : "d" (val));	\
}

REGGETTER(cacr, movec);
REGSETTER(cacr, movec);

void icache_enable_mc68040(void)
{
	u32 val = mc68040_get_cacr();
	val |= MC68040_CACR_IE;
	mc68040_set_cacr(val);
}

void icache_disable_mc68040(void)
{
}

int icache_status_mc68040(void)
{
	return (mc68040_get_cacr() & MC68040_CACR_IE) ? 1 : 0;
}

void dcache_enable_mc68040(void)
{
}

void dcache_disable_mc68040(void)
{
}

static void mc68040_dump_specialregs(void)
{

}

int dcache_status_mc68040(void)
{
	return (mc68040_get_cacr() & MC68040_CACR_DE) ? 1 : 0;
}

#if !defined(CONFIG_TARGET_QEMU)
void icache_enable(void)
{
	icache_enable_mc68040();
}

void icache_disable(void)
{
	icache_disable_mc68040();
}

int icache_status(void)
{
	return icache_status_mc68040();
}

void dcache_enable(void)
{
	dcache_enable_mc68040();
}

void dcache_disable(void)
{
	dcache_disable_mc68040();
}

int dcache_status(void)
{
	return dcache_status_mc68040();
}
#endif
