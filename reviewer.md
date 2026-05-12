## driver:

> The Nuvoton MA35D1 SoC integrates a Verisilicon DCUltra Lite display
> controller, which is a previous generation of the DC8000 series.
> While
> the general register layout is similar to the DC8000, there are
> several
> key differences that require per-variant handling in the driver.
>
> Add a vs_dc_info platform data structure (in vs_hwdb.h) to describe
> per-IP-variant capabilities, and use it throughout the driver to
> select
> the correct code paths at runtime.
>
> Key differences between DC8000 and DCUltra Lite handled:

What the driver supports now is DC8200, DC8000 have the following point
1~4 the same with DCUltraLite (different to DC8200).

1: Understood. I'll rename all `vs_dc8000_*` identifiers to
`vs_dc8200_*` in v2. The commit message will also be corrected to say
that points 1~4 are differences from DC8200, not DC8000.

>
> 1. No chip identity registers (0x0020-0x0030): DCUltra Lite uses
> static
>    platform data instead of reading model/revision/customer_id from
> HW.

My test shows that revision and customer_id is correctly present, only
model is 0 -- I think this can be also considered as a valid model
value because the IP name has also no model number.

The revision number is 0x5560 and customer id is 0x305 .


2: Thank you for testing. I'll drop the `has_chip_id` flag
entirely. In v2, `vs_fill_chip_identity()` will be called for all
variants. A new entry will be added to `vs_chip_identities[]` in
vs_hwdb.c with model=0x0, revision=0x5560, customer_id=0x305,
display_count=1, and `vs_formats_no_yuv444`.
e.g. verisilicon-dc 40260000.display: Found DC0 rev 5560 customer 305

>
> 2. No CONFIG_EX commit mechanism: DC8000 uses registers at 0x1CC0
>    (FB_CONFIG_EX), 0x24D8 (FB_TOP_LEFT), 0x24E0 (FB_BOTTOM_RIGHT),
>    0x2510 (FB_BLEND_CONFIG), 0x2518 (PANEL_CONFIG_EX). DCUltra Lite
>    omits all of these and instead uses enable/reset bits in FB_CONFIG
>    (bit 0 = enable, bit 4 = reset) for direct framebuffer updates.
>
> 3. No PANEL_START register (0x1CCC): DCUltra Lite panel output starts
>    when PANEL_CONFIG.RUNNING is set; no separate multi-display sync
>    start register is needed.
>
> 4. Different IRQ registers: DCUltra Lite uses 0x147C (IRQ_STA) /
>    0x1480 (IRQ_EN); DC8000 uses 0x0010 (IRQ_ACK) / 0x0014 (IRQ_EN).
>
> 5. Different clock/reset topology: DCUltra Lite requires only "core"
>    (bus gate) and "pix0" (pixel divider) clocks with no reset lines
>    managed by the driver. DC8000 needs core/axi/ahb clocks and three
>    resets.

It's possible that your SoC integration combines core clock gate with
bus clock gate instead of bus clock gate not existing.

3: Agreed. In v2 I'll remove the family-gated clock handling and
instead use `devm_clk_get_optional_enabled()` for the axi and ahb
clocks, so they are simply skipped if not present in DT. Resets are
already optional. This keeps the probe path uniform and handles any
SoC-specific clock combinations naturally.

>
> 6. Single output only: DCUltra Lite has one display output; per-
> output
>    index logic is still in place but display_count is fixed at 1.
>
> 7. Reduced register space: max_register is 0x2000 vs DC8000's 0x2544.
>
> Add the "nuvoton,ma35d1-dcu" compatible string to the OF match table,
> extend Kconfig to allow building on ARCH_MA35 platforms, and expose
> vs_formats_no_yuv444 as the default format table for DCUltra Lite
> (YUV444 blending is a DC8000-only feature).
>
> All changes have been tested on Nuvoton MA35D1 hardware and are
> functioning correctly.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>
> ---
>  drivers/gpu/drm/verisilicon/Kconfig           |   2 +-
>  drivers/gpu/drm/verisilicon/vs_bridge.c       |  28 ++--
>  drivers/gpu/drm/verisilicon/vs_crtc.c         |  13 +-
>  drivers/gpu/drm/verisilicon/vs_dc.c           | 129 ++++++++++++----
> --
>  drivers/gpu/drm/verisilicon/vs_dc.h           |   1 +
>  drivers/gpu/drm/verisilicon/vs_drm.c          |  16 ++-
>  drivers/gpu/drm/verisilicon/vs_hwdb.c         |   2 +-
>  drivers/gpu/drm/verisilicon/vs_hwdb.h         |  25 ++++
>  .../gpu/drm/verisilicon/vs_primary_plane.c    |  43 +++---
>  .../drm/verisilicon/vs_primary_plane_regs.h   |   2 +
>  10 files changed, 187 insertions(+), 74 deletions(-)
>
> diff --git a/drivers/gpu/drm/verisilicon/Kconfig
> b/drivers/gpu/drm/verisilicon/Kconfig
> index 7cce86ec8603..295d246eb4b4 100644
> --- a/drivers/gpu/drm/verisilicon/Kconfig
> +++ b/drivers/gpu/drm/verisilicon/Kconfig
> @@ -2,7 +2,7 @@
>  config DRM_VERISILICON_DC
>  	tristate "DRM Support for Verisilicon DC-series display
> controllers"
>  	depends on DRM && COMMON_CLK
> -	depends on RISCV || COMPILE_TEST
> +	depends on RISCV || ARCH_MA35 || COMPILE_TEST
>  	select DRM_BRIDGE_CONNECTOR
>  	select DRM_CLIENT_SELECTION
>  	select DRM_DISPLAY_HELPER
> diff --git a/drivers/gpu/drm/verisilicon/vs_bridge.c
> b/drivers/gpu/drm/verisilicon/vs_bridge.c
> index 7a93049368db..225af322de32 100644
> --- a/drivers/gpu/drm/verisilicon/vs_bridge.c
> +++ b/drivers/gpu/drm/verisilicon/vs_bridge.c
> @@ -164,13 +164,16 @@ static void vs_bridge_enable_common(struct
> vs_crtc *crtc,
>  			VSDC_DISP_PANEL_CONFIG_CLK_EN);
>  	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_CONFIG(output),
>  			VSDC_DISP_PANEL_CONFIG_RUNNING);
> -	regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_START,
> -			  VSDC_DISP_PANEL_START_MULTI_DISP_SYNC);
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_START,
> -			VSDC_DISP_PANEL_START_RUNNING(output));
>  
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_CONFIG_EX(crtc-
>> id),
> -			VSDC_DISP_PANEL_CONFIG_EX_COMMIT);
> +	if (dc->info->has_config_ex) {
> +		regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_START,
> +				 
> VSDC_DISP_PANEL_START_MULTI_DISP_SYNC);
> +		regmap_set_bits(dc->regs, VSDC_DISP_PANEL_START,
> +				VSDC_DISP_PANEL_START_RUNNING(output
> ));
> +
> +		regmap_set_bits(dc->regs,
> VSDC_DISP_PANEL_CONFIG_EX(crtc->id),
> +				VSDC_DISP_PANEL_CONFIG_EX_COMMIT);

Should the commit operation happen on DC8000/DCUltraLite too? (By
writing to DcregFrameBufferConfig0.VALID).

Many registers written has "Note: This field is double buffered" in the
DCUltraLite documentation.

I suggest create a static function for commit -- write to the
corresponding commit bit on DC8200, and write to
DcregFrameBufferConfig0.VALID on DC8000/DCUltraLite.

4: Thank you for the correction. Per the MA35D1 TRM, VALID (BIT(3)) is
software-writable: writing 1=PENDING marks the current register set as
ready to be copied into the working set at the next VBLANK, after which
hardware clears it back to 0=WORKING. I'll add
`#define VSDC_FB_CONFIG_VALID BIT(3)` to vs_primary_plane_regs.h and
write it in `vs_primary_plane_commit()` for non-config_ex variants.
The function then provides a unified commit abstraction:
`VSDC_FB_CONFIG_EX_COMMIT` for config_ex (DC8200) variants, and
`VSDC_FB_CONFIG_VALID` for non-config_ex (DCUltra Lite) variants.

> +	}
>  }
>  
>  static void vs_bridge_atomic_enable_dpi(struct drm_bridge *bridge,
> @@ -228,14 +231,17 @@ static void vs_bridge_atomic_disable(struct
> drm_bridge *bridge,
>  	struct vs_dc *dc = crtc->dc;
>  	unsigned int output = crtc->id;
>  
> -	regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_START,
> -			  VSDC_DISP_PANEL_START_MULTI_DISP_SYNC |
> -			  VSDC_DISP_PANEL_START_RUNNING(output));
>  	regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_CONFIG(output),
>  			  VSDC_DISP_PANEL_CONFIG_RUNNING);

This bit seems to be not present in DCUltraLite, instead should
DcregFrameBufferConfig0.RESET be clear, which will stall the DPI
timing?

5: I'll move `VSDC_DISP_PANEL_CONFIG_RUNNING` inside the
`has_config_ex` block in both the enable and disable paths. For
non-config_ex variants (DCUltra Lite), the disable path will instead
clear `VSDC_FB_CONFIG_RESET` to stall DPI output.
`VSDC_DISP_PANEL_CONFIG_RUNNING` will only be set/cleared for
DC8200 (has_config_ex=true).

>  
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_CONFIG_EX(crtc-
>> id),
> -			VSDC_DISP_PANEL_CONFIG_EX_COMMIT);
> +	if (dc->info->has_config_ex) {
> +		regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_START,
> +				 
> VSDC_DISP_PANEL_START_MULTI_DISP_SYNC |
> +				 
> VSDC_DISP_PANEL_START_RUNNING(output));
> +
> +		regmap_set_bits(dc->regs,
> VSDC_DISP_PANEL_CONFIG_EX(crtc->id),
> +				VSDC_DISP_PANEL_CONFIG_EX_COMMIT);
> +	}

Ditto.

6: Covered by the same fix above — the else branch in
vs_bridge_atomic_disable() will handle the non-config_ex (DCUltra
Lite) path by clearing RESET.

>  }
>  
>  static const struct drm_bridge_funcs vs_dpi_bridge_funcs = {
> diff --git a/drivers/gpu/drm/verisilicon/vs_crtc.c
> b/drivers/gpu/drm/verisilicon/vs_crtc.c
> index 9080344398ca..2f3e6d41c657 100644
> --- a/drivers/gpu/drm/verisilicon/vs_crtc.c
> +++ b/drivers/gpu/drm/verisilicon/vs_crtc.c
> @@ -18,6 +18,7 @@
>  #include "vs_dc.h"
>  #include "vs_dc_top_regs.h"
>  #include "vs_drm.h"
> +#include "vs_hwdb.h"
>  #include "vs_plane.h"
>  
>  static void vs_crtc_atomic_disable(struct drm_crtc *crtc,
> @@ -132,7 +133,11 @@ static int vs_crtc_enable_vblank(struct drm_crtc
> *crtc)
>  	struct vs_crtc *vcrtc = drm_crtc_to_vs_crtc(crtc);
>  	struct vs_dc *dc = vcrtc->dc;
>  
> -	regmap_set_bits(dc->regs, VSDC_TOP_IRQ_EN,
> VSDC_TOP_IRQ_VSYNC(vcrtc->id));
> +	if (dc->info->family == VS_DC_FAMILY_DCULTRA_LITE)
> +		regmap_write(dc->regs, VSDC_DISP_IRQ_EN, BIT(0));

Should `BIT(0)` be assigned with a named macro here, like
`VSDC_DISP_IRQ_VSYNC(0)` ?

In addition, even it's known that DC8000/DCUltraLite has only one
output, hardcoding 0 here isn't so good. The header file at [1] (which
is for DC8000) shows that two display interrupts and more other
interrupts are present.

BTW I don't like the idea of setting a "family" (because DC8000 behave
nearly the same with DCUltraLite), maybe use `.irq_reg = VSDC_DISP_IRQ`
(or a bool variable named `uses_top_irq`) is better?

[1]
https://github.com/milkv-megrez/rockos-u-boot/blob/c9221cf2fa77d39c0b241ab4b030c708e7ebe279/drivers/video/eswin/eswin_dc_reg.h#L2771

7: I'll address all three issues in v2:
1. Add `VSDC_DISP_IRQ_VSYNC(n)` macro (BIT(n)) to vs_crtc_regs.h.
2. Use `VSDC_DISP_IRQ_VSYNC(vcrtc->id)` with
   `regmap_set_bits`/`regmap_clear_bits` instead of hardcoding 0 and
   using `regmap_write` (which would clobber other IRQ bits).
3. Add `uses_top_irq` bool to `struct vs_chip_identity` in vs_hwdb.h.
   DC8200 entries will have `uses_top_irq = true`; the DCUltra Lite
   entry (model=0x0, rev=0x5560) will have `uses_top_irq = false`.

> +	else
> +		regmap_set_bits(dc->regs, VSDC_TOP_IRQ_EN,
> +				VSDC_TOP_IRQ_VSYNC(vcrtc->id));
>  
>  	return 0;
>  }
> @@ -142,7 +147,11 @@ static void vs_crtc_disable_vblank(struct
> drm_crtc *crtc)
>  	struct vs_crtc *vcrtc = drm_crtc_to_vs_crtc(crtc);
>  	struct vs_dc *dc = vcrtc->dc;
>  
> -	regmap_clear_bits(dc->regs, VSDC_TOP_IRQ_EN,
> VSDC_TOP_IRQ_VSYNC(vcrtc->id));
> +	if (dc->info->family == VS_DC_FAMILY_DCULTRA_LITE)
> +		regmap_write(dc->regs, VSDC_DISP_IRQ_EN, 0);
> +	else
> +		regmap_clear_bits(dc->regs, VSDC_TOP_IRQ_EN,
> +				  VSDC_TOP_IRQ_VSYNC(vcrtc->id));
>  }
>  
>  static const struct drm_crtc_funcs vs_crtc_funcs = {
> diff --git a/drivers/gpu/drm/verisilicon/vs_dc.c
> b/drivers/gpu/drm/verisilicon/vs_dc.c
> index dad9967bc10b..82a6a26f6d81 100644
> --- a/drivers/gpu/drm/verisilicon/vs_dc.c
> +++ b/drivers/gpu/drm/verisilicon/vs_dc.c
> @@ -9,21 +9,45 @@
>  #include <linux/of_graph.h>
>  
>  #include "vs_crtc.h"
> +#include "vs_crtc_regs.h"
>  #include "vs_dc.h"
>  #include "vs_dc_top_regs.h"
>  #include "vs_drm.h"
>  #include "vs_hwdb.h"
>  
> -static const struct regmap_config vs_dc_regmap_cfg = {
> +static const struct regmap_config vs_dc8000_regmap_cfg = {

As what I said, the currently supported thing is DC8200, not DC8000.

8: I'll keep the name `vs_dc_regmap_cfg` — the original name from
upstream. A separate DCUltra Lite regmap config is not needed; the
hardware simply does not implement registers above 0x2000, and a regmap
window up to 0x2544 is harmless for that variant.

>  	.reg_bits = 32,
>  	.val_bits = 32,
>  	.reg_stride = sizeof(u32),
> -	/* VSDC_OVL_CONFIG_EX(1) */
>  	.max_register = 0x2544,
>  };
>  
> +static const struct regmap_config vs_dcultra_lite_regmap_cfg = {
> +	.reg_bits = 32,
> +	.val_bits = 32,
> +	.reg_stride = sizeof(u32),
> +	.max_register = 0x2000,
> +};
> +
> +static const struct vs_dc_info vs_dc8000_info = {
> +	.family = VS_DC_FAMILY_DC8000,
> +	.has_chip_id = true,
> +	.has_config_ex = true,
> +	.regmap_cfg = &vs_dc8000_regmap_cfg,
> +};
> +
> +static const struct vs_dc_info vs_dcultra_lite_info = {
> +	.family = VS_DC_FAMILY_DCULTRA_LITE,
> +	.display_count = 1,
> +	.has_chip_id = false,
> +	.has_config_ex = false,
> +	.regmap_cfg = &vs_dcultra_lite_regmap_cfg,
> +	.formats = &vs_formats_no_yuv444,
> +};
> +
>  static const struct of_device_id vs_dc_driver_dt_match[] = {
> -	{ .compatible = "verisilicon,dc" },
> +	{ .compatible = "verisilicon,dc", .data = &vs_dc8000_info },

"verisilicon,dc" is expected for DC8000 and 8200, and because of model,
rev and customer_id values are all present for DCUltraLite, the special
data might be dropped?

9: I'll drop `.data` from both `of_device_id` entries entirely.
`has_config_ex` and `uses_top_irq` will be added as fields to
`struct vs_chip_identity` in vs_hwdb.h, and populated for each entry
in `vs_chip_identities[]` in vs_hwdb.c. `vs_fill_chip_identity()` is
called unconditionally for all variants — chip-ID registers are present
on DCUltra Lite too. There is no need for a separate `vs_dc_info`
platform data structure.

> +	{ .compatible = "nuvoton,ma35d1-dcu", .data =
> &vs_dcultra_lite_info },
>  	{},
>  };
>  MODULE_DEVICE_TABLE(of, vs_dc_driver_dt_match);
> @@ -33,6 +57,13 @@ static irqreturn_t vs_dc_irq_handler(int irq, void
> *private)
>  	struct vs_dc *dc = private;
>  	u32 irqs;
>  
> +	if (dc->info->family == VS_DC_FAMILY_DCULTRA_LITE) {
> +		regmap_read(dc->regs, VSDC_DISP_IRQ_STA, &irqs);

Maybe add an item in the hwdb for the IRQ register?

10: I'll add `uses_top_irq` as a bool field to `struct vs_chip_identity`
in vs_hwdb.h instead. The IRQ handler will branch on
`dc->identity.uses_top_irq` to read either `VSDC_TOP_IRQ_ACK` or
`VSDC_DISP_IRQ_STA`. No register address needs to be stored in the
hwdb struct.

> +		if (irqs & BIT(0))
> +			vs_drm_handle_irq(dc,
> VSDC_TOP_IRQ_VSYNC(0));
> +		return IRQ_HANDLED;

Now I think for DC8200, the irq number decoding should be done here
too.

11: I'll unify the IRQ handler: both paths will read their
respective IRQ status register and pass the raw `irqs` value to
`vs_drm_handle_irq()`. Since `VSDC_TOP_IRQ_VSYNC(n)` and
`VSDC_DISP_IRQ_VSYNC(n)` are both `BIT(n)`, the bit positions are
identical and `vs_drm_handle_irq()` handles both without change.

> +	}
> +
>  	regmap_read(dc->regs, VSDC_TOP_IRQ_ACK, &irqs);
>  
>  	vs_drm_handle_irq(dc, irqs);
> @@ -43,6 +74,7 @@ static irqreturn_t vs_dc_irq_handler(int irq, void
> *private)
>  static int vs_dc_probe(struct platform_device *pdev)
>  {
>  	struct device *dev = &pdev->dev;
> +	const struct vs_dc_info *info;
>  	struct vs_dc *dc;
>  	void __iomem *regs;
>  	unsigned int port_count, i;
> @@ -55,6 +87,10 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  		return -ENODEV;
>  	}
>  
> +	info = of_device_get_match_data(dev);
> +	if (!info)
> +		return -ENODEV;
> +
>  	port_count = of_graph_get_port_count(dev->of_node);
>  	if (!port_count) {
>  		dev_err(dev, "can't find DC downstream ports\n");
> @@ -75,15 +111,31 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  	if (!dc)
>  		return -ENOMEM;
>  
> -	dc->rsts[0].id = "core";
> -	dc->rsts[1].id = "axi";
> -	dc->rsts[2].id = "ahb";
> +	dc->info = info;
>  
> -	ret = devm_reset_control_bulk_get_optional_shared(dev,
> VSDC_RESET_COUNT,
> -							  dc->rsts);
> -	if (ret) {
> -		dev_err(dev, "can't get reset lines\n");
> -		return ret;
> +	if (info->family == VS_DC_FAMILY_DC8000) {
> +		dc->rsts[0].id = "core";
> +		dc->rsts[1].id = "axi";
> +		dc->rsts[2].id = "ahb";
> +
> +		ret =
> devm_reset_control_bulk_get_optional_shared(dev,
> +				VSDC_RESET_COUNT, dc->rsts);
> +		if (ret) {
> +			dev_err(dev, "can't get reset lines\n");
> +			return ret;
> +		}
> +
> +		dc->axi_clk = devm_clk_get_enabled(dev, "axi");
> +		if (IS_ERR(dc->axi_clk)) {
> +			dev_err(dev, "can't get axi clock\n");
> +			return PTR_ERR(dc->axi_clk);
> +		}
> +
> +		dc->ahb_clk = devm_clk_get_enabled(dev, "ahb");
> +		if (IS_ERR(dc->ahb_clk)) {
> +			dev_err(dev, "can't get ahb clock\n");
> +			return PTR_ERR(dc->ahb_clk);
> +		}

Please make these clocks optional, instead of wrap them in a "family
detection". The resets are already optional, see above.

12: I'll remove the family-gated block entirely. Resets will be
acquired unconditionally (they are optional, so absent ones are
skipped). axi and ahb clocks will use `devm_clk_get_optional_enabled()`
so they are silently skipped when not present in DT. The probe path
will be uniform for all variants.

>  	}
>  
>  	dc->core_clk = devm_clk_get_enabled(dev, "core");
> @@ -92,28 +144,18 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  		return PTR_ERR(dc->core_clk);
>  	}
>  
> -	dc->axi_clk = devm_clk_get_enabled(dev, "axi");
> -	if (IS_ERR(dc->axi_clk)) {
> -		dev_err(dev, "can't get axi clock\n");
> -		return PTR_ERR(dc->axi_clk);
> -	}
> -
> -	dc->ahb_clk = devm_clk_get_enabled(dev, "ahb");
> -	if (IS_ERR(dc->ahb_clk)) {
> -		dev_err(dev, "can't get ahb clock\n");
> -		return PTR_ERR(dc->ahb_clk);
> -	}
> -
>  	irq = platform_get_irq(pdev, 0);
>  	if (irq < 0) {
>  		dev_err(dev, "can't get irq\n");
>  		return irq;
>  	}
>  
> -	ret = reset_control_bulk_deassert(VSDC_RESET_COUNT, dc-
>> rsts);
> -	if (ret) {
> -		dev_err(dev, "can't deassert reset lines\n");
> -		return ret;
> +	if (info->family == VS_DC_FAMILY_DC8000) {
> +		ret = reset_control_bulk_deassert(VSDC_RESET_COUNT,
> dc->rsts);
> +		if (ret) {
> +			dev_err(dev, "can't deassert reset
> lines\n");
> +			return ret;
> +		}
>  	}
>  
>  	regs = devm_platform_ioremap_resource(pdev, 0);
> @@ -123,23 +165,30 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  		goto err_rst_assert;
>  	}
>  
> -	dc->regs = devm_regmap_init_mmio(dev, regs,
> &vs_dc_regmap_cfg);
> +	dc->regs = devm_regmap_init_mmio(dev, regs, info-
>> regmap_cfg);
>  	if (IS_ERR(dc->regs)) {
>  		ret = PTR_ERR(dc->regs);
>  		goto err_rst_assert;
>  	}
>  
> -	ret = vs_fill_chip_identity(dc->regs, &dc->identity);
> -	if (ret)
> -		goto err_rst_assert;
> +	if (info->has_chip_id) {
> +		ret = vs_fill_chip_identity(dc->regs, &dc-
>> identity);
> +		if (ret)
> +			goto err_rst_assert;
>  
> -	dev_info(dev, "Found DC%x rev %x customer %x\n", dc-
>> identity.model,
> -		 dc->identity.revision, dc->identity.customer_id);
> +		dev_info(dev, "Found DC%x rev %x customer %x\n",
> +			 dc->identity.model, dc->identity.revision,
> +			 dc->identity.customer_id);
>  
> -	if (port_count > dc->identity.display_count) {
> -		dev_err(dev, "too many downstream ports than HW
> capability\n");
> -		ret = -EINVAL;
> -		goto err_rst_assert;
> +		if (port_count > dc->identity.display_count) {
> +			dev_err(dev, "too many downstream ports than
> HW capability\n");
> +			ret = -EINVAL;
> +			goto err_rst_assert;
> +		}
> +	} else {
> +		/* Fill identity from platform data */
> +		dc->identity.display_count = info->display_count;
> +		dc->identity.formats = info->formats;
>  	}
>  
>  	for (i = 0; i < dc->identity.display_count; i++) {
> @@ -168,7 +217,8 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  	return 0;
>  
>  err_rst_assert:
> -	reset_control_bulk_assert(VSDC_RESET_COUNT, dc->rsts);
> +	if (info->family == VS_DC_FAMILY_DC8000)
> +		reset_control_bulk_assert(VSDC_RESET_COUNT, dc-
>> rsts);
>  	return ret;
>  }
>  
> @@ -180,7 +230,8 @@ static void vs_dc_remove(struct platform_device
> *pdev)
>  
>  	dev_set_drvdata(&pdev->dev, NULL);
>  
> -	reset_control_bulk_assert(VSDC_RESET_COUNT, dc->rsts);
> +	if (dc->info->family == VS_DC_FAMILY_DC8000)
> +		reset_control_bulk_assert(VSDC_RESET_COUNT, dc-
>> rsts);
>  }
>  
>  static void vs_dc_shutdown(struct platform_device *pdev)
> diff --git a/drivers/gpu/drm/verisilicon/vs_dc.h
> b/drivers/gpu/drm/verisilicon/vs_dc.h
> index ed1016f18758..f0613519af37 100644
> --- a/drivers/gpu/drm/verisilicon/vs_dc.h
> +++ b/drivers/gpu/drm/verisilicon/vs_dc.h
> @@ -31,6 +31,7 @@ struct vs_dc {
>  	struct clk *pix_clk[VSDC_MAX_OUTPUTS];
>  	struct reset_control_bulk_data rsts[VSDC_RESET_COUNT];
>  
> +	const struct vs_dc_info *info;
>  	struct vs_drm_dev *drm_dev;
>  	struct vs_chip_identity identity;
>  };
> diff --git a/drivers/gpu/drm/verisilicon/vs_drm.c
> b/drivers/gpu/drm/verisilicon/vs_drm.c
> index fd259d53f49f..ff0fc6673006 100644
> --- a/drivers/gpu/drm/verisilicon/vs_drm.c
> +++ b/drivers/gpu/drm/verisilicon/vs_drm.c
> @@ -27,6 +27,7 @@
>  #include "vs_dc.h"
>  #include "vs_dc_top_regs.h"
>  #include "vs_drm.h"
> +#include "vs_hwdb.h"
>  
>  #define DRIVER_NAME	"verisilicon"
>  #define DRIVER_DESC	"Verisilicon DC-series display controller
> driver"
>  @@ -72,12 +73,19 @@ static struct drm_mode_config_helper_funcs
> vs_mode_config_helper_funcs = {
>  	.atomic_commit_tail = drm_atomic_helper_commit_tail,
>  };
>  
> -static void vs_mode_config_init(struct drm_device *drm)
> +static void vs_mode_config_init(struct drm_device *drm, struct vs_dc
> *dc)
>  {
>  	drm->mode_config.min_width = 0;
>  	drm->mode_config.min_height = 0;
> -	drm->mode_config.max_width = 8192;
> -	drm->mode_config.max_height = 8192;
> +
> +	if (dc->info->family == VS_DC_FAMILY_DCULTRA_LITE) {
> +		drm->mode_config.max_width = 1920;
> +		drm->mode_config.max_height = 1080;

Surely only max width 1920 and max height 1080? Can a display of
1080x1920 (e.g. a portrait DSI panel behind a RGB2DSI bridge) be
supported? Can a 2170x60 display (MacBook Touch Bar panel, also a DSI
panel) be supported? Both displays here will have no higher pixel clock
than 1080p in the same refresh rate, although the width / height is
higher than your restriction.

In addition, these parameters decide how big a FB can be created -- the
FB might be scaned out by multiple devices (e.g. a USB DisplayLink
device scanning out the remaining part). The stride register is said to
have 17-bit width in the MA35D1 TRM, so the possible FB width could be
quite high -- assume the 17th bit is only for the value with one 1 and
all remaining 0, we get 65536 bytes stride; with 4-byte-per-pixel
divided this gets 16384 pixels -- the htotal/hdisplay/vtotal/vdisplay
fields in the manual has 15-bit field width, which can reach 32767.

I strongly suggest leave the original values here.

13: You are right. I'll revert to 8192x8192 for all variants. The
variant-specific max_width/max_height logic will be removed entirely
and vs_mode_config_init() will take no dc argument.

> +	} else {
> +		drm->mode_config.max_width = 8192;
> +		drm->mode_config.max_height = 8192;
> +	}
> +
>  	drm->mode_config.funcs = &vs_mode_config_funcs;
>  	drm->mode_config.helper_private =
> &vs_mode_config_helper_funcs;
>  }
> @@ -125,7 +133,7 @@ int vs_drm_initialize(struct vs_dc *dc, struct
> platform_device *pdev)
>  	if (ret)
>  		return ret;
>  
> -	vs_mode_config_init(drm);
> +	vs_mode_config_init(drm, dc);
>  
>  	/* Enable connectors polling */
>  	drm_kms_helper_poll_init(drm);
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> index 09336af0900a..39402d75d841 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> @@ -78,7 +78,7 @@ static const u32 vs_formats_array_with_yuv444[] = {
>  	/* TODO: non-RGB formats */
>  };
>  
> -static const struct vs_formats vs_formats_no_yuv444 = {
> +const struct vs_formats vs_formats_no_yuv444 = {
>  	.array = vs_formats_array_no_yuv444,
>  	.num = ARRAY_SIZE(vs_formats_array_no_yuv444)
>  };
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> index 92192e4fa086..655cf93ca3aa 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> @@ -14,6 +14,29 @@ struct vs_formats {
>  	unsigned int num;
>  };
>  
> +enum vs_dc_family {
> +	VS_DC_FAMILY_DC8000,
> +	VS_DC_FAMILY_DCULTRA_LITE,
> +};
> +
> +/**
> + * struct vs_dc_info - per-SoC DC platform data
> + * @family:		DC IP family (DC8000, DCUltra Lite, etc.)
> + * @display_count:	number of display outputs (0 = auto-detect
> from DT/HW)
> + * @has_chip_id:	whether chip identity registers exist
> + * @has_config_ex:	whether CONFIG_EX commit mechanism exists
> + * @regmap_cfg:		regmap configuration for this
> variant
> + * @formats:		supported pixel formats (NULL = auto-detect
> from chip ID)
> + */
> +struct vs_dc_info {
> +	enum vs_dc_family family;
> +	u32 display_count;
> +	bool has_chip_id;
> +	bool has_config_ex;
> +	const struct regmap_config *regmap_cfg;
> +	const struct vs_formats *formats;
> +};
> +
>  struct vs_chip_identity {
>  	u32 model;
>  	u32 revision;
> @@ -23,6 +46,8 @@ struct vs_chip_identity {
>  	const struct vs_formats *formats;
>  };
>  
> +extern const struct vs_formats vs_formats_no_yuv444;
> +
>  int vs_fill_chip_identity(struct regmap *regs,
>  			  struct vs_chip_identity *ident);
>  
> diff --git a/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> b/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> index 1f2be41ae496..197d5d683e22 100644
> --- a/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> +++ b/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> @@ -55,8 +55,9 @@ static int vs_primary_plane_atomic_check(struct
> drm_plane *plane,
>  
>  static void vs_primary_plane_commit(struct vs_dc *dc, unsigned int
> output)
>  {
> -	regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			VSDC_FB_CONFIG_EX_COMMIT);
> +	if (dc->info->has_config_ex)
> +		regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> +				VSDC_FB_CONFIG_EX_COMMIT);

Should VALID bit be written here instead of doing nothing on
DC8000/DCUltraLite ?

14: Same as reply 4. I'll add `VSDC_FB_CONFIG_VALID` and set it in
`vs_primary_plane_commit()` for non-config_ex variants. The function
provides a unified commit abstraction for both paths.

>  }
>  
>  static void vs_primary_plane_atomic_enable(struct drm_plane *plane,
> @@ -69,11 +70,13 @@ static void vs_primary_plane_atomic_enable(struct
> drm_plane *plane,
>  	unsigned int output = vcrtc->id;
>  	struct vs_dc *dc = vcrtc->dc;
>  
> -	regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			VSDC_FB_CONFIG_EX_FB_EN);
> -	regmap_update_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			   VSDC_FB_CONFIG_EX_DISPLAY_ID_MASK,
> -			   VSDC_FB_CONFIG_EX_DISPLAY_ID(output));
> +	if (dc->info->has_config_ex) {
> +		regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> +				VSDC_FB_CONFIG_EX_FB_EN);
> +		regmap_update_bits(dc->regs,
> VSDC_FB_CONFIG_EX(output),
> +				  
> VSDC_FB_CONFIG_EX_DISPLAY_ID_MASK,
> +				  
> VSDC_FB_CONFIG_EX_DISPLAY_ID(output));
> +	}
>  
>  	vs_primary_plane_commit(dc, output);
>  }
> @@ -88,8 +91,9 @@ static void vs_primary_plane_atomic_disable(struct
> drm_plane *plane,
>  	unsigned int output = vcrtc->id;
>  	struct vs_dc *dc = vcrtc->dc;
>  
> -	regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			VSDC_FB_CONFIG_EX_FB_EN);
> +	if (dc->info->has_config_ex)
> +		regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> +				VSDC_FB_CONFIG_EX_FB_EN);
>  
>  	vs_primary_plane_commit(dc, output);
>  }
> @@ -126,6 +130,11 @@ static void
> vs_primary_plane_atomic_update(struct drm_plane *plane,
>  			   VSDC_FB_CONFIG_UV_SWIZZLE_EN,
>  			   vs_state->format.uv_swizzle);
>  
> +	/* DCUltra Lite requires explicit enable/reset bits in
> FB_CONFIG */
> +	if (!dc->info->has_config_ex)
> +		regmap_set_bits(dc->regs, VSDC_FB_CONFIG(output),
> +				VSDC_FB_CONFIG_ENABLE |
> VSDC_FB_CONFIG_RESET);

Should VSDC_FB_CONFIG_RESET be only set when it's ready to output the
signal (at least all timing is programmed)? I think it should be
programmed in crtc/bridge instead of primary plane, although it's in
the DcregFrameBufferConfig0 register (obviously this sounds a little
weird, this might be why they changed this in DC8200).

When ENABLE (OUTPUT in the document) is cleared, all pixels will be
blacked out; so I think it's better to set ENABLE in CRTC, and then set
RESET in bridge (doing the work of encoder in this driver) -- it seems
that for DC8000/DCUltraLite the primary plane is not possible to be
disabled.

15: I'll move these as suggested:
- `VSDC_FB_CONFIG_ENABLE` will be set in `vs_crtc_atomic_enable()` for
  non-config_ex variants (after the pixel clock is enabled).
- `VSDC_FB_CONFIG_RESET` will be set in `vs_bridge_enable_common()` for
  non-config_ex variants, after all timing registers are programmed.
- `VSDC_FB_CONFIG_RESET` will be cleared in `vs_bridge_atomic_disable()`
  for non-config_ex variants to stall DPI output.
- The ENABLE/RESET writes will be removed from
  `vs_primary_plane_atomic_update()`.
- `vs_primary_plane_commit()` will perform no action for non-config_ex
  variants; the function only triggers `VSDC_FB_CONFIG_EX_COMMIT` for
  config_ex variants.

>  	dma_addr = vs_fb_get_dma_addr(fb, &state->src);
>  
>  	regmap_write(dc->regs, VSDC_FB_ADDRESS(output),
> @@ -133,16 +142,18 @@ static void
> vs_primary_plane_atomic_update(struct drm_plane *plane,
>  	regmap_write(dc->regs, VSDC_FB_STRIDE(output),
>  		     fb->pitches[0]);
>  
> -	regmap_write(dc->regs, VSDC_FB_TOP_LEFT(output),
> -		     VSDC_MAKE_PLANE_POS(state->crtc_x, state-
>> crtc_y));
> -	regmap_write(dc->regs, VSDC_FB_BOTTOM_RIGHT(output),
> -		     VSDC_MAKE_PLANE_POS(state->crtc_x + state-
>> crtc_w,
> -					 state->crtc_y + state-
>> crtc_h));
>  	regmap_write(dc->regs, VSDC_FB_SIZE(output),
>  		     VSDC_MAKE_PLANE_SIZE(state->crtc_w, state-
>> crtc_h));
>  
> -	regmap_write(dc->regs, VSDC_FB_BLEND_CONFIG(output),
> -		     VSDC_FB_BLEND_CONFIG_BLEND_DISABLE);
> +	if (dc->info->has_config_ex) {
> +		regmap_write(dc->regs, VSDC_FB_TOP_LEFT(output),
> +			     VSDC_MAKE_PLANE_POS(state->crtc_x,
> state->crtc_y));
> +		regmap_write(dc->regs, VSDC_FB_BOTTOM_RIGHT(output),
> +			     VSDC_MAKE_PLANE_POS(state->crtc_x +
> state->crtc_w,
> +						 state->crtc_y +
> state->crtc_h));
> +		regmap_write(dc->regs, VSDC_FB_BLEND_CONFIG(output),
> +			     VSDC_FB_BLEND_CONFIG_BLEND_DISABLE);
> +	}
>  
>  	vs_primary_plane_commit(dc, output);
>  }
> diff --git a/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> b/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> index cbb125c46b39..288064760b48 100644
> --- a/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> +++ b/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> @@ -16,6 +16,8 @@
>  #define VSDC_FB_STRIDE(n)			(0x1408 + 0x4 * (n))
>  
>  #define VSDC_FB_CONFIG(n)			(0x1518 + 0x4 * (n))
> +#define VSDC_FB_CONFIG_ENABLE			BIT(0)

As I mentioned that the VALID bit is quite important, please add it
here (you can call it "COMMIT" too if you like).

#define VSDC_FB_CONFIG_VALID BIT(3)

16: I'll add `#define VSDC_FB_CONFIG_VALID BIT(3)` to
vs_primary_plane_regs.h as suggested. It is used in
`vs_primary_plane_commit()` for non-config_ex variants to trigger the
double-buffer copy at the next VBLANK.

> +#define VSDC_FB_CONFIG_RESET			BIT(4)
>  #define VSDC_FB_CONFIG_CLEAR_EN			BIT(8)
>  #define VSDC_FB_CONFIG_ROT_MASK			GENMASK(13,
> 11)
>  #define VSDC_FB_CONFIG_ROT(v)			((v) << 11)



## yaml

> Extend the verisilicon,dc base schema to accommodate the Nuvoton
> MA35D1
> DCUltra Lite (a previous generation of the DC8000 series) which has a
> different clock topology, no reset control, and a single output.
>
> - Replace the fixed clock/reset item lists with minItems/maxItems
> ranges
>   so sub-schemas can enforce variant-specific constraints
> - Add a 'port' property (single-port alias) alongside the existing
> 'ports'
>   for single-output variants
> - Remove the mandatory 'ports' requirement from the base schema; sub-
> schemas
>   shall enforce their own port topology
> - Add a 'select' stanza so the validator matches any node whose
> compatible
>   contains a known Verisilicon DC string, including SoC-specific glue
> - Relax additionalProperties to allow unevaluatedProperties
> enforcement in
>   sub-schemas
> - Fix a minor whitespace issue in the port@0 description
>
> Add nuvoton,ma35d1-dcu.yaml as a sub-schema for the Nuvoton MA35D1
> DCUltra
> Lite display controller:
>
> The Nuvoton MA35D1 integrates the Verisilicon DCUltra Lite display
> controller. It is a single-output display controller with a 32-bit
> RGB (DPI) interface. Unlike the DC8000, it does not have discoverable
> chip identity registers, does not support the CONFIG_EX commit path,
> and uses dedicated IRQ status/enable registers at offsets
> 0x147C/0x1480.
> The clock topology uses two clocks (bus gate and pixel divider) and
> does not require explicit reset control from the driver.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>
> ---
>  .../bindings/display/nuvoton,ma35d1-dcu.yaml  | 94
> +++++++++++++++++++
>  .../bindings/display/verisilicon,dc.yaml      | 64 +++++++------
>  2 files changed, 131 insertions(+), 27 deletions(-)
>  create mode 100644
> Documentation/devicetree/bindings/display/nuvoton,ma35d1-dcu.yaml
>
> diff --git
> a/Documentation/devicetree/bindings/display/nuvoton,ma35d1-dcu.yaml
> b/Documentation/devicetree/bindings/display/nuvoton,ma35d1-dcu.yaml
> new file mode 100644
> index 000000000000..9279004ae27c
> --- /dev/null
> +++ b/Documentation/devicetree/bindings/display/nuvoton,ma35d1-
> dcu.yaml
> @@ -0,0 +1,94 @@
> +# SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
> +%YAML 1.2
> +---
> +$id: http://devicetree.org/schemas/display/nuvoton,ma35d1-dcu.yaml#
> +$schema: http://devicetree.org/meta-schemas/core.yaml#
> +
> +title: Nuvoton MA35D1 DCUltra Lite display controller
> +
> +maintainers:
> +  - Joey Lu <yclu4@nuvoton.com>
> +
> +description:
> +  The Nuvoton MA35D1 integrates the Verisilicon DCUltra Lite display
> +  controller. It is a single-output display controller with a 32-bit
> +  RGB (DPI) interface.

You'd better write this in verisilicon,dc.yaml with if clauses. See
Documentation/devicetree/bindings/mmc/snps,dwcmshc-sdhci.yaml for an
example for a generic IP with different integrations, and how it
constraints different SoC's integration. 

Reply: Understood. I'll drop both the separate nuvoton,ma35d1-dcu.yaml
file and the `nuvoton,ma35d1-dcu` compatible string entirely. Since the
chip identity registers are present on the DCUltra Lite (model=0x0,
rev=0x5560, customer=0x305), the MA35D1 DT node will use
`compatible = "verisilicon,dc"` directly. The YAML will use a single
`if/then/else` block in `allOf:` keyed on whether `thead,th1520-dc8200`
is present in the compatible list: the `then` branch enforces DC8200
constraints (5 clocks, 3 resets, `ports` required), and the `else`
branch enforces DCUltra Lite / generic constraints (2 clocks, 1 reset,
`port` required, `ports` forbidden). The Nuvoton MA35D1 example will use
`compatible = "verisilicon,dc"` and fall into the `else` branch.

> +
> +select:
> +  properties:
> +    compatible:
> +      contains:
> +        enum:
> +          - nuvoton,ma35d1-dcu
> +  required:
> +    - compatible
> +
> +allOf:
> +  - $ref: http://devicetree.org/schemas/display/verisilicon,dc.yaml#
> +
> +properties:
> +  compatible:
> +    const: nuvoton,ma35d1-dcu
> +
> +  reg:
> +    maxItems: 1
> +    description:
> +      Register range of the DCUltra Lite controller. The address space
> +      is 0x2000 bytes.

Is it really 0x2000 bytes? The next peripherals in the address space,
the GC520L 2D GPU, is 0x20000 bytes away from the start of DCU
registers space.

Reply: Good point. The 0x2000 figure is the driver's `max_register`,
not the full hardware block size. I'll remove the description text 
claiming "0x2000 bytes" and update the example to use `0x20000` to
match the actual hardware address space.

> +
> +  interrupts:
> +    maxItems: 1
> +
> +  clocks:
> +    items:
> +      - description: Bus clock that gates register access (DCU_GATE)
> +      - description: Pixel clock divider for display timing
> (DCUP_DIV)
> +
> +  clock-names:
> +    items:
> +      - const: core
> +      - const: pix0
> +
> +  resets:
> +    maxItems: 1
> +    description:
> +      Optional reset for the display controller. The driver does not
> +      assert or deassert this reset; it may be used by firmware or
> +      boot loaders to bring the hardware to a clean state.

Why is there a reset in hardware but not toggled in the software?

Reply: The driver will handle the reset in v2. The probe path will call
`devm_reset_control_bulk_get_optional_shared()` unconditionally with all
three names ("core", "axi", "ahb"). Since the MA35D1 DT only provides a
single reset named "core", the "axi" and "ahb" entries will be no-op
handles — optional bulk reset lookup skips absent entries silently.
The YAML `else` branch (covering all non-DC8200 variants, including
MA35D1) will constrain `reset-names` to a single item `core`, matching
the driver's `rsts[0].id`. The remove path will call
`reset_control_bulk_assert()` unconditionally; no-op handles are safe
to assert.

> +
> +  port:
> +    $ref: /schemas/graph.yaml#/properties/port
> +    description:
> +      Output port to the downstream display device (e.g. RGB panel).
> +      The DCUltra Lite supports a single parallel RGB output.
> +
> +required:
> +  - compatible
> +  - reg
> +  - interrupts
> +  - clocks
> +  - clock-names
> +  - port
> +
> +unevaluatedProperties: false
> +
> +examples:
> +  - |
> +    #include <dt-bindings/interrupt-controller/arm-gic.h>
> +    #include <dt-bindings/clock/nuvoton,ma35d1-clk.h>
> +    #include <dt-bindings/reset/nuvoton,ma35d1-reset.h>
> +
> +    display@40260000 {
> +        compatible = "nuvoton,ma35d1-dcu";
> +        reg = <0x40260000 0x2000>;
> +        interrupts = <GIC_SPI 20 IRQ_TYPE_LEVEL_HIGH>;
> +        clocks = <&clk DCU_GATE>, <&clk DCUP_DIV>;
> +        clock-names = "core", "pix0";
> +        resets = <&sys MA35D1_RESET_DISP>;
> +
> +        port {
> +            dpi_out: endpoint {
> +                remote-endpoint = <&panel_in>;
> +            };
> +        };
> +    };
> diff --git
> a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> index 9dc35ab973f2..00884529f8c1 100644
> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> @@ -9,15 +9,34 @@ title: Verisilicon DC-series display controllers
>  maintainers:
>    - Icenowy Zheng <uwu@icenowy.me>
>  
> +description:
> +  Verisilicon DC-series display controllers.
> +
> +# Select any node whose compatible contains one of the known
> Verisilicon DC
> +# or DC-derived compatible strings, including SoC-specific glue
> variants.
> +select:
> +  properties:
> +    compatible:
> +      contains:
> +        enum:
> +          - verisilicon,dc
> +          - thead,th1520-dc8200
> +          - nuvoton,ma35d1-dcu
> +  required:
> +    - compatible
> +
>  properties:
>    $nodename:
>      pattern: "^display@[0-9a-f]+$"
>  
>    compatible:
> -    items:
> -      - enum:
> -          - thead,th1520-dc8200
> -      - const: verisilicon,dc # DC IPs have discoverable ID/revision
> registers
> +    # Enumerated in full so the schema validator can verify any
> compatible
> +    # string against this list, including those from child schemas.
> +    contains:
> +      enum:
> +        - verisilicon,dc
> +        - thead,th1520-dc8200
> +        - nuvoton,ma35d1-dcu
>  
>    reg:
>      maxItems: 1
> @@ -26,32 +45,24 @@ properties:
>      maxItems: 1
>  
>    clocks:
> -    items:
> -      - description: DC Core clock
> -      - description: DMA AXI bus clock
> -      - description: Configuration AHB bus clock
> -      - description: Pixel clock of output 0
> -      - description: Pixel clock of output 1
> +    minItems: 2
> +    maxItems: 5
>  
>    clock-names:
> -    items:
> -      - const: core
> -      - const: axi
> -      - const: ahb
> -      - const: pix0
> -      - const: pix1
> +    minItems: 2
> +    maxItems: 5
>  
>    resets:
> -    items:
> -      - description: DC Core reset
> -      - description: DMA AXI bus reset
> -      - description: Configuration AHB bus reset
> +    minItems: 1
> +    maxItems: 3
>  
>    reset-names:
> -    items:
> -      - const: core
> -      - const: axi
> -      - const: ahb
> +    minItems: 1
> +    maxItems: 3
> +
> +  port:
> +    $ref: /schemas/graph.yaml#/properties/port
> +    description: Single video output port for single-output
> variants.
>  
>    ports:
>      $ref: /schemas/graph.yaml#/properties/ports
> @@ -59,7 +70,7 @@ properties:
>      properties:
>        port@0:
>          $ref: /schemas/graph.yaml#/properties/port
> -        description: The first output channel , endpoint 0 should be
> +        description: The first output channel, endpoint 0 should be
>            used for DPI format output and endpoint 1 should be used
>            for DP format output.
>  
> @@ -75,9 +86,8 @@ required:
>    - interrupts
>    - clocks
>    - clock-names
> -  - ports
>  
> -additionalProperties: false
> +additionalProperties: true
> 
