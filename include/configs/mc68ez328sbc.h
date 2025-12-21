//#define CFG_SYS_FLASH_BASE 0x800000
#ifdef CONFIG_SPL_BUILD
#define CFG_SYS_INIT_RAM_ADDR 0x400000
#define CFG_SYS_INIT_RAM_SIZE 0x300000
#else
#define CFG_SYS_INIT_RAM_ADDR 0
#define CFG_SYS_INIT_RAM_SIZE 0x100000
#endif

#define CFG_SYS_FLASH_BASE 0x10000000

#define CFG_EXTRA_ENV_SETTINGS \
		"bootcmd=mmc rescan; fatload mmc 0:1 0x400 vmlinux.mc68ez328.lz4;" \
		"unlz4 0x400 0x400000 0x300000; bootelf 0x400000\0" \
		"autostart=yes\0" \
		"bootargs=earlycon=mc68ez328,0xfffff900 console=ttyDB0 root=/dev/mmcblk0p2 rootfstype=squashfs rootwait\0"
