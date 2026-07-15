/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Zilog 8530 SCC serial platform data
 */

#ifndef __SERIAL_SCC_H
#define __SERIAL_SCC_H

struct scc_serial_plat {
	unsigned long ctrl;	/* channel control register address */
	unsigned long data;	/* channel data register address */
};

#endif /* __SERIAL_SCC_H */
