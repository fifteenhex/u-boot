#ifdef CONFIG_SPL_BUILD
#define CFG_SYS_INIT_RAM_ADDR 0x400000
#define CFG_SYS_INIT_RAM_SIZE 0x300000
#else
#define CFG_SYS_INIT_RAM_ADDR 0
#define CFG_SYS_INIT_RAM_SIZE 0x200000
#endif
/*
 * Autoboot: scan the SCSI bus, load vmlinux from the FAT partition on
 * disk 0 and boot it.  The board only has 16 MiB of RAM, so the vmlinux
 * load address stays low.
 */
#define CFG_EXTRA_ENV_SETTINGS \
		"bootcmd=scsi scan; fatload scsi 0:1 0x400000 vmlinux; bootelf 0x400000\0"
