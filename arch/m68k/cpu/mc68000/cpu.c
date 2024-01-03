#include <command.h>
#include <asm/global_data.h>
#include <cpu_func.h>

/*
 * Jumping back to 0 might not be workable, so just spin.
 * If you have a safe way to reboot on your board override this.
 */
__weak void reset_cpu(void)
{
	while(true) {
	}
}

int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	reset_cpu();
	return 0;
};
