// SPDX-License-Identifier: GPL-2.0
/*
 * Nuvoton MA35 series EHCI Host Controller driver
 *
 * Copyright (C) 2025 Nuvoton Technology Corp.
 *
 * Each EHCI instance initializes only its own USB PHY via direct
 * syscon register access to USBPMISCR.  PHY ownership is encoded as
 * the argument cell of the "nuvoton,sys" phandle property:
 *
 *   nuvoton,sys = <&sys 0>;
 *   nuvoton,sys = <&sys 1>;
 *
 * This ensures that enabling only ehci1 (USB host) does not disturb
 * PHY0, which may already be in use by the USB device/gadget controller.
 * If a PHY is already operational when the EHCI driver probes, the
 * reset sequence is skipped.
 *
 * Derived from ehci-ma35d1.c (vendor BSP, kernel 6.6) and
 * ehci-npcm7xx.c (upstream reference).
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "ehci.h"

#define DRIVER_DESC "Nuvoton MA35 EHCI driver"

/* SYS register offsets (relative to syscon base 0x40460000) */
#define MA35_SYS_USBPMISCR	0x60
#define MA35_SYS_MISCFCR0	0x70

/*
 * USBPMISCR register layout (n = PHY index, 0 or 1):
 *
 *  PHY0 (n=0): control bits 0-2, stable bits 8-9, RCALCODE bits 12-15
 *  PHY1 (n=1): control bits 16-18, stable bits 24-25, RCALCODE bits 28-31
 *
 * All PHY1 fields are exactly 16 bit-positions above PHY0 fields,
 * enabling a single set of parameterized macros for both PHYs.
 *
 * SUSPEND bit semantics: 1 = operational mode, 0 = suspended.
 * Ready condition: SUSPEND + HSTCKSTB + CK12MSTB all set.
 */
#define USBPMISCR_PHY_POR(n)		BIT(0 + (n) * 16)  /* Power-On Reset */
#define USBPMISCR_PHY_SUSPEND(n)	BIT(1 + (n) * 16)  /* 1=operational */
#define USBPMISCR_PHY_HSTCKSTB(n)	BIT(8 + (n) * 16)  /* 60 MHz clock stable */
#define USBPMISCR_PHY_CK12MSTB(n)	BIT(9 + (n) * 16)  /* 12 MHz clock stable */

#define USBPMISCR_PHY_READY(n)		(USBPMISCR_PHY_SUSPEND(n)  | \
					 USBPMISCR_PHY_HSTCKSTB(n) | \
					 USBPMISCR_PHY_CK12MSTB(n))

/* RCALCODE: 4-bit resistor trim at bits [12:15] (PHY0) or [28:31] (PHY1) */
#define USBPMISCR_RCAL_SHIFT(n)		(12 + (n) * 16)
#define USBPMISCR_RCAL_MASK(n) \
	GENMASK(USBPMISCR_RCAL_SHIFT(n) + 3, USBPMISCR_RCAL_SHIFT(n))

/* MISCFCR0[12]: USB host over-current detect polarity */
#define MISCFCR0_UHOVRCURH	BIT(12)	/* 1 = active-high, 0 = active-low */

struct ma35_ehci_priv {
	struct clk *clk;
	struct regmap *sysreg;
	int oc_active_level;
	u32 phy_idx;		/* 0 = PHY0 (may be shared w/ gadget), 1 = PHY1 */
};

#define hcd_to_ma35_priv(h) \
	((struct ma35_ehci_priv *)hcd_to_ehci(h)->priv)

static struct hc_driver __read_mostly ma35_ehci_hc_driver;

static const struct ehci_driver_overrides ma35_ehci_overrides __initconst = {
	.extra_priv_size = sizeof(struct ma35_ehci_priv),
};

/**
 * ma35_ehci_phy_init - bring up the USB PHY owned by this EHCI instance
 * @pdev: platform device
 *
 * Operates only on the PHY indicated by priv->phy_idx.  If the PHY
 * is already operational (SUSPEND + HSTCKSTB + CK12MSTB all set), the
 * reset sequence is skipped — this protects PHY0 when the USB gadget
 * controller has already initialized it.
 *
 * Init sequence mirrors the vendor BSP:
 *  1. Assert POR and set SUSPEND=1 (operational mode)
 *  2. Wait 20 ms, then deassert POR keeping SUSPEND=1
 *  3. Poll until HSTCKSTB + CK12MSTB are stable (up to 500 ms)
 */
static int ma35_ehci_phy_init(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ma35_ehci_priv *priv = hcd_to_ma35_priv(hcd);
	unsigned int n = priv->phy_idx;
	u32 ready = USBPMISCR_PHY_READY(n);
	unsigned int reg;
	int err;

	/*
	 * Skip init if the PHY is already operational with stable clocks.
	 * Guards against re-initializing PHY0 if the USB gadget driver
	 * has already run its own init sequence before ehci0 probed.
	 */
	regmap_read(priv->sysreg, MA35_SYS_USBPMISCR, &reg);
	if ((reg & ready) == ready)
		goto configure_oc;

	/* Step 1: assert POR and place PHY in operational mode */
	regmap_update_bits(priv->sysreg, MA35_SYS_USBPMISCR,
			   USBPMISCR_PHY_POR(n) | USBPMISCR_PHY_SUSPEND(n),
			   USBPMISCR_PHY_POR(n) | USBPMISCR_PHY_SUSPEND(n));
	msleep(20);

	/* Step 2: deassert POR, keep SUSPEND=1 */
	regmap_update_bits(priv->sysreg, MA35_SYS_USBPMISCR,
			   USBPMISCR_PHY_POR(n) | USBPMISCR_PHY_SUSPEND(n),
			   USBPMISCR_PHY_SUSPEND(n));

	/* Step 3: wait for host and 12 MHz clocks to stabilize */
	err = regmap_read_poll_timeout(priv->sysreg, MA35_SYS_USBPMISCR, reg,
				       (reg & ready) == ready,
				       20000, 500000);
	if (err) {
		dev_err(&pdev->dev,
			"USB PHY%u clock not stable (USBPMISCR=0x%08x)\n",
			n, reg);
		return err;
	}

configure_oc:
	/*
	 * Configure over-current detect polarity.
	 * MISCFCR0_UHOVRCURH (MISCFCR0[12]): 0 = active-low (default), 1 = active-high.
	 * Both USB host ports share this one bit; each EHCI probe sets it to
	 * ensure the board DT value wins regardless of probe order.
	 */
	regmap_update_bits(priv->sysreg, MA35_SYS_MISCFCR0, MISCFCR0_UHOVRCURH,
			   priv->oc_active_level ? MISCFCR0_UHOVRCURH : 0);

	return 0;
}

static int ma35_ehci_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct ma35_ehci_priv *priv;
	struct of_phandle_args args;
	struct resource *res;
	u32 rcalcode;
	unsigned int reg;
	int irq, err;

	if (usb_disabled())
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err)
		return err;

	hcd = usb_create_hcd(&ma35_ehci_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));
	if (!hcd)
		return -ENOMEM;

	platform_set_drvdata(pdev, hcd);
	priv = hcd_to_ma35_priv(hcd);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		err = PTR_ERR(priv->clk);
		dev_err(&pdev->dev, "failed to get clock: %d\n", err);
		goto err_put_hcd;
	}

	err = clk_prepare_enable(priv->clk);
	if (err)
		goto err_put_hcd;

	/*
	 * Parse "nuvoton,sys = <&sys N>" where N encodes the PHY index:
	 *   0 → ehci0, PHY0 (USB0, may be shared with gadget controller)
	 *   1 → ehci1, PHY1 (USB1, host-only)
	 *
	 * Using of_parse_phandle_with_fixed_args + syscon_node_to_regmap
	 * matches the pattern used by the MA35D1 GMAC driver and avoids
	 * a separate "nuvoton,phy-index" property.
	 */
	err = of_parse_phandle_with_fixed_args(pdev->dev.of_node,
					       "nuvoton,sys", 1, 0, &args);
	if (err) {
		dev_err(&pdev->dev, "failed to parse nuvoton,sys: %d\n", err);
		goto err_clk;
	}

	priv->phy_idx = args.args[0];
	priv->sysreg = syscon_node_to_regmap(args.np);
	of_node_put(args.np);

	if (IS_ERR(priv->sysreg)) {
		err = PTR_ERR(priv->sysreg);
		dev_err(&pdev->dev, "failed to get syscon: %d\n", err);
		goto err_clk;
	}

	if (priv->phy_idx > 1) {
		dev_err(&pdev->dev, "invalid PHY index %u in nuvoton,sys\n",
			priv->phy_idx);
		err = -EINVAL;
		goto err_clk;
	}

	/* Over-current polarity: 0 = active-low (default), 1 = active-high */
	if (of_property_read_u32(pdev->dev.of_node, "oc-active-level",
				 &priv->oc_active_level))
		priv->oc_active_level = 0;

	/*
	 * Optional per-PHY resistor trim calibration code (4 bits).
	 * Applied to RCALCODE0 (PHY0) or RCALCODE1 (PHY1) depending on
	 * priv->phy_idx.  If absent, the hardware default is used.
	 */
	if (!of_property_read_u32(pdev->dev.of_node, "nuvoton,rcalcode",
				  &rcalcode)) {
		unsigned int n = priv->phy_idx;

		regmap_read(priv->sysreg, MA35_SYS_USBPMISCR, &reg);
		reg = (reg & USBPMISCR_RCAL_MASK(n)) |
		      ((rcalcode & 0xf) << USBPMISCR_RCAL_SHIFT(n));
		regmap_write(priv->sysreg, MA35_SYS_USBPMISCR, reg);
	}

	/* Bring up only this EHCI's own PHY */
	err = ma35_ehci_phy_init(pdev);
	if (err)
		goto err_clk;

	hcd->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(hcd->regs)) {
		err = PTR_ERR(hcd->regs);
		goto err_clk;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	hcd_to_ehci(hcd)->caps = hcd->regs;

	err = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (err)
		goto err_clk;

	device_wakeup_enable(hcd->self.controller);
	return 0;

err_clk:
	clk_disable_unprepare(priv->clk);
err_put_hcd:
	usb_put_hcd(hcd);
	return err;
}

static void ma35_ehci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ma35_ehci_priv *priv = hcd_to_ma35_priv(hcd);

	usb_remove_hcd(hcd);
	clk_disable_unprepare(priv->clk);
	usb_put_hcd(hcd);
}

static int __maybe_unused ma35_ehci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ma35_ehci_priv *priv = hcd_to_ma35_priv(hcd);
	int ret;

	ret = ehci_suspend(hcd, false);
	if (ret)
		return ret;

	clk_disable_unprepare(priv->clk);
	return 0;
}

static int __maybe_unused ma35_ehci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ma35_ehci_priv *priv = hcd_to_ma35_priv(hcd);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err)
		return err;

	ehci_resume(hcd, false);
	return 0;
}

static SIMPLE_DEV_PM_OPS(ma35_ehci_pm_ops, ma35_ehci_suspend,
			  ma35_ehci_resume);

static const struct of_device_id ma35_ehci_of_match[] = {
	{ .compatible = "nuvoton,ma35d1-ehci" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ma35_ehci_of_match);

static struct platform_driver ma35_ehci_driver = {
	.probe		= ma35_ehci_probe,
	.remove		= ma35_ehci_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "ma35-ehci",
		.pm	= pm_ptr(&ma35_ehci_pm_ops),
		.of_match_table = ma35_ehci_of_match,
	},
};

static int __init ma35_ehci_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	ehci_init_driver(&ma35_ehci_hc_driver, &ma35_ehci_overrides);
	return platform_driver_register(&ma35_ehci_driver);
}
module_init(ma35_ehci_init);

static void __exit ma35_ehci_cleanup(void)
{
	platform_driver_unregister(&ma35_ehci_driver);
}
module_exit(ma35_ehci_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Nuvoton Technology Corp.");
MODULE_LICENSE("GPL");
