#define CFG_SYS_INIT_RAM_ADDR 0

/*
 * We want to map the save SRAM into the last 512KB
 * slot so the most RAM we can map into the cart address
 * space is 3.5MB
 */
#define CFG_SYS_INIT_RAM_SIZE 0x380000

//		"bootcmd=mmc rescan; fatload mmc 0:1 0x400 vmlinux.mc68ez328.lz4;"
//		"unlz4 0x400 0x400000 0x300000; bootelf 0x400000\0"

#define CFG_EXTRA_ENV_SETTINGS \
		"stdout=serial,vidconsole\0" \
		"autostart=yes\0" \
		"bootargs=earlycon=mc68ez328,0xfffff900 console=ttyDB0 init=/root/init root=/dev/mmcblk0p2 rootfstype=squashfs rootwait\0"
