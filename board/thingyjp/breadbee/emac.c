/*
 * emac.c
 *
 *  Created on: 3 May 2020
 *      Author: daniel
 */

#include <common.h>

#define EMAC_RIU_REG_BASE           (0x1F000000)
#define REG_BANK_ALBANY0                    0x0031
#define REG_BANK_ALBANY1                    0x0032
#define REG_BANK_ALBANY2                    0x0033
static void MHal_EMAC_WritReg8( u32 bank, u32 reg, u8 val )
{
    u32 address = EMAC_RIU_REG_BASE + bank*0x100*2;
    address = address + (reg << 1) - (reg & 1);

    *( ( volatile u8* ) address ) = val;
}

void emacphypowerup_msc313(void){
	printf("emac power up, msc313\n");

	//gain shift
	MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb4, 0x02);

	    //det max
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x4f, 0x02);

	    //det min
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x51, 0x01);

	    //snr len (emc noise)
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x77, 0x18);

	    //lpbk_enable set to 0
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x72, 0xa0);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xfc, 0x00);   // Power-on LDO
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xfd, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb7, 0x17);   // Power-on ADC**
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcb, 0x11);   // Power-on BGAP
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcc, 0x20);   // Power-on ADCPL
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xcd, 0xd0);   // Power-on ADCPL
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xd4, 0x00);   // Power-on LPF_OP
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xb9, 0x40);   // Power-on LPF
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xbb, 0x05);   // Power-on REF
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x3a, 0x03);   // PD_TX_IDAC, PD_TX_LD = 0
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x3b, 0x00);


	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x3b, 0x01);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xa1, 0xc0);// PD_SADC, EN_SAR_LOGIC**
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x8a, 0x01);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xc4, 0x44);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x80, 0x30);

	    //100 gat
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xc5, 0x00);

	    //200 gat
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x30, 0x43);

	    //en_100t_phase
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0x39, 0x41);   // en_100t_phase;  [6] save2x_tx

	    // Prevent packet drop by inverted waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x79, 0xd0);   // prevent packet drop by inverted waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x77, 0x5a);

	    //disable eee
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2d, 0x7c);   // disable eee

	    //10T waveform
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x06);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2b, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0x2b, 0x00);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x06);   // shadow_ctrl
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaa, 0x1c);   // tin17_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xac, 0x1c);   // tin18_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xad, 0x1c);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xae, 0x1c);   // tin19_s2
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaf, 0x1c);

	    MHal_EMAC_WritReg8(REG_BANK_ALBANY2, 0xe8, 0x00);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xaa, 0x1c);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY0, 0xab, 0x28);

	    //speed up timing recovery
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xf5, 0x02);

	    // Signal_det k
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x0f, 0xc9);

	    // snr_h
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x89, 0x50);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x8b, 0x80);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x8e, 0x0e);
	    MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0x90, 0x04);

	    //set CLKsource to hv
	       MHal_EMAC_WritReg8(REG_BANK_ALBANY1, 0xC7, 0x80);
}

void emacphypowerup_msc313e(void){
	 printf("emac power up, msc313e\n");
	     // gain shift
	     *(int8_t *)0x1f006568 = 0x2;

	     // det max
	     *(int8_t *)0x1f00649d = 0x2;

	     // det min
	     *(int8_t *)0x1f0064a1 = 0x1;

	     //snr len(emc noise)
	     *(int8_t *)0x1f0064ed = 0x18;

	     // lpbk_enable set to 0
	     *(int8_t *)0x1f0062e4 = 0xa0;

	     // power on ldo
	     *(int8_t *)0x1f0065f8 = 0x0;
	     *(int8_t *)0x1f0065f9 = 0x0;
	     // power on adc
	     *(int8_t *)0x1f006741 = 0x80;
	     // power on bgap
	     *(int8_t *)0x1f006598 = 0x40;
	     // power on adcpl
	     *(int8_t *)0x1f006575 = 0x4;
	     *(int8_t *)0x1f006674 = 0x0;
	     // power on lpf_op
	     *(int8_t *)0x1f0067e1 = 0x0;

	     // lpf
	     *(int8_t *)0x1f006714 = 0x1;


	     *(int8_t *)0x1f006475 = 0x1;
	     *(int8_t *)0x1f006588 = 0x44;
	     *(int8_t *)0x1f006700 = 0x30;
	     *(int8_t *)0x1f006789 = 0x0;
	     *(int8_t *)0x1f006660 = 0x43;

	     // 100 gat
	     *(int8_t *)0x1f006671 = 0x41;

	     // 200 gat
	     *(int8_t *)0x1f0067e4 = 0xf5;

	     // en_100t_phase
	     *(int8_t *)0x1f0067e5 = 0xd;

	     // prevent packet drop by inverted waveform
	     *(int8_t *)0x1f0062f1 = 0xd0;
	     *(int8_t *)0x1f0062ed = 0x5a;

	     // disable eee
	     *(int8_t *)0x1f006259 = 0x7c;

	     // 10T waveform
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006255 = 0x0;
	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006255 = 0x0;

	     // shadow_ctrl
	     *(int8_t *)0x1f0067d0 = 0x6;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006358 = 0x1c;
	     *(int8_t *)0x1f006359 = 0x1c;
	     *(int8_t *)0x1f00635c = 0x1c;
	     *(int8_t *)0x1f00635d = 0x1c;


	     *(int8_t *)0x1f0067d0 = 0x0;
	     *(int8_t *)0x1f006354 = 0x1c;
	     *(int8_t *)0x1f006355 = 0x28;

	     // speed up timing recovery
	     *(int8_t *)0x1f0065e9 = 0x2;

	     // signal_det ket
	     *(int8_t *)0x1f00641d = 0xc9;

	     // snr h
	     *(int8_t *)0x1f006511 = 0x50;
	     *(int8_t *)0x1f006515 = 0x80;
	     *(int8_t *)0x1f00651c = 0xe;
	     *(int8_t *)0x1f006520 = 0x4;

	     // pinctrl
	     *(int8_t *)0x1f203d41 = *(int8_t *)0x1f203d41 & 0x7f;



	     *(int8_t *)0x1f001ca0 = *(int8_t *)0x1f001ca0 & 0xcf | 0x10;
}

void emac_patches(void){
	printf("emac patches\n");

	// this is "switch rx descriptor format to mode 1"
	*(int8_t *)0x1f2a2274 = 0x0;
	*(int8_t *)0x1f2a2275 = 0x1;

	// RX shift patch
	*(int8_t *)0x1f2a2200 = *(int8_t *)0x1f2a2200 | 0x10;

	// TX underrun patch
	*(int8_t *)0x1f2a2271 = *(int8_t *)0x1f2a2271 | 0x1;

	// clkgen setup
	*(int8_t *)0x1f207108 = 0x0;
	*(int8_t *)0x1f226688 = 0x0;
	*(int8_t *)0x1f22668c = 0x0;

	*(u16*)(0x1f2a2000 + 0x200) = 0xF051; // mstar call this julian100, magic number, seems to be related to the phy
	*(u16*)(0x1f2a2000 + 0x204) = 0x0000;
	*(u16*)(0x1f2a2000 + 0x208) = 0x0001; // mstar call this julian104, this enables software descriptors apparently
	*(u16*)(0x1f2a2000 + 0x20c) = 0x0000;
}
