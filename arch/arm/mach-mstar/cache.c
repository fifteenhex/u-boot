#include <asm/armv7.h>
#include <asm/io.h>
#include <chenxingv7.h>

static void mstar_miu_flush(void)
{
	/* toggle the flush miu pipe fire bit */
	writew_relaxed(0, L3BRIDGE + L3BRIDGE_14);
	writew_relaxed(L3BRIDGE_FLUSH_TRIGGER, L3BRIDGE + L3BRIDGE_14);
	while (!(readw_relaxed(L3BRIDGE + L3BRIDGE_40) & L3BRIDGE_STATUS_DONE)) {
		/* wait for flush to complete */
	}
}

void v7_outer_cache_flush_all()
{
	barrier();
	mstar_miu_flush();
}

void v7_outer_cache_flush_range(u32 start, u32 end)
{
	barrier();
	mstar_miu_flush();
}
