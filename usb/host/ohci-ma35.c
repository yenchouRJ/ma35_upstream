// SPDX-License-Identifier: GPL-2.0
/*
 * Nuvoton MA35 series OHCI Host Controller driver
 *
 * Copyright (C) 2025 Nuvoton Technology Corp.
 *
 * OHCI companion controller for MA35 EHCI. The USB PHY is
 * initialized by the EHCI driver; this driver only manages
 * clocks and the standard OHCI host controller.
 *
 * Derived from ohci-ma35d1.c (vendor BSP) and ohci-platform.c
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "ohci.h"

#define DRIVER_DESC "Nuvoton MA35 OHCI driver"

struct ma35_ohci_priv {
	struct clk *clk;
};

#define hcd_to_ma35_priv(h) \
	((struct ma35_ohci_priv *)hcd_to_ohci(h)->priv)

static struct hc_driver __read_mostly ma35_ohci_hc_driver;

static const struct ohci_driver_overrides ma35_ohci_overrides __initconst = {
	.product_desc = "Nuvoton MA35 OHCI controller",
	.extra_priv_size = sizeof(struct ma35_ohci_priv),
};

static int ma35_ohci_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct ma35_ohci_priv *priv;
	struct resource *res;
	int irq, err;

	if (usb_disabled())
		return -ENODEV;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	err = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (err)
		return err;

	hcd = usb_create_hcd(&ma35_ohci_hc_driver, &pdev->dev,
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

	hcd->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(hcd->regs)) {
		err = PTR_ERR(hcd->regs);
		goto err_clk;
	}
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

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

static void ma35_ohci_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct ma35_ohci_priv *priv = hcd_to_ma35_priv(hcd);

	usb_remove_hcd(hcd);
	clk_disable_unprepare(priv->clk);
	usb_put_hcd(hcd);
}

static int __maybe_unused ma35_ohci_suspend(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ma35_ohci_priv *priv = hcd_to_ma35_priv(hcd);
	bool do_wakeup = device_may_wakeup(dev);
	int ret;

	ret = ohci_suspend(hcd, do_wakeup);
	if (ret)
		return ret;

	clk_disable_unprepare(priv->clk);
	return 0;
}

static int __maybe_unused ma35_ohci_resume(struct device *dev)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct ma35_ohci_priv *priv = hcd_to_ma35_priv(hcd);
	int err;

	err = clk_prepare_enable(priv->clk);
	if (err)
		return err;

	ohci_resume(hcd, false);
	return 0;
}

static SIMPLE_DEV_PM_OPS(ma35_ohci_pm_ops, ma35_ohci_suspend,
			  ma35_ohci_resume);

static const struct of_device_id ma35_ohci_of_match[] = {
	{ .compatible = "nuvoton,ma35d1-ohci" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ma35_ohci_of_match);

static struct platform_driver ma35_ohci_driver = {
	.probe		= ma35_ohci_probe,
	.remove		= ma35_ohci_remove,
	.shutdown	= usb_hcd_platform_shutdown,
	.driver		= {
		.name	= "ma35-ohci",
		.pm	= pm_ptr(&ma35_ohci_pm_ops),
		.of_match_table = ma35_ohci_of_match,
	},
};

static int __init ma35_ohci_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	ohci_init_driver(&ma35_ohci_hc_driver, &ma35_ohci_overrides);
	return platform_driver_register(&ma35_ohci_driver);
}
module_init(ma35_ohci_init);

static void __exit ma35_ohci_cleanup(void)
{
	platform_driver_unregister(&ma35_ohci_driver);
}
module_exit(ma35_ohci_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Nuvoton Technology Corp.");
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: ehci_ma35");
