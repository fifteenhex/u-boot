#ifndef _CD2401_H
#define _CD2401_H

#define CD2401_LIV			0x09
#define CD2401_IER			0x11
#define CD2401_IER_TXD		BIT(0)
#define CD2401_IER_TXEMPTY	BIT(1)
#define CD2401_IER_RXD		BIT(3)
#define CD2401_CCR			0x13
#define CD2401_CCR_ENBXMTR	BIT(3)
#define CD2401_CCR_ENBRX	BIT(1)
#define CD2401_CSR			0x1a
#define CD2401_CMR			0x1b
#define CD2401_CMR_ASYNC	0x2
#define CD2401_LICR			0x26
#define CD2401_RFOC			0x30
#define CD2401_TFTC			0x80
#define CD2401_REOIR		0x84
#define CD2401_REOIR_NOTRANS BIT(3)
#define CD2401_TEOIR		0x85
#define CD2401_TEOIR_NOTRANS BIT(3)
#define CD2401_RISRH		0x88
#define CD2401_RISRL		0x89
#define CD2401_TISR			0x8a
#define CD2401_TISR_TXEMPTY BIT(1)
#define CD2401_TPILR		0xe0
#define CD2401_RPILR		0xe1
#define CD2401_TIR			0xec
#define CD2401_TIR_TACT		BIT(6)
#define CD2401_RIR			0xed
#define CD2401_RIR_RACT		BIT(6)
#define CD2401_RIR_REN		BIT(7)
#define CD2401_CAR			0xee
#define CD2401_TDR			0xf8
#define CD2401_RDR			CD2401_TDR

#define CD2401_FIFO_SZ		16

#define CD2401_VECTOR_TYPE_MASK		0x3
#define CD2401_VECTOR_TYPE_RXEXCP	0b00
#define CD2401_VECTOR_TYPE_MDM		0b01
#define CD2401_VECTOR_TYPE_TXD		0b10
#define CD2401_VECTOR_TYPE_RXD		0b11

#ifndef __ASSEMBLY__
static inline unsigned int cd2401_interrupting_port(void *base)
{
	return (readb(base + CD2401_LICR) >> 2) & 0x3;
}

static inline unsigned int cd2401_vector_type(u8 vector)
{
	return (vector & CD2401_VECTOR_TYPE_MASK);
}
#endif

#endif
