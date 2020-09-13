/*
 * Mstar Armv7 generic board
 */

#include <init.h>
#include <fdtdec.h>

#ifdef CONFIG_DEBUG_UART
#include <debug_uart.h>
#endif

#ifdef CONFIG_SPL_BUILD
void board_init_f(ulong dummy)
{
#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
#endif

	spl_early_init();
	preloader_console_init();
}

#endif // spl

int embedded_dtb_select(void)
{
	fdtdec_setup();
	return 0;
}
