#include <lmb.h>

void m68k_lmb_protect_vectors(void)
{
#ifndef CONFIG_M68K_HAVE_VBAR
	int ret;

	/* Don't let the vectors get overwritten if there is no vbar to move the table away from 0x0*/
	ret = lmb_alloc_mem(LMB_MEM_ALLOC_ADDR, 0, 0x0 + 0x4, 0x400, LMB_NOOVERWRITE);
		if (ret)
			printf("mmm %d\n", ret);
#endif
}
