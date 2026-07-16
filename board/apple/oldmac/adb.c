// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Apple Desktop Bus (ADB) keyboard and mouse for classic 68k Macintosh.
 *
 * ADB is driven through the VIA1 shift register using the "Mac II" polled
 * protocol: the host toggles the two ADB state lines in Port B (command / even
 * / odd / idle) and each transition clocks a byte through the shift register,
 * raising the SR interrupt flag.  We poll that flag rather than taking the
 * interrupt.  This mirrors Linux drivers/macintosh/via-macii.c and the QEMU
 * hw/misc/mac_via.c model it was validated against.
 *
 * The keyboard is exposed as a UCLASS_KEYBOARD device feeding U-Boot's input
 * layer (ADB keycodes are translated to Linux keycodes, then to ASCII by the
 * standard keymap).  A small "adb" command enumerates the bus and shows live
 * keyboard/mouse events.
 */

#include <command.h>
#include <dm.h>
#include <input.h>
#include <keyboard.h>
#include <stdio.h>
#include <stdio_dev.h>
#include <time.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/string.h>

/* VIA1 sits at 0x50F00000 on every classic Mac; 6522 registers are byte-wide,
 * spaced 0x200 apart.  (QEMU q800 also aliases 0x5000xxxx here, but real
 * hardware only decodes 0x50F0xxxx.) */
#define VIA_BASE	0x50f00000UL
#define VIA_RS		0x200
#define R_B		0	/* port B data */
#define R_DIRB		2	/* port B direction */
#define R_SR		10	/* shift register */
#define R_ACR		11	/* auxiliary control register */
#define R_IFR		13	/* interrupt flag register */
#define R_IER		14	/* interrupt enable register */

/* Port B ADB signalling */
#define ST_CMD		0x00	/* state: command byte */
#define ST_EVEN		0x10	/* state: even data byte */
#define ST_ODD		0x20	/* state: odd data byte */
#define ST_IDLE		0x30	/* state: idle */
#define ST_MASK		0x30
#define CTLR_IRQ	0x08	/* transceiver interrupt (input, 0 = asserted) */

/* ACR / IFR bits */
#define SR_CTRL		0x1c	/* shift-register control field in ACR */
#define SR_EXT		0x0c	/* shift on external clock */
#define SR_OUT		0x10	/* shift out (vs in) */
#define SR_INT		0x04	/* shift-register flag in IFR/IER */

/* ADB commands: (address << 4) | op.  Talk = 0xC, reg in low 2 bits. */
#define ADB_TALK(addr, reg)	(((addr) << 4) | 0x0c | (reg))
#define ADB_ADDR_KEYBOARD	2
#define ADB_ADDR_MOUSE		3

static inline u8 vrd(unsigned int reg)
{
	return readb((void *)(VIA_BASE + reg * VIA_RS));
}

static inline void vwr(unsigned int reg, u8 val)
{
	writeb(val, (void *)(VIA_BASE + reg * VIA_RS));
}

static void adb_init(void)
{
	static bool done;

	if (done)
		return;

	vwr(R_IER, SR_INT);		/* bit7 clear: disable SR IRQ, we poll */
	vwr(R_DIRB, (vrd(R_DIRB) | ST_MASK) & ~CTLR_IRQ);
	vwr(R_ACR, (vrd(R_ACR) & ~SR_CTRL) | SR_EXT);
	vwr(R_B, (vrd(R_B) & ~ST_MASK) | ST_IDLE);
	(void)vrd(R_SR);
	vwr(R_IFR, SR_INT);
	done = true;
}

/* Wait for the shift register to signal a byte, then clear the flag. */
static int adb_wait_sr(void)
{
	int timeout = 100000;

	while (!(vrd(R_IFR) & SR_INT)) {
		if (--timeout <= 0)
			return -ETIMEDOUT;
		udelay(5);
	}
	vwr(R_IFR, SR_INT);
	return 0;
}

/*
 * Issue one ADB command byte and collect the reply.  Returns the number of
 * reply bytes read (0 on a bus timeout, i.e. no device / no data).
 */
static int adb_talk(u8 cmd, u8 *buf, int max)
{
	int n = 0;
	u8 st;

	/* Command phase: shift the command byte out in the "command" state. */
	vwr(R_ACR, vrd(R_ACR) | SR_OUT);
	vwr(R_SR, cmd);
	vwr(R_B, (vrd(R_B) & ~ST_MASK) | ST_CMD);
	if (adb_wait_sr())
		goto idle;

	/* Reply phase: switch to input and clock bytes in until the
	 * transceiver de-asserts CTLR_IRQ to mark the end of the reply. */
	vwr(R_ACR, vrd(R_ACR) & ~SR_OUT);
	(void)vrd(R_SR);
	vwr(R_B, (vrd(R_B) & ~ST_MASK) | ST_EVEN);

	for (;;) {
		if (adb_wait_sr())
			break;
		st = vrd(R_B) & (ST_MASK | CTLR_IRQ);
		u8 x = vrd(R_SR);

		if (!(st & CTLR_IRQ))	/* end of data / bus timeout */
			break;
		if (n < max)
			buf[n++] = x;
		vwr(R_B, vrd(R_B) ^ ST_MASK);	/* toggle even/odd */
	}

idle:
	vwr(R_ACR, vrd(R_ACR) & ~SR_OUT);
	(void)vrd(R_SR);
	vwr(R_B, (vrd(R_B) & ~ST_MASK) | ST_IDLE);
	return n;
}

/* ADB keycode -> Linux keycode (from Linux drivers/macintosh/adbhid.c). */
static const u16 adb_keymap[128] = {
	[0x00] = KEY_A,		[0x01] = KEY_S,		[0x02] = KEY_D,
	[0x03] = KEY_F,		[0x04] = KEY_H,		[0x05] = KEY_G,
	[0x06] = KEY_Z,		[0x07] = KEY_X,		[0x08] = KEY_C,
	[0x09] = KEY_V,		[0x0a] = KEY_102ND,	[0x0b] = KEY_B,
	[0x0c] = KEY_Q,		[0x0d] = KEY_W,		[0x0e] = KEY_E,
	[0x0f] = KEY_R,		[0x10] = KEY_Y,		[0x11] = KEY_T,
	[0x12] = KEY_1,		[0x13] = KEY_2,		[0x14] = KEY_3,
	[0x15] = KEY_4,		[0x16] = KEY_6,		[0x17] = KEY_5,
	[0x18] = KEY_EQUAL,	[0x19] = KEY_9,		[0x1a] = KEY_7,
	[0x1b] = KEY_MINUS,	[0x1c] = KEY_8,		[0x1d] = KEY_0,
	[0x1e] = KEY_RIGHTBRACE,[0x1f] = KEY_O,		[0x20] = KEY_U,
	[0x21] = KEY_LEFTBRACE,	[0x22] = KEY_I,		[0x23] = KEY_P,
	[0x24] = KEY_ENTER,	[0x25] = KEY_L,		[0x26] = KEY_J,
	[0x27] = KEY_APOSTROPHE,[0x28] = KEY_K,		[0x29] = KEY_SEMICOLON,
	[0x2a] = KEY_BACKSLASH,	[0x2b] = KEY_COMMA,	[0x2c] = KEY_SLASH,
	[0x2d] = KEY_N,		[0x2e] = KEY_M,		[0x2f] = KEY_DOT,
	[0x30] = KEY_TAB,	[0x31] = KEY_SPACE,	[0x32] = KEY_GRAVE,
	[0x33] = KEY_BACKSPACE,	[0x34] = KEY_KPENTER,	[0x35] = KEY_ESC,
	[0x36] = KEY_LEFTCTRL,	[0x37] = KEY_LEFTMETA,	[0x38] = KEY_LEFTSHIFT,
	[0x39] = KEY_CAPSLOCK,	[0x3a] = KEY_LEFTALT,	[0x3b] = KEY_LEFT,
	[0x3c] = KEY_RIGHT,	[0x3d] = KEY_DOWN,	[0x3e] = KEY_UP,
	[0x41] = KEY_KPDOT,	[0x43] = KEY_KPASTERISK,[0x45] = KEY_KPPLUS,
	[0x47] = KEY_NUMLOCK,	[0x4b] = KEY_KPSLASH,	[0x4c] = KEY_KPENTER,
	[0x4e] = KEY_KPMINUS,	[0x51] = KEY_KPEQUAL,	[0x52] = KEY_KP0,
	[0x53] = KEY_KP1,	[0x54] = KEY_KP2,	[0x55] = KEY_KP3,
	[0x56] = KEY_KP4,	[0x57] = KEY_KP5,	[0x58] = KEY_KP6,
	[0x59] = KEY_KP7,	[0x5b] = KEY_KP8,	[0x5c] = KEY_KP9,
};

static int adb_kbd_read_keys(struct input_config *input)
{
	u8 buf[4];
	int n, i;

	n = adb_talk(ADB_TALK(ADB_ADDR_KEYBOARD, 0), buf, sizeof(buf));
	for (i = 0; i < n; i++) {
		u8 raw = buf[i];
		int lk;

		if (raw == 0xff)	/* empty key slot */
			continue;
		lk = adb_keymap[raw & 0x7f];
		if (lk)
			input_add_keycode(input, lk, raw & 0x80);
	}
	return 0;
}

static int adb_kbd_probe(struct udevice *dev)
{
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct input_config *input = &uc_priv->input;
	int ret;

	adb_init();

	input_init(input, false);
	input_add_tables(input, false);
	input->dev = dev;
	input->read_keys = adb_kbd_read_keys;

	strcpy(sdev->name, "adb-kbd");
	ret = input_stdio_register(sdev);
	if (ret)
		return ret;

	return 0;
}

static const struct keyboard_ops adb_kbd_ops = { };

U_BOOT_DRIVER(adb_kbd) = {
	.name	= "adb_kbd",
	.id	= UCLASS_KEYBOARD,
	.probe	= adb_kbd_probe,
	.ops	= &adb_kbd_ops,
};

U_BOOT_DRVINFO(adb_kbd) = {
	.name = "adb_kbd",
};

#if IS_ENABLED(CONFIG_CMD_ADB)
static void adb_show_devices(void)
{
	int addr;

	printf("ADB bus:\n");
	for (addr = 1; addr <= 7; addr++) {
		u8 buf[4];
		int n = adb_talk(ADB_TALK(addr, 3), buf, sizeof(buf));

		if (n >= 2) {
			const char *kind = addr == ADB_ADDR_KEYBOARD ? " (keyboard)" :
					   addr == ADB_ADDR_MOUSE ? " (mouse)" : "";

			printf("  address %d: handler 0x%02x%s\n",
			       addr, buf[1], kind);
		}
	}
}

/*
 * Read one packet from the ADB mouse and decode it.  The mouse packet is two
 * bytes: [~btn | 7-bit dY][~btn2 | 7-bit dX], both movement fields 7-bit
 * two's complement.  Live polling here is best-effort: when the keyboard is the
 * console input device the transceiver autopolls the keyboard, so a lone mouse
 * Talk often returns a bus timeout.  Enumeration (below) is always reliable.
 */
static void adb_read_mouse(void)
{
	u8 buf[4];
	int n = adb_talk(ADB_TALK(ADB_ADDR_MOUSE, 0), buf, sizeof(buf));

	if (n >= 2 && !(buf[0] == 0xff && buf[1] == 0xff)) {
		int dy = (buf[0] & 0x40) ? (buf[0] & 0x7f) - 0x80 : (buf[0] & 0x7f);
		int dx = (buf[1] & 0x40) ? (buf[1] & 0x7f) - 0x80 : (buf[1] & 0x7f);

		printf("  mouse dx=%d dy=%d%s\n", dx, dy,
		       (buf[0] & 0x80) ? "" : " button");
	} else {
		printf("  mouse: no movement pending\n");
	}
}

static int do_adb(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	adb_init();
	adb_show_devices();

	if (argc > 1 && !strcmp(argv[1], "mouse"))
		adb_read_mouse();
	else
		printf("Keyboard input is delivered to the console (stdin).\n");

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(adb, 2, 1, do_adb,
	   "probe the Apple Desktop Bus",
	   "         - list ADB devices\n"
	   "adb mouse - list devices and read one mouse packet");
#endif /* CMD_ADB */
