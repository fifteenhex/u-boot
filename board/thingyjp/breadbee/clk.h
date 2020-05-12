/*
 * clk.h
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#ifndef BOARD_THINGYJP_BREADBEE_CLK_H_
#define BOARD_THINGYJP_BREADBEE_CLK_H_

void cpu_clk_setup(void);
void mstar_early_clksetup(void);
void mstar_bump_cpufreq(void);

#endif /* BOARD_THINGYJP_BREADBEE_CLK_H_ */
