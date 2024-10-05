#ifndef _ASM_E17_H
#define _ASM_E17_H

#include "cd2401.h"
#include "vic068a.h"

#define E17_VIDEORAM			0x0fc00000
#define E17_OVERLAYRAM			0x0fe00000
#define E17_VIC					0xfec00000
#define E17_VIC_LICR6			(E17_VIC + VIC_LICR6)
#define E17_VMEBUSDECODER		0xfec08000
#define E17_USERCIO				0xfec10000
#define E17_NVRAMRTC			0xfec20000
#define E17_SYSTEMCIO			0xfec30000
#define E17_HEXDISP				E17_SYSTEMCIO
#define E17_VIDEOCONTROLLER		0xfec40000
#define E17_WATCHDOG			0xfec50000
#define E17_REVISIONEPPROM_IRQ	0xfec54000
#define E17_SECONDARYCPUCTRL	0xfec58000
#define E17_SLAVESELECT			0xfec5c000
#define E17_SNOOPCONTROL		0xfec5e000
#define E17_KEYCTRL				0xfec60000
#define E17_SERIAL				0xfec64000
#define E17_SERIAL_IER			(E17_SERIAL + CD2401_IER)
#define E17_SERIAL_CCR			(E17_SERIAL + CD2401_CCR)
#define E17_SERIAL_CSR			(E17_SERIAL + CD2401_CSR)
#define E17_SERIAL_CMR			(E17_SERIAL + CD2401_CMR)
#define E17_SERIAL_LICR			(E17_SERIAL + CD2401_LICR)
#define E17_SERIAL_TFTC			(E17_SERIAL + CD2401_TFTC)
#define E17_SERIAL_TEOIR		(E17_SERIAL + CD2401_TEOIR)
#define E17_SERIAL_TISR			(E17_SERIAL + CD2401_TISR)
#define E17_SERIAL_CAR			(E17_SERIAL + CD2401_CAR)
#define E17_SERIAL_TDR			(E17_SERIAL + CD2401_TDR)

/* From the hardware manual:
 * For polled operation, the MPC can be switched to IACK context by
 * reading address $FEC6.6000 - $FEC6.7FFF. A0-A6 must match one of
 * the priority interrupt level registers of the MPC.
 */

#define E17_SERIAL_IACK			0xfec66000

#define E17_ILACC				0xfec68000
#define E17_SCSI				0xfec6c000
#define E17_IOC					0xfec70000

#ifndef __ASSEMBLY__
static inline void e17_sethexdisplay(u8 val)
{
	writeb(val & 0xf, E17_HEXDISP);
}
#endif

#endif
