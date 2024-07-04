// SPDX-License-Identifier: eCos-2.0
/*
 *==========================================================================
 *
 *      xyzModem.c
 *
 *      RedBoot stream handler for xyzModem protocol
 *
 *==========================================================================
 *#####DESCRIPTIONBEGIN####
 *
 * Author(s):    gthomas
 * Contributors: gthomas, tsmith, Yoshinori Sato
 * Date:         2000-07-14
 * Purpose:
 * Description:
 *
 * This code is part of RedBoot (tm).
 *
 *####DESCRIPTIONEND####
 *
 *==========================================================================
 */

#define DEBUG 1

#include <xyzModem.h>
#include <stdarg.h>
#include <time.h>
#include <u-boot/crc.h>
#include <watchdog.h>
#include <env.h>
#include <vsprintf.h>

/* Assumption - run xyzModem protocol over the console port */

/* Values magic to the protocol */
#define SOH 0x01
#define STX 0x02
/* ^C for interrupt */
#define ETX 0x03
#define EOT 0x04
#define ACK 0x06
#define BSP 0x08
#define NAK 0x15
#define CAN 0x18
/* ^Z for DOS officionados */
#define EOF 0x1A

/* Data & state local to the protocol */
struct xyz {
	u8 blk, cblk;
	u8 pkt[1024];
	u8 crc1, crc2;

	unsigned char next_blk; /* Expected block */
	int len, mode, total_retries;
	int total_SOH, total_STX, total_CAN;
	bool crc_mode, at_eof, tx_ack;
	bool first_xmodem_packet;
	ulong initial_time, timeout;
	unsigned long file_length, read_length;
};

static struct xyz xyz;

/* 2 seconds */
#define xyzModem_CHAR_TIMEOUT            2000
#define xyzModem_MAX_RETRIES             20
#define xyzModem_MAX_RETRIES_WITH_CRC    10
/* Wait for 3 CAN before quitting */
#define xyzModem_CAN_COUNT                3

/* Get a char but don't wait forever */
static int xyzModem_getchar(char *c) {
	ulong now = get_timer(0);

	schedule();
	while (true) {
		if (tstc()) {
			int ret = getchar();
			if (ret >= 0) {
				*c = ret;
				return 1;
			} else if (ret != -EAGAIN)
				return 0;
		}
		if (get_timer(now) > xyzModem_CHAR_TIMEOUT)
			break;
	}

	return 0;
}

static int xyzModem_getchars(char *c, unsigned int n) {
	ulong now;
	int i, charsread = 0;
	const int blksz = 16;

	schedule();

	/*
	 * Constantly checking the timeout is a waste of
	 * cycles, spam getchar() for a small block of chars
	 * and check the timeout between those blocks.
	 */
	while (charsread != n) {
		now = get_timer(0);

		for (i = 0; (i < blksz) && (charsread < n); i++) {
			int ret = getchar();
			if (ret >= 0)
				c[charsread++] = ret;
			else if (ret != -EAGAIN)
				return ret;
		}
		if (get_timer(now) > xyzModem_CHAR_TIMEOUT * blksz)
			break;
	}

	return charsread;
}

/* Validate a hex character */
__inline__ static bool _is_hex(char c) {
	return (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F'))
			|| ((c >= 'a') && (c <= 'f')));
}

/* Convert a single hex nibble */
__inline__ static int _from_hex(char c) {
	int ret = 0;

	if ((c >= '0') && (c <= '9')) {
		ret = (c - '0');
	} else if ((c >= 'a') && (c <= 'f')) {
		ret = (c - 'a' + 0x0a);
	} else if ((c >= 'A') && (c <= 'F')) {
		ret = (c - 'A' + 0x0A);
	}
	return ret;
}

/* Convert a character to lower case */
__inline__ static char _tolower(char c) {
	if ((c >= 'A') && (c <= 'Z')) {
		c = (c - 'A') + 'a';
	}
	return c;
}

/* Parse (scan) a number */
static bool parse_num(char *s, unsigned long *val, char **es, char *delim) {
	bool first = true;
	int radix = 10;
	char c;
	unsigned long result = 0;
	int digit;

	while (*s == ' ')
		s++;
	while (*s) {
		if (first && (s[0] == '0') && (_tolower(s[1]) == 'x')) {
			radix = 16;
			s += 2;
		}
		first = false;
		c = *s++;
		if (_is_hex(c) && ((digit = _from_hex(c)) < radix)) {
			/* Valid digit */
			result = (result * radix) + digit;
		} else {
			if (delim != (char*) 0) {
				/* See if this character is one of the delimiters */
				char *dp = delim;
				while (*dp && (c != *dp))
					dp++;
				if (*dp)
					break; /* Found a good delimiter */
			}
			return false; /* Malformatted number */
		}
	}
	*val = result;
	if (es != (char**) 0) {
		*es = s;
	}
	return true;
}

#if defined(DEBUG) && !CONFIG_IS_ENABLED(USE_TINY_PRINTF)
/*
 * Note: this debug setup works by storing the strings in a fixed buffer
 */
static char zm_debug_buf[8192] = { 0 };
static char *zm_out = zm_debug_buf;
static char *zm_out_start = zm_debug_buf;

static int
zm_dprintf(char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = diag_vsprintf(zm_out, fmt, args);
	va_end(args);
	zm_out += len;
	return len;
}

static void
zm_flush (void)
{
  puts(zm_debug_buf);
  memset(zm_out_start, 0, zm_out - zm_out_start);
  zm_out = zm_out_start;
}

static void
zm_dump_buf (void *buf, int len)
{

}

static unsigned char zm_buf[2048];
static unsigned char *zm_bp;

static void
zm_new (void)
{
	zm_bp = zm_buf;
}

static void
zm_save (unsigned char c)
{
	*zm_bp++ = c;
}

static void
zm_dump (int line)
{
//  zm_dprintf ("Packet at line: %d\n", line);
//  zm_dump_buf (zm_buf, zm_bp - zm_buf);
}

#define ZM_DEBUG(x) x
#else
#define ZM_DEBUG(x)
#endif

/* Wait for the line to go idle */
static void xyzModem_flush(void) {
	int res;
	char c;
	while (true) {
		res = xyzModem_getchar(&c);
		if (!res)
			return;
	}
}

static int xyzModem_sync_packet_start(void) {
	char c;
	int res;
	bool hdr_found = false;
	/* Find the start of a header */
	int can_total, hdr_chars;
	can_total = 0;
	hdr_chars = 0;

	while (!hdr_found) {
		res = xyzModem_getchar(&c);
		ZM_DEBUG(zm_save (c));
		if (res) {
			hdr_chars++;
			switch (c) {
			case SOH:
				xyz.total_SOH++;
			case STX:
				if (c == STX)
					xyz.total_STX++;
				hdr_found = true;
				break;
			case CAN:
			case ETX:
				xyz.total_CAN++;
				ZM_DEBUG (zm_dump (__LINE__));
				if (++can_total == xyzModem_CAN_COUNT) {
					return xyzModem_cancel;
				} else {
					/* Wait for multiple CAN to avoid early quits */
					continue;
				}
			case EOT:
				/* EOT only supported if no noise */
				if (hdr_chars == 1) {
					putc(ACK);
					ZM_DEBUG (zm_dprintf ("ACK on EOT #%d\n", __LINE__));ZM_DEBUG (zm_dump (__LINE__));
					return xyzModem_eof;
				}
			default:
				/* Ignore, waiting for start of header */
				;
			}
		} else {
			/* Data stream timed out */
			xyzModem_flush(); /* Toss any current input */
			ZM_DEBUG (zm_dump (__LINE__));
			CYGACC_CALL_IF_DELAY_US(250000);
			return xyzModem_timeout;
		}
	}

	if (hdr_found)
		xyz.len = (c == SOH) ? 128 : 1024;

	return 0;
}

static int xyzModem_read_block(struct xyz *_xyz)
{
	int ret;

	ret = xyzModem_getchar(&_xyz->blk);
	if (!ret)
		return xyzModem_timeout;

	ret = xyzModem_getchar(&_xyz->cblk);
	if (!ret)
		return xyzModem_timeout;

	return 0;
}

static int xyzModem_read_data(void)
{
	return xyzModem_getchars(xyz.pkt, xyz.len);
}

static int xyzModem_read_checksum(struct xyz *_xyz)
{
	int res;

	res = xyzModem_getchar(&_xyz->crc1);
	if (!res)
		return xyzModem_timeout;

	if (xyz.crc_mode) {
		res = xyzModem_getchar(&_xyz->crc2);
		if (!res)
			return xyzModem_timeout;
	}

	return 0;
}

static int xyzModem_validate_message(const struct xyz *_xyz)
{
	int i;

	/* Validate the block number */
	if ((_xyz->blk ^ _xyz->cblk) != 0xFF) {
		return xyzModem_frame;
	}

	/* Verify checksum/CRC */
	if (xyz.crc_mode) {
		u16 cksum = crc16_ccitt(0, xyz.pkt, xyz.len);
		if (cksum != ((xyz.crc1 << 8) | xyz.crc2)) {
			ZM_DEBUG (zm_dprintf ("CRC error - recvd: %02x%02x, computed: %x\n",
							xyz.crc1, xyz.crc2, cksum & 0xFFFF));
			return xyzModem_cksum;
		}
	} else {
		unsigned short cksum = 0;
		for (i = 0; i < xyz.len; i++) {
			cksum += xyz.pkt[i];
		}
		if (xyz.crc1 != (cksum & 0xFF)) {
			ZM_DEBUG (zm_dprintf
					("Checksum error - recvd: %x, computed: %x\n", xyz.crc1,
							cksum & 0xFF));
			return xyzModem_cksum;
		}
	}

	return 0;
}

#define XXX_LOGERR(v) ZM_DEBUG(zm_dprintf("ERR(%d) %d\n", v, __LINE__))

static int xyzModem_get_hdr(void) {
	int res;

	/* Flush the log buffer */
	ZM_DEBUG(zm_new());

	res = xyzModem_sync_packet_start();
	if (res) {
		XXX_LOGERR(res);
		goto err;
	}

	if (xyz.tx_ack) {
		putc(ACK);
		xyz.tx_ack = false;
	}

	res = xyzModem_read_block(&xyz);
	if (res) {
		XXX_LOGERR(res);
		goto err;
	}

	res = xyzModem_read_data();
	if (res <= 0) {
		XXX_LOGERR(res);
		goto err;
	}

	res = xyzModem_read_checksum(&xyz);
	if (res) {
		XXX_LOGERR(res);
		goto err;
	}

	res = xyzModem_validate_message(&xyz);
	if (res) {
		XXX_LOGERR(res);
		goto err;
	}
	/* If we get here, the message passes [structural] muster */
	return 0;

err:
	xyzModem_flush();
	return res;
}

static ulong xyzModem_get_initial_timeout(void) {
	/* timeout is in seconds, non-positive timeout value is infinity */
#if CONFIG_IS_ENABLED(ENV_SUPPORT)
	const char *timeout_str = env_get("loadxy_timeout");
	if (timeout_str)
		return 1000 * simple_strtol(timeout_str, NULL, 10);
#endif
	return 1000 * CONFIG_CMD_LOADXY_TIMEOUT;
}

static void xyzModem_parse_ymodemhdr(void)
{
	char *bufp = xyz.pkt;
	/* skip filename */
	while (*bufp++)
		;
	/* get the length */
	parse_num(bufp, &xyz.file_length, NULL, " ");
	/* The rest of the file name data block quietly discarded */
	xyz.tx_ack = true;
}

int xyzModem_stream_open(connection_info_t *info, int *err) {
	int stat = 0;
	int retries = xyzModem_MAX_RETRIES;
	int crc_retries = xyzModem_MAX_RETRIES_WITH_CRC;

	/*    ZM_DEBUG(zm_out = zm_out_start); */
#ifdef xyzModem_zmodem
	if (info->mode == xyzModem_zmodem)
	{
		*err = xyzModem_noZmodem;
		return -1;
	}
#endif

	memset(&xyz, 0, sizeof(xyz));
	xyz.crc_mode = true;
	xyz.mode = info->mode;
	xyz.initial_time = get_timer(0);
	xyz.timeout = xyzModem_get_initial_timeout();

	putc((xyz.crc_mode ? 'C' : NAK));

	if (xyz.mode == xyzModem_xmodem) {
		/* X-modem doesn't have an information header - exit here */
		xyz.first_xmodem_packet = true;
		xyz.next_blk = 1;
		return 0;
	}

	while (!(xyz.timeout && get_timer(xyz.initial_time) > xyz.timeout)) {
		if (--retries <= 0) {
			retries = xyzModem_MAX_RETRIES;
			crc_retries = xyzModem_MAX_RETRIES_WITH_CRC;
			xyz.crc_mode = true;
		}

		stat = xyzModem_get_hdr();
		if (stat == 0) {
			/* Y-modem file information header */
			if (xyz.blk == 0)
				xyzModem_parse_ymodemhdr();
			xyz.next_blk = 1;
			xyz.len = 0;
			return 0;
		} else if (stat == xyzModem_timeout) {
			if (--crc_retries <= 0)
				xyz.crc_mode = false;

			/* Extra delay for startup */
			CYGACC_CALL_IF_DELAY_US(5 * 100000);

			putc((xyz.crc_mode ? 'C' : NAK));
			xyz.total_retries++;
			ZM_DEBUG (zm_dprintf ("NAK (%d)\n", __LINE__));
		}
		if (stat == xyzModem_cancel)
			break;
	}

	*err = stat;
	ZM_DEBUG (zm_flush ());
	return -1;
}

int xyzModem_stream_read(char *buf, int size, int *err) {
	int stat, total, len;
	int retries;

	*err = 0;
	total = 0;
	stat = xyzModem_cancel;

	/* Try and get 'size' bytes into the buffer */
	while (!xyz.at_eof && xyz.len >= 0 && (size > 0)) {
		if (xyz.len == 0) {
			retries = xyzModem_MAX_RETRIES;

			while (retries-- > 0) {
				if (xyz.first_xmodem_packet && xyz.timeout
						&& get_timer(xyz.initial_time) > xyz.timeout) {
					*err = xyzModem_timeout;
					xyz.len = -1;
					return total;
				}

				stat = xyzModem_get_hdr();
				if (stat == 0) {
					if (xyz.mode == xyzModem_xmodem && xyz.first_xmodem_packet)
						xyz.first_xmodem_packet = false;
					if (xyz.blk == xyz.next_blk) {
						xyz.tx_ack = true;
						//ZM_DEBUG (zm_dprintf
						//	("ACK block %d (%d)\n", xyz.blk, __LINE__));
						xyz.next_blk = (xyz.next_blk + 1) & 0xFF;
						if (xyz.mode == xyzModem_xmodem
								|| xyz.file_length == 0) {
							/* Data blocks can be padded with ^Z (EOF) characters */
							/* This code tries to detect and remove them */
							char *bufp = xyz.pkt;
							if ((bufp[xyz.len - 1] == EOF)
									&& (bufp[xyz.len - 2] == EOF)
									&& (bufp[xyz.len - 3] == EOF)) {
								while (xyz.len && (bufp[xyz.len - 1] == EOF)) {
									xyz.len--;
								}
							}
						}

						/*
						 * See if accumulated length exceeds that of the file.
						 * If so, reduce size (i.e., cut out pad bytes)
						 * Only do this for Y-modem (and Z-modem should it ever
						 * be supported since it can fall back to Y-modem mode).
						 */
						if (xyz.mode != xyzModem_xmodem
								&& 0 != xyz.file_length) {
							xyz.read_length += xyz.len;
							if (xyz.read_length > xyz.file_length) {
								xyz.len -= (xyz.read_length - xyz.file_length);
							}
						}
						break;
					} else if (xyz.blk == ((xyz.next_blk - 1) & 0xFF)) {
						/* Just re-ACK this so sender will get on with it */
						putc(ACK);
						continue; /* Need new header */
					} else {
						stat = xyzModem_sequence;
					}
				}
				if (stat == xyzModem_cancel) {
					break;
				}

				if (stat == xyzModem_eof) {
					putc( ACK);
					ZM_DEBUG (zm_dprintf ("ACK (%d)\n", __LINE__));
					if (xyz.mode == xyzModem_ymodem) {
						putc((xyz.crc_mode ? 'C' : NAK));
						xyz.total_retries++;
						ZM_DEBUG (zm_dprintf ("Reading Final Header\n"));
						stat = xyzModem_get_hdr();
						putc(ACK);
						ZM_DEBUG(zm_dprintf ("FINAL ACK (%d)\n", __LINE__));
					} else
						stat = 0;
					xyz.at_eof = true;
					break;
				}

				putc( (xyz.crc_mode ? 'C' : NAK));
				xyz.total_retries++;
				ZM_DEBUG (zm_dprintf ("NAK (%d,%d)\n", __LINE__, len));
			}
			if (stat < 0 && (!xyz.first_xmodem_packet
					|| stat != xyzModem_timeout)) {
				*err = stat;
				xyz.len = -1;
				return total;
			}
		}
		/* Don't "read" data from the EOF protocol package */
		if (!xyz.at_eof && xyz.len > 0) {
			len = xyz.len;
			if (size < len)
				len = size;
			memcpy(buf, xyz.pkt, len);
			size -= len;
			buf += len;
			total += len;
			xyz.len -= len;
		}
	}

	return total;
}

void xyzModem_stream_close(int *err) {
	ZM_DEBUG (zm_dprintf
			("xyzModem - %s mode, %d(SOH)/%d(STX)/%d(CAN) packets, %d retries\n",
					xyz.crc_mode ? "CRC" : "Cksum", xyz.total_SOH, xyz.total_STX,
					xyz.total_CAN, xyz.total_retries)); ZM_DEBUG (zm_flush ());
}

/* Need to be able to clean out the input buffer, so have to take the */
/* getc */
void xyzModem_stream_terminate(bool abort, int (*getc)(void)) {
	int c;

	if (abort) {
		ZM_DEBUG (zm_dprintf ("!!!! TRANSFER ABORT !!!!\n"));
		switch (xyz.mode) {
		case xyzModem_xmodem:
		case xyzModem_ymodem:
			/* The X/YMODEM Spec seems to suggest that multiple CAN followed by an equal */
			/* number of Backspaces is a friendly way to get the other end to abort. */
			putc(CAN);
			putc(CAN);
			putc(CAN);
			putc(CAN);
			putc(BSP);
			putc(BSP);
			putc(BSP);
			putc(BSP);
			/* Now consume the rest of what's waiting on the line. */
			ZM_DEBUG (zm_dprintf ("Flushing serial line.\n"));
			xyzModem_flush();
			xyz.at_eof = true;
			break;
#ifdef xyzModem_zmodem
	case xyzModem_zmodem:
	  /* Might support it some day I suppose. */
#endif
			break;
		}
	} else {
		ZM_DEBUG (zm_dprintf ("Engaging cleanup mode...\n"));
		/*
		 * Consume any trailing crap left in the inbuffer from
		 * previous received blocks. Since very few files are an exact multiple
		 * of the transfer block size, there will almost always be some gunk here.
		 * If we don't eat it now, RedBoot will think the user typed it.
		 */
		ZM_DEBUG (zm_dprintf ("Trailing gunk:\n"));
		while ((c = (*getc)()) > -1)
			ZM_DEBUG (zm_dprintf ("\n"));
		/*
		 * Make a small delay to give terminal programs like minicom
		 * time to get control again after their file transfer program
		 * exits.
		 */
		CYGACC_CALL_IF_DELAY_US(250000);
	}
}

const char* xyzModem_error(int err) {
	switch (err) {
	case xyzModem_access:
		return "Can't access file";
		break;
	case xyzModem_noZmodem:
		return "Sorry, zModem not available yet";
		break;
	case xyzModem_timeout:
		return "Timed out";
		break;
	case xyzModem_eof:
		return "End of file";
		break;
	case xyzModem_cancel:
		return "Cancelled";
		break;
	case xyzModem_frame:
		return "Invalid framing";
		break;
	case xyzModem_cksum:
		return "CRC/checksum error";
		break;
	case xyzModem_sequence:
		return "Block sequence error";
		break;
	default:
		return "Unknown error";
		break;
	}
}
