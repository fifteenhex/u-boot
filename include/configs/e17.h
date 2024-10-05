#ifdef CONFIG_SPL_BUILD
#define CFG_SYS_INIT_RAM_ADDR 0x400000
#define CFG_SYS_INIT_RAM_SIZE 0x300000
#else
#define CFG_SYS_INIT_RAM_ADDR 0
#define CFG_SYS_INIT_RAM_SIZE 0x200000
#endif
#if 0
#define CFG_EXTRA_ENV_SETTINGS \
		"bootcmd=virtio scan; fatload virtio 1:1 0x3000000 vmlinux.virt\; bootelf 0x3000000\0" \
		"autostart=yes\0" \
		"bootargs=console=ttyGF0 root=/dev/vda rootfstype=squashfs\0"
#endif
