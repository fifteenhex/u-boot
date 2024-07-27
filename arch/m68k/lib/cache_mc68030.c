#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/bitops.h>

#define MC68030_CACR_EI	BIT(0)
#define MC68030_CACR_ED	BIT(8)

static inline u32 mc68030_get_cacr(void)
{
	u32 cacr;

	asm volatile ("movec %%cacr,%0" : "=r" (cacr));

	return cacr;
}

static inline void mc68030_set_cacr(u32 cacr)
{
	asm volatile ("movec %0,%%cacr" : : "r" (cacr));
}

void icache_enable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_EI);
}

void icache_disable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_EI);
}

int icache_status(void)
{
	return (mc68030_get_cacr() & MC68030_CACR_EI) ? 1 : 0;
}

void dcache_enable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_ED);
}

void dcache_disable(void)
{
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_ED);
}

int dcache_status(void)
{
	u32 vbr;


	asm volatile ("movec %%vbr,%0" : "=r" (vbr));

	printf("vbr: 0x%08x\n", vbr);

	return (mc68030_get_cacr() & MC68030_CACR_ED) ? 1 : 0;
}

