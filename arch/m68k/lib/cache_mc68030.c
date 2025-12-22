#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/bitops.h>

#include "cache_mc68030.h"

#define MC68030_CACR_EI		BIT(0)
#define MC68030_CACR_CI		BIT(3)
#define MC68030_CACR_IBE	BIT(4)
#define MC68030_CACR_ED		BIT(8)
#define MC68030_CACR_CD		BIT(11)


#define REGGETTER(_which, _inst)					\
static inline u32 mc68030_get_##_which(void)				\
{									\
	u32 val;							\
									\
	asm volatile (#_inst " %%" #_which ",%0\n\t" : "=d" (val));	\
									\
	return val;							\
}

#define REGSETTER(_which, _inst)					\
static inline void mc68030_set_##_which(u32 val)			\
{									\
	asm volatile (#_inst " %0,%%" #_which "\n\t" : : "d" (val));	\
}

REGGETTER(vbr, movec);
REGSETTER(vbr, movec);

/* Cache */
REGGETTER(cacr, movec);
REGSETTER(cacr, movec);
REGGETTER(caar, movec);
REGSETTER(caar, movec);

/* MMU */
REGGETTER(mmusr, movec);
REGGETTER(tc, pmove);
REGGETTER(tt0, pmove);
REGGETTER(tt1, pmove);

void icache_enable_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_EI | MC68030_CACR_IBE);
}

void icache_disable_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_CI);
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_EI);
}

int icache_status_mc68030(void)
{
	return (mc68030_get_cacr() & MC68030_CACR_EI) ? 1 : 0;
}

void dcache_enable_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_ED);
}

void dcache_disable_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_CD);
	mc68030_set_cacr(mc68030_get_cacr() & ~MC68030_CACR_ED);
}

static void mc68030_dump_specialregs(void)
{
	u32 vbr = mc68030_get_vbr();
	u32 cacr = mc68030_get_cacr();
	//u32 caar = mc68030_get_caar();
	u32 caar = 0;

	printf("vbr:  0x%08x\n"
		   "CACR: 0x%08x\n"
		   "CAAR: 0x%08x\n",
		   vbr, cacr, caar);

#if 0
	{
		//u32 tc = mc68030_get_tc();
		u32 mmusr = mc68030_get_mmusr();
		u32 tc = mc68030_get_tc();
		u32 tt0 = mc68030_get_tt0();
		u32 tt1 = mc68030_get_tt1();
		printf("-- MMU --\n"
			   "TC:  0x%08x\n"
			   "TT0: 0x%08x\n"
			   "TT1: 0x%08x\n",
			   tc, tt0, tt1);
	}
#endif
}

int dcache_status_mc68030(void)
{
	mc68030_dump_specialregs();

	return (mc68030_get_cacr() & MC68030_CACR_ED) ? 1 : 0;
}

void invalidate_dcache_all_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_CD);
}

void flush_dcache_all_mc68030(void)
{
}

void invalidate_icache_all_mc68030(void)
{
	mc68030_set_cacr(mc68030_get_cacr() | MC68030_CACR_CI);
}
#if !defined(CONFIG_TARGET_QEMU)
void icache_enable(void)
{
	icache_enable_mc68030();
}

void icache_disable(void)
{
	icache_disable_mc68030();
}

int icache_status(void)
{
	return icache_status_mc68030();
}

void dcache_enable(void)
{
	dcache_enable_mc68030();
}

void dcache_disable(void)
{
	dcache_disable_mc68030();
}

int dcache_status(void)
{
	return dcache_status_mc68030();
}

void invalidate_dcache_all(void)
{
	invalidate_dcache_all_mc68030();
}

void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	invalidate_dcache_all();
}

void flush_dcache_all(void)
{
	flush_dcache_all_mc68030();
}

void flush_dcache_range(unsigned long start, unsigned long stop)
{
	flush_dcache_all();
}

void invalidate_icache_all(void)
{
	invalidate_icache_all_mc68030();
}

void flush_cache(unsigned long addr, unsigned long size)
{
	flush_dcache_all();
}
#endif
