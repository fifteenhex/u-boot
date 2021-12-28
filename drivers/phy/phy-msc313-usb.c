// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/device_compat.h>
#include <dt-bindings/phy/phy.h>
#include <generic-phy.h>
#include <syscon.h>
#include <time.h>
#include <regmap.h>
#include <linux/usb/mstar_usbc.h>
#include <linux/usb/mstar_utmi.h>

#define NUM_PORTS 2
#define PORT_UHC 0
#define PORT_OTG 1

struct msc313_usb_phy {
	struct phy *ports[NUM_PORTS];
	struct regmap *utmi;
	struct regmap *usbc;
};

static int msc313_usb_phy_init(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct msc313_usb_phy *msc313_usb_phy = dev_get_priv(dev);

	if (!msc313_usb_phy)
		return -ENODEV;

	return 0;
}

static const struct phy_ops msc313_usb_phy_ops = {
	.init = msc313_usb_phy_init,
};

static void msc313_usb_phy_switch_port(struct udevice *dev, struct msc313_usb_phy *phy)
{
	dev_info(dev, "Switching to UHC port\n");
	regmap_update_bits(phy->usbc, MSTAR_USBC_REG_PRTCTRL,
				MSTAR_PRTCTRL_OTG | MSTAR_PRTCTRL_UHC, 0);
	regmap_update_bits(phy->usbc, MSTAR_USBC_REG_PRTCTRL,
				MSTAR_PRTCTRL_UHC, MSTAR_PRTCTRL_UHC);
	// not sure what this is, might be MAC power down
	//regmap_update_bits(msc313_usb_phy->usbc, MSTAR_USBC_REG_RSTCTRL,
	//			BIT(MSTAR_RSTCTRL_VBUSVALID),
	//			0);

}

static void msc313_usb_phy_do_calibration(struct udevice *dev, struct msc313_usb_phy *phy)
{
	unsigned int calval;

	dev_info(dev, "starting calibration...\n");
	regmap_update_bits(phy->utmi, MSTAR_UTMI_REG_CAL,
			MSTAR_UTMI_REG_CAL_START, MSTAR_UTMI_REG_CAL_START);
	mdelay(1);
	regmap_update_bits(phy->utmi, MSTAR_UTMI_REG_CAL,
			MSTAR_UTMI_REG_CAL_START, 0);
	if(regmap_read_poll_timeout(phy->utmi, MSTAR_UTMI_REG_CAL,
			calval, calval & MSTAR_UTMI_REG_CAL_END, 0, 1000000)){
		dev_info(dev, "calibration timeout\n");
	}
	else {
		calval >>= MSTAR_UTMI_REG_CAL_DATA_SHIFT;
		if(calval > 0 && calval < 0xfff)
			dev_info(dev, "calibration finished.\n");
		else
			dev_warn(dev, "calibration failed.\n");
	}
}

static int msc313_usb_phy_probe(struct udevice *dev)
{
	struct msc313_usb_phy *msc313_usb_phy = dev_get_priv(dev);
	int i;
	bool swap = false;

	msc313_usb_phy->utmi = syscon_regmap_lookup_by_phandle(dev, "mstar,utmi");
	if(IS_ERR(msc313_usb_phy->utmi)){
		return PTR_ERR(msc313_usb_phy->utmi);
	}

	msc313_usb_phy->usbc = syscon_regmap_lookup_by_phandle(dev, "mstar,usbc");
	if (IS_ERR(msc313_usb_phy->usbc))
		return PTR_ERR(msc313_usb_phy->usbc);

	// hack for m5, these seem to be the reset values for i3
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_RSTCTRL,
			0x228);
	regmap_write(msc313_usb_phy->utmi, MSTAR_UTMI_REG_PLL_TEST1,
				0x2088);
	regmap_write(msc313_usb_phy->utmi, MSTAR_UTMI_REG_PLL_TEST0,
				0x8051);
	regmap_write(msc313_usb_phy->utmi, MSTAR_UTMI_REG_CONFIG,
			0x2084);

	//FIXME for some reason this doesn't update the registers.
	// some voodoo that is enabled for the msc313 in the vendor sdk
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_MIUCFG0,
					0x0);
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_MIUCFG1,
					0xffff);
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_MIUCFG2,
					BIT(8) | 0xff);

	// clear any existing interrupts and then enable
	// the interrupt
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_INTEN,
			MSTAR_USBC_INT_MASK);
	regmap_write(msc313_usb_phy->usbc, MSTAR_USBC_REG_INTSTS,
                        MSTAR_USBC_INT_MASK);

	// power up hacks
	regmap_write(msc313_usb_phy->utmi, REG_CLKCTRL, 0x0c2f);
	regmap_write(msc313_usb_phy->utmi, REG_CLKCTRL, 0x040f);
	regmap_write(msc313_usb_phy->utmi, REG_PWRCTRL, 0x7f05);

	msc313_usb_phy_switch_port(dev, msc313_usb_phy);

	regmap_write(msc313_usb_phy->utmi, REG_CLKCTRL, 0x0426);
	regmap_write(msc313_usb_phy->utmi, REG_PWRCTRL, 0x6bc3);
	regmap_write(msc313_usb_phy->utmi, REG_PWRCTRL, 0x69c3);
	regmap_write(msc313_usb_phy->utmi, REG_PWRCTRL, 0x0001);

	regmap_write(msc313_usb_phy->utmi, REG_EYESETTING1, 0x0210);
	regmap_write(msc313_usb_phy->utmi, REG_EYESETTING2, 0x8100);


	msc313_usb_phy_do_calibration(dev, msc313_usb_phy);

	//swap = of_property_read_bool(dev_ofnode(dev), "mstar,utmi-dxswap");

	if(swap)
		dev_info(dev, "enabling data line swap");
	regmap_update_bits(msc313_usb_phy->utmi, MSTAR_UTMI_REG_CLKINV,
			MSTAR_UTMI_REG_CLKINV_DPDNSWP, swap ? MSTAR_UTMI_REG_CLKINV_DPDNSWP : 0);

	//regmap_update_bits(msc313_usb_phy->regmap, REG_PWRCTRL, PWRCTRL_UPLL_PDN, 0);

	for(i = 0; i < NUM_PORTS; i++){
	}

	return 0;
}

static const struct udevice_id msc313_usb_phy_ids[] = {
	{ .compatible = "mstar,msc313-usb-phy", },
	{ /* end of list */ },
};

U_BOOT_DRIVER(msc313_usb_phy) = {
	.name		= "msc313_usb_phy",
	.id		= UCLASS_PHY,
	.of_match	= msc313_usb_phy_ids,
	.probe		= msc313_usb_phy_probe,
	.ops		= &msc313_usb_phy_ops,
	.priv_auto	= sizeof(struct msc313_usb_phy),
};
