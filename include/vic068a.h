#ifndef _VIC_H
#define _VIC_H

#define VIC_VIICR		0x3
#define VIC_CICR1		0x7
#define VIC_CICR2		0xb
#define VIC_CICR3		0xf
#define VIC_CICR4		0x13
#define VIC_CICR5		0x17
#define VIC_CICR6		0x1b
#define VIC_CICR7		0x1f
#define VIC_DMASR		0x23
#define VIC_LICR1		0x27
#define VIC_LICR2		0x2b
#define VIC_LICR3		0x2f
#define VIC_LICR4		0x33
#define VIC_LICR5		0x37
#define VIC_LICR6		0x3b
#define VIC_LICR7		0x3f
#define VIC_LICRn_STATE	(1 << 3)
#define VIC_LICRn_MASK	(1 << 7)
#define VIC_ICGSICR		0x43
#define VIC_ICMSICR		0x47
#define VIC_EGICR		0x4b
#define VIC_ICGSVBR		0x4f
#define VIC_ICMSVBR		0x53
#define VIC_LIVBR		0x57
#define VIC_EGIVBR		0x5b
#define VIC_ICSR		0x5f

#ifndef __ASSEMBLY__
static inline int vic_lirq_state(void *base, unsigned int which)
{
	u8 val;

	val = readb(base + which);
	val &= VIC_LICRn_STATE;

	return val ? 1 : 0;
}

static inline void vic_lirq_unmask(void *base, unsigned int which)
{
	u8 val;

	val = readb(base + which);
	val &= ~VIC_LICRn_MASK;
	writeb(val, base + which);
}

static inline void vic_lirq_mask(void *base, unsigned int which)
{
	u8 val;

	val = readb(base + which);
	val |= VIC_LICRn_MASK;
	writeb(val, base + which);
}
#endif

#endif
