// SPDX-License-Identifier: GPL-2.0
/*
 * Nuvoton MA35D1 USB 2.0 OTG PHY driver
 *
 * Copyright (C) 2024 Nuvoton Technology Corp.
 *
 * This driver initializes USB 2.0 PHYs in the MA35D1 SoC for host
 * mode (EHCI/OHCI).  It extends the original phy-ma35d1-usb2 gadget
 * PHY driver with:
 *
 *   - Support for both PHY0 and PHY1 via parameterized register macros
 *   - PHY index encoded as the phandle-arg of "nuvoton,sys = <&sys N>"
 *   - Host-mode clock stable bits (HSTCKSTB + CK12MSTB)
 *   - Optional resistor calibration trim (nuvoton,rcalcode)
 *   - Optional over-current detect polarity (nuvoton,oc-active-high)
 *
 * PHY0 (USB0) is shared between DWC2 gadget and EHCI0/OHCI0 host
 * controllers — the hardware switches via the USB ID pin.  PHY1
 * (USB1) is host-only.
 *
 * The driver checks whether the PHY is already operational before
 * running the Power-On Reset sequence, protecting against double
 * init when multiple consumers share the same PHY.
 */
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

/* SYS register offsets (relative to syscon base 0x40460000) */
#define MA35_SYS_USBPMISCR		0x60
#define MA35_SYS_MISCFCR0		0x70

/*
 * USBPMISCR register layout (n = PHY index, 0 or 1):
 *
 *  PHY0 (n=0): control bits [2:0], host stable [9:8], dev stable [10],
 *              RCALCODE [15:12]
 *  PHY1 (n=1): control bits [18:16], host stable [25:24],
 *              RCALCODE [31:28]
 *
 * All PHY1 fields are exactly 16 bit-positions above PHY0.
 *
 * SUSPEND bit semantics: 1 = operational mode, 0 = suspended.
 */
#define USBPMISCR_PHY_POR(n)		BIT(0 + (n) * 16)
#define USBPMISCR_PHY_SUSPEND(n)	BIT(1 + (n) * 16)
#define USBPMISCR_PHY_COMN(n)		BIT(2 + (n) * 16)
#define USBPMISCR_PHY_HSTCKSTB(n)	BIT(8 + (n) * 16)
#define USBPMISCR_PHY_CK12MSTB(n)	BIT(9 + (n) * 16)
#define USBPMISCR_PHY_DEVCKSTB(n)	BIT(10 + (n) * 16) /* PHY0 only */

/* Mask for control bits (POR, SUSPEND, COMN) of one PHY */
#define USBPMISCR_PHY_CTL_MASK(n)	(0x7 << ((n) * 16))

/* Host-mode ready: SUSPEND + HSTCKSTB + CK12MSTB */
#define USBPMISCR_PHY_HOST_READY(n)	(USBPMISCR_PHY_SUSPEND(n)  | \
					 USBPMISCR_PHY_HSTCKSTB(n) | \
					 USBPMISCR_PHY_CK12MSTB(n))

/* Device-mode ready: SUSPEND + DEVCKSTB (PHY0 only) */
#define USBPMISCR_PHY_DEV_READY(n)	(USBPMISCR_PHY_SUSPEND(n)  | \
					 USBPMISCR_PHY_DEVCKSTB(n))

/* RCALCODE: 4-bit resistor trim at bits [15:12] (PHY0) or [31:28] (PHY1) */
#define USBPMISCR_RCAL_SHIFT(n)		(12 + (n) * 16)
#define USBPMISCR_RCAL_MASK(n)		GENMASK(USBPMISCR_RCAL_SHIFT(n) + 3, \
						USBPMISCR_RCAL_SHIFT(n))

/* MISCFCR0[12]: USB host over-current detect polarity (shared, both ports) */
#define MISCFCR0_UHOVRCURH		BIT(12)

struct ma35_otg_phy {
	struct clk *clk;
	struct device *dev;
	struct regmap *sysreg;
	unsigned int phy_idx;	/* 0 or 1, from nuvoton,sys phandle arg */
};

/**
 * ma35_otg_phy_init - run PHY Power-On Reset sequence
 *
 * If the PHY is already operational (SUSPEND set and host clocks stable),
 * the POR is skipped.  This protects PHY0 from being re-initialized
 * when the gadget controller already brought it up.
 */
static int ma35_otg_phy_init(struct phy *phy)
{
	struct ma35_otg_phy *p = phy_get_drvdata(phy);
	unsigned int n = p->phy_idx;
	u32 ready_mask = USBPMISCR_PHY_HOST_READY(n);
	unsigned int val;
	int ret;

	/* If already operational with stable clocks, skip POR */
	regmap_read(p->sysreg, MA35_SYS_USBPMISCR, &val);
	if ((val & ready_mask) == ready_mask)
		return 0;

	/* Step 1: assert POR and set SUSPEND=1 (operational) */
	regmap_update_bits(p->sysreg, MA35_SYS_USBPMISCR,
			   USBPMISCR_PHY_CTL_MASK(n),
			   USBPMISCR_PHY_POR(n) | USBPMISCR_PHY_SUSPEND(n));
	msleep(20);

	/* Step 2: deassert POR, keep SUSPEND=1 */
	regmap_update_bits(p->sysreg, MA35_SYS_USBPMISCR,
			   USBPMISCR_PHY_CTL_MASK(n),
			   USBPMISCR_PHY_SUSPEND(n));

	/* Step 3: wait for host clock stable bits */
	ret = regmap_read_poll_timeout(p->sysreg, MA35_SYS_USBPMISCR, val,
				       (val & ready_mask) == ready_mask,
				       20000, 500000);
	if (ret) {
		dev_err(p->dev, "USB PHY%u clock not stable (USBPMISCR=0x%08x)\n",
			n, val);
		return ret;
	}

	return 0;
}

static int ma35_otg_phy_power_on(struct phy *phy)
{
	struct ma35_otg_phy *p = phy_get_drvdata(phy);

	return clk_prepare_enable(p->clk);
}

static int ma35_otg_phy_power_off(struct phy *phy)
{
	struct ma35_otg_phy *p = phy_get_drvdata(phy);

	clk_disable_unprepare(p->clk);
	return 0;
}

static const struct phy_ops ma35_otg_phy_ops = {
	.init = ma35_otg_phy_init,
	.power_on = ma35_otg_phy_power_on,
	.power_off = ma35_otg_phy_power_off,
	.owner = THIS_MODULE,
};

static int ma35_otg_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *provider;
	struct ma35_otg_phy *p;
	struct of_phandle_args args;
	struct phy *phy;
	u32 rcalcode;
	int ret;

	p = devm_kzalloc(&pdev->dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	p->dev = &pdev->dev;
	platform_set_drvdata(pdev, p);

	/*
	 * Parse "nuvoton,sys = <&sys N>" where N is the PHY index:
	 *   0 → PHY0 (USB0, shared with gadget)
	 *   1 → PHY1 (USB1, host-only)
	 *
	 * This follows the same pattern as the MA35D1 GMAC driver.
	 */
	ret = of_parse_phandle_with_fixed_args(pdev->dev.of_node,
					       "nuvoton,sys", 1, 0, &args);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse nuvoton,sys: %d\n", ret);
		return ret;
	}

	p->sysreg = syscon_node_to_regmap(args.np);
	p->phy_idx = args.args[0];
	of_node_put(args.np);

	if (IS_ERR(p->sysreg))
		return dev_err_probe(&pdev->dev, PTR_ERR(p->sysreg),
				     "Failed to get SYS regmap\n");

	if (p->phy_idx > 1) {
		dev_err(&pdev->dev, "invalid PHY index %u (must be 0 or 1)\n",
			p->phy_idx);
		return -EINVAL;
	}

	p->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(p->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(p->clk),
				     "failed to get PHY clock\n");

	/* Optional: resistor calibration trim code (4-bit, 0-15) */
	if (!of_property_read_u32(pdev->dev.of_node, "nuvoton,rcalcode",
				  &rcalcode)) {
		if (rcalcode > 15) {
			dev_err(&pdev->dev, "rcalcode %u out of range (0-15)\n",
				rcalcode);
			return -EINVAL;
		}
		regmap_update_bits(p->sysreg, MA35_SYS_USBPMISCR,
				   USBPMISCR_RCAL_MASK(p->phy_idx),
				   rcalcode << USBPMISCR_RCAL_SHIFT(p->phy_idx));
	}

	/*
	 * Optional: over-current detect polarity.
	 * MISCFCR0[12]: 0 = active-low (default), 1 = active-high.
	 * This is a shared bit affecting both USB host ports.
	 */
	if (of_property_read_bool(pdev->dev.of_node, "nuvoton,oc-active-high"))
		regmap_update_bits(p->sysreg, MA35_SYS_MISCFCR0,
				   MISCFCR0_UHOVRCURH, MISCFCR0_UHOVRCURH);

	phy = devm_phy_create(&pdev->dev, pdev->dev.of_node, &ma35_otg_phy_ops);
	if (IS_ERR(phy))
		return dev_err_probe(&pdev->dev, PTR_ERR(phy),
				     "Failed to create PHY\n");

	phy_set_drvdata(phy, p);

	provider = devm_of_phy_provider_register(&pdev->dev,
						 of_phy_simple_xlate);
	if (IS_ERR(provider))
		return dev_err_probe(&pdev->dev, PTR_ERR(provider),
				     "Failed to register PHY provider\n");

	dev_info(&pdev->dev, "USB PHY%u initialized (host mode)\n", p->phy_idx);

	return 0;
}

static const struct of_device_id ma35_otg_phy_of_match[] = {
	{ .compatible = "nuvoton,ma35d1-usb2-phy-otg" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ma35_otg_phy_of_match);

static struct platform_driver ma35_otg_phy_driver = {
	.probe	= ma35_otg_phy_probe,
	.driver	= {
		.name		= "ma35d1-usb2-phy-otg",
		.of_match_table	= ma35_otg_phy_of_match,
	},
};
module_platform_driver(ma35_otg_phy_driver);

MODULE_DESCRIPTION("Nuvoton MA35D1 USB 2.0 OTG PHY driver");
MODULE_AUTHOR("Nuvoton Technology Corp.");
MODULE_LICENSE("GPL");
