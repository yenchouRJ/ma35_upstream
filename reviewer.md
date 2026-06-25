# V4 of the patch series for Verisilicon DC driver

## local changable
- local driver at /home/joeylu/Documents/upstream2026/ma35_upstream/drm/verisilicon 
- local yaml at /home/joeylu/Documents/upstream2026/ma35_upstream/yaml/verisilicon,dc.yaml
- local dts at /home/joeylu/Documents/upstream2026/ma35_upstream/dts

## reference only
- The original patches at /home/joeylu/Documents/upstream2026/drm/kernel branch upstream-v4

# Reviewers' comments on the v4 of the patch series (Done)

## Re: [PATCH v4 1/6]

>      items:
>        - enum:
>            - thead,th1520-dc8200
> +          - nuvoton,ma35d1-dcu
>        - const: verisilicon,dc # DC IPs have discoverable ID/revision
> registers
>  
>    reg:
> @@ -26,14 +27,12 @@ properties:
>      maxItems: 1
>  
>    clocks:
> -    items:
> -      - description: DC Core clock
> -      - description: DMA AXI bus clock
> -      - description: Configuration AHB bus clock
> -      - description: Pixel clock of output 0
> -      - description: Pixel clock of output 1

Clock descriptions should still be in the global part instead of the
per-compatible part.

In the per-compatible part, clock-names should be constraint for SoCs.

Reply: I will move the `items:` clock descriptions back into the global `clocks:` property, covering all five possible clocks. In the per-compatible sections I will remove the description items and only constrain `clocks: minItems/maxItems` and `clock-names: minItems/maxItems`; for nuvoton,ma35d1-dcu I will additionally override `clock-names: items:` to the two names actually used (core, pix0).

>    reset-names:
> +    minItems: 1
> +    maxItems: 3
>      items:
>        - const: core
>        - const: axi
> @@ -59,7 +62,7 @@ properties:
>      properties:
>        port@0:
>          $ref: /schemas/graph.yaml#/properties/port
> -        description: The first output channel , endpoint 0 should be
> +        description: The first output channel, endpoint 0 should be

If you really want to fix this, please make it a separated patch
instead of doing it here, for commit atomicity.

Thanks,
Icenowy

Reply:  I’ll drop this change and keep it as is

## Re: [PATCH v4 2/6]

>   VSDC_DISP_IRQ_VSYNC(n) in vs_crtc_regs.h: bit mask for per-output
>   VSYNC interrupt bits in DISP_IRQ_STA (0x147C) / DISP_IRQ_EN
> (0x1480),
>   which are the IRQ registers used by DCUltraLite in place of the
> DC8200
>   TOP_IRQ_ACK / TOP_IRQ_EN registers.
>
>   VSDC_FB_CONFIG_ENABLE (bit 0), VSDC_FB_CONFIG_VALID (bit 3) and
>   VSDC_FB_CONFIG_RESET (bit 4) in vs_primary_plane_regs.h: control
> bits
>   in the FB_CONFIG register used by DCUltraLite for framebuffer
> enable
>   and per-frame commit handshake.

Validated against DC8000 register list.

Reviewed-by: Icenowy Zheng <zhengxingda@iscas.ac.cn>


## Re: [PATCH v4 3/6]

>  static void vs_crtc_atomic_disable(struct drm_crtc *crtc,
>  				   struct drm_atomic_commit *state)
>  {
> @@ -30,6 +53,9 @@ static void vs_crtc_atomic_disable(struct drm_crtc
> *crtc,
>  	drm_crtc_vblank_off(crtc);
>  
>  	clk_disable_unprepare(dc->pix_clk[output]);
> +
> +	if (dc->funcs->crtc_disable)
> +		dc->funcs->crtc_disable(dc, output);

Should this be `crtc_disable_ex` ? Because the clock-related operation
is shared.

Reply: I will rename `crtc_disable` to `crtc_disable_ex` and `crtc_enable` to `crtc_enable_ex` in v5 to make clear these hooks extend the shared clock operations rather than replace them.

>  }
>  
>  static void vs_crtc_atomic_enable(struct drm_crtc *crtc,
> @@ -42,6 +68,9 @@ static void vs_crtc_atomic_enable(struct drm_crtc
> *crtc,
>  	drm_WARN_ON(&dc->drm_dev->base,
>  		    clk_prepare_enable(dc->pix_clk[output]));
>  
> +	if (dc->funcs->crtc_enable)
> +		dc->funcs->crtc_enable(dc, output);
> 
Ditto for appending `_ex` .

Reply: Addressed above.

>  #include <linux/of.h>
>  #include <linux/of_graph.h>
>  
> -#include "vs_crtc.h"
>  #include "vs_dc.h"
> -#include "vs_dc_top_regs.h"
>  #include "vs_drm.h"
>  #include "vs_hwdb.h"
>  
> @@ -33,7 +31,7 @@ static irqreturn_t vs_dc_irq_handler(int irq, void
> *private)
>  	struct vs_dc *dc = private;
>  	u32 irqs;
>  
> -	regmap_read(dc->regs, VSDC_TOP_IRQ_ACK, &irqs);
> +	irqs = dc->funcs->irq_ack(dc);

The definition of bits in 0x0010 seems to be different to ones in
0x147C, although the first 2 bits are the same. (e.g. `BIT(2)` is
"cursor interrupt" in DC8000 0x147C but "secure reset done" in DC8200
0x0010).

Should some kind of translation be done in `irq_ack` ? (and add a
unified interrupt definition that aims to be the translation target).

Thanks,
Icenowy

Reply: I will add a set of unified IRQ bit definitions (e.g. `VSDC_IRQ_VSYNC(n)`) in a shared header. Each `irq_ack` implementation will translate its hardware-specific register bits into those unified bits before returning, so `vs_drm_handle_irq` can use the unified definitions without knowing which register layout was used. Currently the VSYNC bits (BIT(0)/BIT(1)) coincide between DC8200 0x0010 and DC8000 0x147C, so the translation is trivial; this change will prevent future confusion from diverging bit meanings (such as BIT(2)).


## Re: [PATCH v4 4/6]

> --- a/drivers/gpu/drm/verisilicon/vs_dc.c
> +++ b/drivers/gpu/drm/verisilicon/vs_dc.c
> @@ -90,13 +90,13 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  		return PTR_ERR(dc->core_clk);
>  	}
>  
> -	dc->axi_clk = devm_clk_get_enabled(dev, "axi");
> +	dc->axi_clk = devm_clk_get_optional_enabled(dev, "axi");
>  	if (IS_ERR(dc->axi_clk)) {
>  		dev_err(dev, "can't get axi clock\n");
>  		return PTR_ERR(dc->axi_clk);
>  	}
>  
> -	dc->ahb_clk = devm_clk_get_enabled(dev, "ahb");
> +	dc->ahb_clk = devm_clk_get_optional_enabled(dev, "ahb");

Please make the clock change a separated patch for atomicity.

BTW the MA35D1 manual's clock tree shows that DCUltra appears on AXI2
ACLK, AHB_HCLK2, behind a mux of SYS-PLL/EPLL-DIV2 (which seems to be
the core clock), and behind a divider (which seems to be the pixel
clock).

However it's weird that only one DCUltra Clock Enable Bit exists
despite both bus clocks have "ICG" (I think it means "Integrated Clock
Gating"). In addition the linux clk-ma35d1 driver assigns "dcu_gate" as
a downstream of "dcu_mux", although the Figure 6.5-2 in the TRM shows
no ICG after the "Display core CLK" mux.

Is the two bus clocks controlled by a single gate bit, and is the bit
also gating DC core clock?

Thanks,
Icenowy

Reply: I will split the axi/ahb optional-clock change into its own patch in v5 for atomicity.
Regarding the MA35D1 clock tree: from the TRM, the single "dcu_gate" bit gates both bus clocks (AXI ACLK and AHB HCLK) together with the display core clock through the same ICG cell. The clk-ma35d1 driver exposes only "dcu_gate" (downstream of "dcu_mux") and does not provide separate axi/ahb clock entries. Therefore the MA35D1 DT binding will use only two clocks ("core" and "pix0"); making axi and ahb optional in the driver is the correct approach, and this will be stated clearly in the split-out patch.

## Re: [PATCH v4 5/6]

> Register the Nuvoton MA35D1 DCUltraLite chip identity in
> vs_chip_identities[]:
>   model       = 0x0   (DCUltraLite; Verisilicon uses 0 for this IP)
>   revision    = 0x5560
>   customer_id = 0x305
>   generation  = VSDC_GEN_DC8000
>   display_count = 1
>   max_cursor_size = 32

I suggest make this more human-readable instead of replicating the
machine-readable data of HWDB.

My proposal here:

```
The Nuvoton MA35D1 chip contains a DCUltraLite display controller with
model number 0x0 (sic, the model name contains no number either),
revision 0x5560 and customer ID 0x305. It has a similar register map
with DC8000, only one display output and only 32x32 cursor supported.
```

Reply: I will use your proposed wording for the commit message in v5


> Placing this entry last makes it the gate that enables MA35D1
> hardware
> recognition only after all the supporting ops and DT binding changes
> are
> in place.

It's a little ambiguous that "last" here means whether the last in the
patchset or the last in the HWDB array, although I think it's not so
needed to explain the reason of the place in the patchset.

I propose just say `Adding it to the HWDB to enable it to be usable
with the verisilicon driver.` .

Reply: I will simplify the placement sentence to "Adding it to the HWDB to enable it to be usable with the verisilicon driver." in v5.


## Re: [PATCH v4 6/6]

> The DCUltraLite hardware ops and HWDB entry added in the preceding
> commits
> enable the driver to work on Nuvoton MA35D1 hardware.  Allow the
> driver
> to be built when ARCH_MA35 is selected; this dependency is meaningful
> only
> now that all supporting code is in place.

The explaination of patch sequence is not needed, but anyway,

`Reviewed-by: Icenowy Zheng <zhengxingda@iscas.ac.cn>`

Thanks,
Icenowy

# Reviewers' comments on the v4 of the patch series (Follow up)

## Re: [PATCH v4 1/6]

>>>     clocks:
>>> -    items:
>>> -      - description: DC Core clock
>>> -      - description: DMA AXI bus clock
>>> -      - description: Configuration AHB bus clock
>>> -      - description: Pixel clock of output 0
>>> -      - description: Pixel clock of output 1
>> Clock descriptions should still be in the global part instead of
>> the
>> per-compatible part.
>>
>> In the per-compatible part, clock-names should be constraint for
>> SoCs.
> I will move the `items:` clock descriptions back into the global 
> `clocks:` property, covering all five possible clocks. In the 
> per-compatible sections I will remove the description items and only 
> constrain `clocks: minItems/maxItems` and `clock-names: 
> minItems/maxItems`; for nuvoton,ma35d1-dcu I will additionally
> override 
> `clock-names: items:` to the two names actually used (core, pix0).

Yes, this should be the correct practice, although I wonder whether the
minItems and maxItems properties are needed globally (because these two
seem to have default implicit value).

BTW the MA35D1 manual in fact shows 4 clocks for "DCUltra" in the clock
tree, maybe the DT binding needs to be reconsidered?

Thanks,
Icenowy

Reply: I will drop the global `minItems`/`maxItems` on `clocks` and `clock-names` in v5, as they are redundant with the implicit defaults.
Regarding the 4-clock question: the TRM clock tree diagram does show four paths reaching DCUltra (display core mux/gate, AXI ACLK, AHB HCLK, and the pixel clock divider). However, the MA35D1 hardware provides only one software-controllable enable bit (SYSCLK0[26]) that gates the core clock together with the AXI and AHB bus clocks through shared ICG cells; there are no separate register bits for the bus clocks alone. Due to this hardware design constraint, the `clk-ma35d1` driver is intentionally designed to register only three DCU-related CCF nodes: `dcu_mux` (ID 61, an internal routing mux), `dcu_gate` (ID 62, the single gate at SYSCLK0 bit 26), and `dcup_div` (ID 63, the pixel divider from VPLL at CLKDIV0[18:16]), with no independent AXI or AHB gate entries for DCU. Since the DT binding can only reference clock handles that the platform clock driver actually provides, the MA35D1 binding will remain at two clock entries: "core" mapped to `DCU_GATE` and "pix0" mapped to `DCUP_DIV`.

## Re: [PATCH v4 4/6]

>>>   
>>> -	dc->axi_clk = devm_clk_get_enabled(dev, "axi");
>>> +	dc->axi_clk = devm_clk_get_optional_enabled(dev, "axi");
>>>   	if (IS_ERR(dc->axi_clk)) {
>>>   		dev_err(dev, "can't get axi clock\n");
>>>   		return PTR_ERR(dc->axi_clk);
>>>   	}
>>>   
>>> -	dc->ahb_clk = devm_clk_get_enabled(dev, "ahb");
>>> +	dc->ahb_clk = devm_clk_get_optional_enabled(dev, "ahb");
>> Please make the clock change a separated patch for atomicity.
>>
>> BTW the MA35D1 manual's clock tree shows that DCUltra appears on
>> AXI2
>> ACLK, AHB_HCLK2, behind a mux of SYS-PLL/EPLL-DIV2 (which seems to
>> be
>> the core clock), and behind a divider (which seems to be the pixel
>> clock).
>>
>> However it's weird that only one DCUltra Clock Enable Bit exists
>> despite both bus clocks have "ICG" (I think it means "Integrated
>> Clock
>> Gating"). In addition the linux clk-ma35d1 driver assigns
>> "dcu_gate" as
>> a downstream of "dcu_mux", although the Figure 6.5-2 in the TRM
>> shows
>> no ICG after the "Display core CLK" mux.
>>
>> Is the two bus clocks controlled by a single gate bit, and is the
>> bit
>> also gating DC core clock?
>>
>> Thanks,
>> Icenowy
> I will split the axi/ahb optional-clock change into its own patch in
> v5 
> for atomicity.
> Regarding the MA35D1 clock tree: from the TRM, the single "dcu_gate"
> bit 
> gates both bus clocks (AXI ACLK and AHB HCLK) together with the
> display 
> core clock through the same ICG cell. The clk-ma35d1 driver exposes
> only 
> "dcu_gate" (downstream of "dcu_mux") and does not provide separate 

Then it's one of the case that the clock tree doesn't properly
represent the hardware, which is bad. However, as three gates share the
same bit, I am not sure how to represent such kind of thing in the
common clk framework.

> axi/ahb clock entries. Therefore the MA35D1 DT binding will use only
> two 
> clocks ("core" and "pix0"); making axi and ahb optional in the driver
> is the correct approach, and this will be stated clearly in the
>  split-out patch.

I agree to make them optional, although these two clocks do exist in
the hardware of MA35D1.

Thanks,
Icenowy

Reply: As mentioned in the DT binding reply, the absence of separate AXI/AHB clock entries for DCU in `clk-ma35d1` is due to the hardware design constraint of a single shared enable bit, not a driver oversight. In v5, the axi and ahb clock fetches in `vs_dc_probe` will be split into their own patch and made optional via `devm_clk_get_optional_enabled`, with a comment explaining that on MA35D1 the AXI and AHB bus clocks share the single `dcu_gate` enable bit and are therefore not separately exposed by the clock driver.
