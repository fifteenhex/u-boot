/*
 */

#ifndef ARCH_ARM_MACH_MSTAR_INCLUDE_IPL_H_
#define ARCH_ARM_MACH_MSTAR_INCLUDE_IPL_H_


struct mstar_ipl {
	void* entry;
	char header[4];
	u16 size;
	u8 cid;
	u8 something;
	u32 chksum;
} __attribute__ ((packed));

#endif /* ARCH_ARM_MACH_MSTAR_INCLUDE_IPL_H_ */
