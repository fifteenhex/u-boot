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

#define CORNAK(__xyz) putc((__xyz->crc_mode ? 'C' : NAK));

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

	/* stats */
	int opened;
	int total_RX;
	int total_SOH;
	int total_STX;
	int total_CAN;

	unsigned char next_blk; /* Expected block */
	int len, mode, total_retries;

	bool crc_mode, at_eof;
	bool first_xmodem_packet;
	ulong initial_time, timeout;
	unsigned long file_length, read_length;

	/* The last packet needs to be ack'd */
	bool tx_ack;
};

static struct xyz _xyz;

/* 2 seconds */
#define xyzModem_CHAR_TIMEOUT            1000
#define xyzModem_MAX_RETRIES             20
#define xyzModem_MAX_RETRIES_WITH_CRC    10
/* Wait for 3 CAN before quitting */
#define xyzModem_CAN_COUNT                3

/* Get a char but don't wait forever */
static int xyzModem_getchar(struct xyz *xyz, char *c) {
	ulong now = get_timer(0);

	schedule();
	while (true) {
		if (tstc()) {
			int ret = getchar();
			if (ret >= 0) {
				*c = ret;
				xyz->total_RX++;
				return 1;
			} else if (ret != -EAGAIN)
				return 0;
		}
		if (get_timer(now) > xyzModem_CHAR_TIMEOUT)
			break;
	}

	return 0;
}

static int xyzModem_getchars(struct xyz *xyz, char *c, unsigned int n) {
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
			if (ret >= 0) {
				xyz->total_RX++;
				c[charsread++] = ret;
			}
			else if (ret != -EAGAIN)
				return ret;
		}
		if (get_timer(now) > xyzModem_CHAR_TIMEOUT * blksz)
			break;
	}

	return charsread;
}

/* Validate a hex character */
static inline bool _is_hex(char c) {
	return (((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F'))
			|| ((c >= 'a') && (c <= 'f')));
}

/* Convert a single hex nibble */
static inline int _from_hex(char c) {
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
static inline char _tolower(char c) {
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

/* Wait for the line to go idle */
static void xyzModem_flush(struct xyz *xyz) {
	int res;
	char c;
	while (true) {
		res = xyzModem_getchar(xyz, &c);
		if (!res)
			return;
	}
}

static int xyzModem_sync_packet_start(struct xyz *xyz) {
	char c;
	int res;
	bool hdr_found = false;
	/* Find the start of a header */
	int can_total, hdr_chars;
	can_total = 0;
	hdr_chars = 0;

	while (!hdr_found) {
		res = xyzModem_getchar(xyz, &c);
		//ZM_DEBUG(zm_save(c));
		if (res) {
			hdr_chars++;
			switch (c) {
			case SOH:
				xyz->total_SOH++;
			case STX:
				if (c == STX)
					xyz->total_STX++;
				hdr_found = true;
				break;
			case CAN:
			case ETX:
				xyz->total_CAN++;
				//ZM_DEBUG(zm_dump(__LINE__));
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
					log_info("ACK on EOT #%d\n", __LINE__);
					return xyzModem_eof;
				}
			default:
				/* Ignore, waiting for start of header */
				;
			}
		} else {
			/* Data stream timed out */
			xyzModem_flush(xyz); /* Toss any current input */
			udelay(250000);
			return xyzModem_timeout;
		}
	}

	if (hdr_found) {
		xyz->len = (c == SOH) ? 128 : 1024;
		//ZM_DEBUG(zm_dprintf("Header found, %d\n", xyz->len));
	}

	return 0;
}

static int xyzModem_read_block(struct xyz *xyz)
{
	int ret;

	ret = xyzModem_getchar(xyz, &xyz->blk);
	if (ret == 0)
		return xyzModem_timeout;

	ret = xyzModem_getchar(xyz, &xyz->cblk);
	if (ret == 0)
		return xyzModem_timeout;

	return 0;
}

static int xyzModem_read_data(struct xyz *xyz)
{
	return xyzModem_getchars(xyz, xyz->pkt, xyz->len);
}

static int xyzModem_read_checksum(struct xyz *xyz)
{
	int res;

	res = xyzModem_getchar(xyz, &xyz->crc1);
	if (!res)
		return xyzModem_timeout;

	if (xyz->crc_mode) {
		res = xyzModem_getchar(xyz, &xyz->crc2);
		if (!res)
			return xyzModem_timeout;
	}

	return 0;
}

static int xyzModem_validate_message(const struct xyz *xyz)
{

	/* Validate the block number */
	if ((xyz->blk ^ xyz->cblk) != 0xFF) {
		return xyzModem_frame;
	}

	/* Verify checksum/CRC */
	if (xyz->crc_mode) {
		u16 cksum = crc16_ccitt(0, xyz->pkt, xyz->len);
		if (cksum != ((xyz->crc1 << 8) | xyz->crc2)) {
			log_err("CRC error - recvd: %02x%02x, computed: %x\n",
							xyz->crc1, xyz->crc2, cksum & 0xFFFF);
			return xyzModem_cksum;
		}
	} else {
		int i;
		unsigned short cksum = 0;
		for (i = 0; i < xyz->len; i++) {
			cksum += xyz->pkt[i];
		}
		if (xyz->crc1 != (cksum & 0xFF)) {
			log_err("Checksum error - recvd: %x, computed: %x\n", xyz->crc1,
							cksum & 0xFF);
			return xyzModem_cksum;
		}
	}

	return 0;
}

static int xyzModem_get_hdr(struct xyz *xyz) {
	int res;

	/* If we need to ACK the previous packet do that now */
	if (xyz->tx_ack) {
		putc(ACK);
		xyz->tx_ack = false;
	}

	/* Sync with the start of the next packet */
	res = xyzModem_sync_packet_start(xyz);
	if (res)
		goto err;

	res = xyzModem_read_block(xyz);
	if (res)
		goto err;

	res = xyzModem_read_data(xyz);
	if (res <= 0)
		goto err;

	res = xyzModem_read_checksum(xyz);
	if (res)
		goto err;

	res = xyzModem_validate_message(xyz);
	if (res)
		goto err;

	/* If we get here, the message passes [structural] muster */
	return 0;

err:
	//log_err("Failed to get header: %d\n", res);
	xyzModem_flush(xyz);
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

static void xyzModem_parse_ymodemhdr(struct xyz *xyz)
{
	char *bufp = xyz->pkt;
	/* skip filename */
	while (*bufp++)
		;
	/* get the length */
	parse_num(bufp, &xyz->file_length, NULL, " ");
	/* The rest of the file name data block quietly discarded */
	xyz->tx_ack = true;
}

int xyzModem_stream_open(connection_info_t *info) {
	struct xyz *xyz = &_xyz;
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

	/* Set default state */
	memset(xyz, 0, sizeof(*xyz));
	xyz->crc_mode = true;
	xyz->mode = info->mode;
	xyz->initial_time = get_timer(0);
	xyz->timeout = xyzModem_get_initial_timeout();

	/* Tell the sender we are here */
	CORNAK(xyz);

	/* X-modem doesn't have an information header - exit here */
	if (xyz->mode == xyzModem_xmodem) {
		xyz->first_xmodem_packet = true;
		xyz->next_blk = 1;
		goto opened;
	}

	/* For ymodem get the information header */
	while (!(xyz->timeout && get_timer(xyz->initial_time) > xyz->timeout)) {
		if (--retries <= 0) {
			retries = xyzModem_MAX_RETRIES;
			crc_retries = xyzModem_MAX_RETRIES_WITH_CRC;
			xyz->crc_mode = true;
		}

		stat = xyzModem_get_hdr(xyz);
		if (!stat) {
			/* Y-modem file information header */
			if (xyz->blk == 0)
				xyzModem_parse_ymodemhdr(xyz);
			xyz->next_blk = 1;
			xyz->len = 0;

			goto opened;
		} else if (stat == xyzModem_timeout) {
			if (--crc_retries <= 0)
				xyz->crc_mode = false;

			/* Extra delay for startup */
			udelay(5 * 100000);

			CORNAK(xyz);
			xyz->total_retries++;
			log_debug("NAK (%d)\n", __LINE__);
		}
		else if (stat == xyzModem_cancel)
			return stat;
	}

	return -1;

opened:
	xyz->opened++;
	return 0;
}

static int xyzModem_stream_read_handle_valid_pkt(struct xyz *xyz)
{
	if (xyz->mode == xyzModem_xmodem && xyz->first_xmodem_packet)
		xyz->first_xmodem_packet = false;

	if (xyz->blk != xyz->next_blk) {
		/* We got a repeat of the previous packet */
		if (xyz->blk == ((xyz->next_blk - 1) & 0xFF))
			return xyzModem_repeat;

		return xyzModem_sequence;
	}

	xyz->tx_ack = true;
	//ZM_DEBUG (zm_dprintf
	//	("ACK block %d (%d)\n", xyz->blk, __LINE__));
	xyz->next_blk = (xyz->next_blk + 1) & 0xFF;
	if (xyz->mode == xyzModem_xmodem
			|| xyz->file_length == 0) {
		/* Data blocks can be padded with ^Z (EOF) characters */
		/* This code tries to detect and remove them */
		char *bufp = xyz->pkt;
		if ((bufp[xyz->len - 1] == EOF)
				&& (bufp[xyz->len - 2] == EOF)
				&& (bufp[xyz->len - 3] == EOF)) {
			while (xyz->len && (bufp[xyz->len - 1] == EOF)) {
				xyz->len--;
			}
		}
	}

	/*
	 * See if accumulated length exceeds that of the file.
	 * If so, reduce size (i.e., cut out pad bytes)
	 * Only do this for Y-modem (and Z-modem should it ever
	 * be supported since it can fall back to Y-modem mode).
	 */
	if (xyz->mode != xyzModem_xmodem
			&& 0 != xyz->file_length) {
		xyz->read_length += xyz->len;
		if (xyz->read_length > xyz->file_length) {
			xyz->len -= (xyz->read_length - xyz->file_length);
		}
	}

	return 0;
}

int xyzModem_stream_read(char *buf, int size) {
	struct xyz *xyz = &_xyz;
	int stat, total, len;
	int retries;

	total = 0;
	stat = xyzModem_cancel;

	/* Try and get 'size' bytes into the buffer */
	while (!xyz->at_eof && xyz->len >= 0 && (size > 0)) {
		if (xyz->len == 0) {
			retries = xyzModem_MAX_RETRIES;

			while (retries-- > 0) {
				if (xyz->first_xmodem_packet && xyz->timeout
						&& get_timer(xyz->initial_time) > xyz->timeout) {
					xyz->len = -1;
					return xyzModem_timeout;
				}

				stat = xyzModem_get_hdr(xyz);
				if (!stat) {
					stat = xyzModem_stream_read_handle_valid_pkt(xyz);

					/* Packet handled, get outta here */
					if (!stat)
						break;

					/*
					 * Sender resent the last packet, ACK it and do another
					 * loop to get the packet we actually wanted
					 */
					if (stat == xyzModem_repeat) {
						putc(ACK);
						continue;
					}
				}

				if (stat == xyzModem_cancel)
					break;

				if (stat == xyzModem_eof) {
					putc(ACK);
					log_info("ACK (%d)\n", __LINE__);
					if (xyz->mode == xyzModem_ymodem) {
						CORNAK(xyz);
						xyz->total_retries++;
						log_info("Reading Final Header\n");
						stat = xyzModem_get_hdr(xyz);
						putc(ACK);
						log_info("FINAL ACK (%d)\n", __LINE__);
					} else
						stat = 0;
					xyz->at_eof = true;
					break;
				}

				CORNAK(xyz);
				xyz->total_retries++;
				//log_err("NAK (%d,%d)\n", __LINE__, len);
			}
			if (stat < 0 && (!xyz->first_xmodem_packet
					|| stat != xyzModem_timeout)) {
				xyz->len = -1;
				return total;
			}
		}
		/* Don't "read" data from the EOF protocol package */
		if (!xyz->at_eof && xyz->len > 0) {
			len = xyz->len;
			if (size < len)
				len = size;
			memcpy(buf, xyz->pkt, len);
			size -= len;
			buf += len;
			total += len;
			xyz->len -= len;
		}
	}

	return total;
}

void xyzModem_stream_close() {
	struct xyz *xyz = &_xyz;

	log_err("xyzModem - %s mode, opened:%d %d(RX), %d(SOH)/%d(STX)/%d(CAN) packets, %d retries\n",
					xyz->crc_mode ? "CRC" : "Cksum",
					xyz->opened,
					xyz->total_RX,
					xyz->total_SOH,
					xyz->total_STX,
					xyz->total_CAN,
					xyz->total_retries);
}

/* Need to be able to clean out the input buffer, so have to take the */
/* getc */
void xyzModem_stream_terminate(bool abort, int (*getc)(void)) {
	struct xyz *xyz = &_xyz;
	int c;

	if (abort) {
		log_info("!!!! TRANSFER ABORT !!!!\n");
		switch (xyz->mode) {
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
			log_debug("Flushing serial line.\n");
			xyzModem_flush(xyz);
			xyz->at_eof = true;
			break;
#ifdef xyzModem_zmodem
		case xyzModem_zmodem:
	  /* Might support it some day I suppose. */
#endif
			break;
		}
	} else {
		log_info("Engaging cleanup mode...\n");
		/*
		 * Consume any trailing crap left in the inbuffer from
		 * previous received blocks. Since very few files are an exact multiple
		 * of the transfer block size, there will almost always be some gunk here.
		 * If we don't eat it now, RedBoot will think the user typed it.
		 */
		log_info("Trailing gunk:\n");
		while ((c = (*getc)()) > -1)
			log_info("\n");
		/*
		 * Make a small delay to give terminal programs like minicom
		 * time to get control again after their file transfer program
		 * exits.
		 */
		udelay(250000);
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
