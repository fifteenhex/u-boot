/*
 */

#include <common.h>
#include <asm/io.h>

#include "chenxingv7.h"
#include "clk.h"

void mstar_clockfixup()
{
	// once the DDR is running the deglitch clock doesn't work anymore.
	writew_relaxed(0x10, CLKGEN + CLKGEN_BDMA);
}
