#define CFG_SYS_INIT_RAM_ADDR 0
#define CFG_SYS_INIT_RAM_SIZE 0x200000
#define CFG_EXTRA_ENV_SETTINGS \
		"kernel_addr_r=0x400000\0" \
		"bootcmd=virtio scan; fatload virtio 1:1 ${kernel_addr_r} vmlinux.virt; bootelf ${kernel_addr_r}\0" \
		"autostart=yes\0" \
		"bootargs=console=ttyGF0 root=/dev/vda rootfstype=squashfs init=/root/init\0"
