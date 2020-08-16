/*
 */

#ifndef BOARD_THINGYJP_BREADBEE_DDR_H_
#define BOARD_THINGYJP_BREADBEE_DDR_H_

enum mstar_dram_type {
	MSTAR_DRAM_DDR1,
	MSTAR_DRAM_DDR2,
	MSTAR_DRAM_DDR3,
};

void mstar_ddr_init(int chiptype, struct regmap *ana, struct regmap *dig);

#endif
