# Reviewers' comments on the v3 of the patch series (Done)

## Re: [PATCH v3 1/5]

### Reviwer Icenowy Zheng

> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> @@ -17,7 +17,8 @@ properties:
>      items:
>        - enum:
>            - thead,th1520-dc8200
> -      - const: verisilicon,dc # DC IPs have discoverable ID/revision
> registers
> +          - nuvoton,ma35d1-dcu
> +      - const: verisilicon,dc  # DC IPs have discoverable
> ID/revision registers

Ah is an extra space added here, which leads to this hunk looking
strange?

**Reply:** The extra space was added because `yamllint` reports "too few spaces before comment" (warning: comments) when only one space precedes the `#`. However, since this constitutes an unrelated whitespace change that makes the diff harder to read, I will revert to the original single-space form to keep the patch clean.

>  
>    reg:
>      maxItems: 1
> @@ -26,6 +27,7 @@ properties:
>      maxItems: 1
>  
>    clocks:
> +    minItems: 2

Maybe restrictions about the clock count shouldn't be inserted here,
and technically it's possible that only the pixel clock is controllable
by Linux (all other clocks are in a fixed configuration).

**Reply:** Understood. I will remove the per-variant clock items descriptions from the top-level `clocks:` section and move them into the respective allOf/if blocks. The top-level will only carry `minItems`/`maxItems` for schema validation range.

>      items:
>        - description: DC Core clock
>        - description: DMA AXI bus clock
> @@ -34,24 +36,19 @@ properties:
>        - description: Pixel clock of output 1
>  
>    clock-names:
> -    items:
> -      - const: core
> -      - const: axi
> -      - const: ahb
> -      - const: pix0
> -      - const: pix1

Ah I think the total list should still appear here, and they should be
corresponding to the descriptions above?

**Reply:** Understood. I will restore the full items list for `clock-names` at the top level (all five entries: core, axi, ahb, pix0, pix1) and add `minItems` to make it flexible. Per-variant allOf blocks will only constrain with `minItems`/`maxItems`.

> +    minItems: 2
> +    maxItems: 5
>  
>    resets:
> +    minItems: 1
>      items:
>        - description: DC Core reset
>        - description: DMA AXI bus reset
>        - description: Configuration AHB bus reset
>  
>    reset-names:
> -    items:
> -      - const: core
> -      - const: axi
> -      - const: ahb

Ditto here.

**Reply:** Understood. I will restore the full items list for `reset-names` at the top level (core, axi, ahb) with `minItems`. Same pattern as clock-names.

> +    minItems: 1
> +    maxItems: 3
>  
>    ports:
>      $ref: /schemas/graph.yaml#/properties/ports
> @@ -59,7 +56,7 @@ properties:
>      properties:
>        port@0:
>          $ref: /schemas/graph.yaml#/properties/port
> -        description: The first output channel , endpoint 0 should be
> +        description: The first output channel, endpoint 0 should be
>            used for DPI format output and endpoint 1 should be used
>            for DP format output.
>  
> @@ -77,7 +74,60 @@ required:
>    - clock-names
>    - ports
>  
> -additionalProperties: false
> +allOf:
> +  - if:
> +      properties:
> +        compatible:
> +          contains:
> +            const: thead,th1520-dc8200
> +    then:
> +      properties:
> +        clocks:
> +          minItems: 5
> +          maxItems: 5
> +
> +        clock-names:
> +          items:
> +            - const: core
> +            - const: axi
> +            - const: ahb
> +            - const: pix0
> +            - const: pix1
> +
> +        resets:
> +          minItems: 3
> +          maxItems: 3
> +
> +        reset-names:
> +          items:
> +            - const: core
> +            - const: axi
> +            - const: ahb
> +
> +  - if:
> +      properties:
> +        compatible:
> +          contains:
> +            const: nuvoton,ma35d1-dcu
> +    then:
> +      properties:
> +        clocks:
> +          minItems: 2
> +          maxItems: 2
> +
> +        clock-names:
> +          items:
> +            - const: core
> +            - const: pix0
> +
> +        resets:

Do we have minItems: 1 here? (The DT schema validator always has some
quirks that I fail to remember, so I am not sure.)

**Reply:** Yes, I will add `minItems: 1` to `resets:` in the nuvoton block.

> +          maxItems: 1
> +
> +        reset-names:
> +          items:
> +            - const: core
> +

I think resets should be described as required in both device-specific
bindings.

**Reply:** Understood. I will add `required: [resets, reset-names]` inside the `then:` block for both thead,th1520-dc8200 and nuvoton,ma35d1-dcu.

Thanks,
Icenowy

### Reviewer Krzysztof Kozlowski

> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> @@ -17,7 +17,8 @@ properties:
>      items:
>        - enum:
>            - thead,th1520-dc8200
> -      - const: verisilicon,dc # DC IPs have discoverable ID/revision registers
> +          - nuvoton,ma35d1-dcu
> +      - const: verisilicon,dc  # DC IPs have discoverable ID/revision registers

Why do you need to change indentation? Why introducing irrelevant
changes to the diff?

**Reply:** The extra space was introduced to satisfy `yamllint`'s "too few spaces before comment" warning, which requires two spaces before an inline `#`. Since this is an unrelated change that pollutes the diff, I will revert it to the original single-space form.

>  
>    reg:
>      maxItems: 1
> @@ -26,6 +27,7 @@ properties:
>      maxItems: 1
>  
>   clocks:
> +    minItems: 2
>      items:
>        - description: DC Core clock
>        - description: DMA AXI bus clock

That's not true anymore. In such case the list should also be defined
per variant and here only min/maxItems.


**Reply:** Understood. I will remove the `items:` description list from the top-level `clocks:` and keep only `minItems`/`maxItems`. The per-variant items descriptions will be moved into the allOf/if blocks.

> @@ -34,24 +36,19 @@ properties:
>        - description: Pixel clock of output 1
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
> +    minItems: 1
>      items:
>        - description: DC Core reset
>        - description: DMA AXI bus reset
>        - description: Configuration AHB bus reset
>  
>    reset-names:
> -    items:
> -      - const: core
> -      - const: axi
> -      - const: ahb

This stays, with minItems. Variants only need min/maxItems

**Reply:** Understood. I will restore the top-level `clock-names` and `reset-names` items lists and add `minItems` to each. The per-variant allOf blocks will only carry `minItems`/`maxItems`.

> +    minItems: 1
> +    maxItems: 3
>  
>    ports:
>      $ref: /schemas/graph.yaml#/properties/ports
> @@ -59,7 +56,7 @@ properties:
>      properties:
>        port@0:
>          $ref: /schemas/graph.yaml#/properties/port
> -        description: The first output channel , endpoint 0 should be
> +        description: The first output channel, endpoint 0 should be
>            used for DPI format output and endpoint 1 should be used
>            for DP format output.
>  
> @@ -77,7 +74,60 @@ required:
>    - clock-names
>    - ports
>  
> -additionalProperties: false
> +allOf:
> +  - if:
> +      properties:
> +        compatible:
> +          contains:
> +            const: thead,th1520-dc8200
> +    then:
> +      properties:
> +        clocks:
> +          minItems: 5
> +          maxItems: 5
> +
> +        clock-names:
> +          items:
> +            - const: core
> +            - const: axi
> +            - const: ahb
> +            - const: pix0
> +            - const: pix1
> +
> +        resets:
> +          minItems: 3
> +          maxItems: 3
> +
> +        reset-names:

minItems: 3

**Reply:** Understood. I will add `minItems: 3` to `reset-names` in the thead,th1520-dc8200 block.

> +          items:
> +            - const: core
> +            - const: axi
> +            - const: ahb
> +
> +  - if:
> +      properties:
> +        compatible:
> +          contains:
> +            const: nuvoton,ma35d1-dcu
> +    then:
> +      properties:
> +        clocks:
> +          minItems: 2
> +          maxItems: 2
> +
> +        clock-names:
> +          items:
> +            - const: core
> +            - const: pix0
> +
> +        resets:
> +          maxItems: 1
> +
> +        reset-names:

maxItems: 1

**Reply:** Understood. I will add `maxItems: 1` to `reset-names` in the nuvoton block.

> +          items:
> +            - const: core
> +
> +unevaluatedProperties: false

Stop making random changes to the binding.

**Reply:** Understood. I will revert to `additionalProperties: false` as in the original binding.

Best regards,
Krzysztof

> The existing schema hard-codes the five-clock/three-reset/dual-port
> topology of the DC8200 IP block, preventing reuse for single-output
> variants such as the Verisilicon DCUltraLite used in the Nuvoton MA35D1
> SoC.
>
> Rework the schema so that variant-specific constraints are expressed via
> allOf/if blocks:
>
> - Add nuvoton,ma35d1-dcu to the SoC-specific compatible enum.  The
>   generic verisilicon,dc fallback remains the driver-binding string.
> - Relax the top-level clocks/resets definitions to minItems ranges so
>   the base schema accepts both variants.
> - Keep ports in the global required list and keep additionalProperties
>   tightened to unevaluatedProperties.
> - Add an allOf/if block for thead,th1520-dc8200: five-clock (core, axi,
>   ahb, pix0, pix1), three-reset (core, axi, ahb).
> - Add an allOf/if block for nuvoton,ma35d1-dcu: two-clock (core, pix0),
>   one-reset (core).
> - Fix a stray space in the port@0 description.
> - Add a DT example for the Nuvoton MA35D1 DCU Lite using ports/port@0.

Difference in clocks and resets does not need a new new example.

**Reply:** Understood. I will remove the second example for nuvoton,ma35d1-dcu from the binding.

Best regards,
Krzysztof

## Re: [PATCH v3 3/5]

> diff --git a/drivers/gpu/drm/verisilicon/vs_bridge.c
> b/drivers/gpu/drm/verisilicon/vs_bridge.c
> index 7a93049368db..6a9af10c64e6 100644
> --- a/drivers/gpu/drm/verisilicon/vs_bridge.c
> +++ b/drivers/gpu/drm/verisilicon/vs_bridge.c
> @@ -162,15 +162,8 @@ static void vs_bridge_enable_common(struct
> vs_crtc *crtc,
>  			VSDC_DISP_PANEL_CONFIG_DE_EN |
>  			VSDC_DISP_PANEL_CONFIG_DAT_EN |
>  			VSDC_DISP_PANEL_CONFIG_CLK_EN);
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_CONFIG(output),
> -			VSDC_DISP_PANEL_CONFIG_RUNNING);
> -	regmap_clear_bits(dc->regs, VSDC_DISP_PANEL_START,
> -			  VSDC_DISP_PANEL_START_MULTI_DISP_SYNC);
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_START,
> -			VSDC_DISP_PANEL_START_RUNNING(output));
> -
> -	regmap_set_bits(dc->regs, VSDC_DISP_PANEL_CONFIG_EX(crtc-
>> id),
> -			VSDC_DISP_PANEL_CONFIG_EX_COMMIT);
> +
> +	dc->funcs->bridge_enable(dc, output);

The code here being called "bridge" is only internal to kernel. Naming
it in such a way is okay, but maybe naming it "panel" is better
(because they're configuring PANEL-named registers).

And, as the common code setting common fields of DcregPanelConfig0 is
still here, maybe the helper name should be named "panel_enable_ex" (or
"bridge_enable_ex") ?

**Reply:** Understood. I will rename `bridge_enable`/`bridge_disable` to `panel_enable_ex`/`panel_disable_ex` throughout: in `vs_dc_funcs`, `vs_dc8200.c`, `vs_dc8000.c`, and the call sites in `vs_bridge.c`.

>  }
>  
>  static const struct drm_bridge_funcs vs_dpi_bridge_funcs = {
> ====== 8< ==============
> diff --git a/drivers/gpu/drm/verisilicon/vs_dc.c
> b/drivers/gpu/drm/verisilicon/vs_dc.c
> index dad9967bc10b..c94957024189 100644
> --- a/drivers/gpu/drm/verisilicon/vs_dc.c
> +++ b/drivers/gpu/drm/verisilicon/vs_dc.c
> @@ -8,9 +8,7 @@
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
> +	irqs = dc->funcs->irq_handler(dc);

The IRQ isn't handled in this helper.

So maybe call it "irq_ack"?

**Reply:** Understood. I will rename `irq_handler` to `irq_ack` in `vs_dc_funcs`, `vs_dc8200.c`, `vs_dc8000.c`, and the call site in `vs_dc.c`.

>  
>  	vs_drm_handle_irq(dc, irqs);
>  
> @@ -136,6 +134,8 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  	dev_info(dev, "Found DC%x rev %x customer %x\n", dc-
>> identity.model,
>  		 dc->identity.revision, dc->identity.customer_id);
>  
> +	dc->funcs = &vs_dc8200_funcs;
> +
>  	if (port_count > dc->identity.display_count) {
>  		dev_err(dev, "too many downstream ports than HW
> capability\n");
>  		ret = -EINVAL;
> diff --git a/drivers/gpu/drm/verisilicon/vs_dc.h
> b/drivers/gpu/drm/verisilicon/vs_dc.h
> index ed1016f18758..d77d4a1babdf 100644
> --- a/drivers/gpu/drm/verisilicon/vs_dc.h
> +++ b/drivers/gpu/drm/verisilicon/vs_dc.h
> @@ -14,6 +14,7 @@
>  #include <linux/reset.h>
>  
>  #include <drm/drm_device.h>
> +#include <drm/drm_plane.h>
>  
>  #include "vs_hwdb.h"
>  
> @@ -22,6 +23,34 @@
>  
>  struct vs_drm_dev;
>  struct vs_crtc;
> +struct vs_dc;
> +
> +struct vs_dc_funcs {
> +	/* Bridge: atomic_enable, atomic_disable */
> +	void (*bridge_enable)(struct vs_dc *dc, unsigned int
> output);
> +	void (*bridge_disable)(struct vs_dc *dc, unsigned int
> output);
> +
> +	/* CRTC: atomic_begin, atomic_flush */
> +	void (*crtc_begin)(struct vs_dc *dc, unsigned int output);
> +	void (*crtc_flush)(struct vs_dc *dc, unsigned int output);
> +
> +	/* CRTC: atomic_enable, atomic_disable */
> +	void (*crtc_enable)(struct vs_dc *dc, unsigned int output);
> +	void (*crtc_disable)(struct vs_dc *dc, unsigned int output);
> +
> +	/* CRTC: enable_vblank, disable_vblank */
> +	void (*enable_vblank)(struct vs_dc *dc, unsigned int
> output);
> +	void (*disable_vblank)(struct vs_dc *dc, unsigned int
> output);
> +
> +	/* Primary plane: atomic_enable, atomic_disable,
> atomic_update */
> +	void (*plane_enable_ex)(struct vs_dc *dc, unsigned int
> output);
> +	void (*plane_disable_ex)(struct vs_dc *dc, unsigned int
> output);
> +	void (*plane_update_ex)(struct vs_dc *dc, unsigned int
> output,
> +				struct drm_plane_state *state);
> +
> +	/* IRQ handler */
> +	u32 (*irq_handler)(struct vs_dc *dc);

See my comments elsewhere for the helper naming.

**Reply:** Understood. I will rename all vtable members per the comments above: `panel_enable_ex`, `panel_disable_ex`, `primary_plane_enable_ex`, `primary_plane_disable_ex`, `primary_plane_update_ex`, `irq_ack`.

> +};
>  
>  struct vs_dc {
>  	struct regmap *regs;
> ============= 8< =================
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> index 2a0f7c59afa3..91524d16f778 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> @@ -94,6 +94,7 @@ static struct vs_chip_identity vs_chip_identities[]
> = {
>  		.revision = 0x5720,
>  		.customer_id = ~0U,
>  
> +		.generation = VSDC_GEN_DC8200,
>  		.display_count = 2,
>  		.max_cursor_size = 64,
>  		.formats = &vs_formats_no_yuv444,
> @@ -103,6 +104,7 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.revision = 0x5721,
>  		.customer_id = 0x30B,
>  
> +		.generation = VSDC_GEN_DC8200,
>  		.display_count = 2,
>  		.max_cursor_size = 64,
>  		.formats = &vs_formats_no_yuv444,
> @@ -112,6 +114,7 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.revision = 0x5720,
>  		.customer_id = 0x310,
>  
> +		.generation = VSDC_GEN_DC8200,
>  		.display_count = 2,
>  		.max_cursor_size = 64,
>  		.formats = &vs_formats_with_yuv444,
> @@ -121,6 +124,7 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.revision = 0x5720,
>  		.customer_id = 0x311,
>  
> +		.generation = VSDC_GEN_DC8200,
>  		.display_count = 2,
>  		.max_cursor_size = 64,
>  		.formats = &vs_formats_no_yuv444,
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> index 2065ecb73043..a15c8b565604 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> @@ -9,6 +9,11 @@
>  #include <linux/regmap.h>
>  #include <linux/types.h>
>  
> +enum vs_dc_generation {
> +	VSDC_GEN_DC8000,
> +	VSDC_GEN_DC8200,
> +};
> +
>  struct vs_formats {
>  	const u32 *array;
>  	unsigned int num;
> @@ -19,6 +24,7 @@ struct vs_chip_identity {
>  	u32 revision;
>  	u32 customer_id;
>  
> +	enum vs_dc_generation generation;
>  	u32 display_count;
>  	/*
>  	 * The hardware only supports square cursor planes, so this
> field
> diff --git a/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> b/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> index 1f2be41ae496..75bc36a078f7 100644
> --- a/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> +++ b/drivers/gpu/drm/verisilicon/vs_primary_plane.c
> @@ -53,12 +53,6 @@ static int vs_primary_plane_atomic_check(struct
> drm_plane *plane,
>  	return 0;
>  }
>  
> -static void vs_primary_plane_commit(struct vs_dc *dc, unsigned int
> output)
> -{
> -	regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			VSDC_FB_CONFIG_EX_COMMIT);
> -}
> -
>  static void vs_primary_plane_atomic_enable(struct drm_plane *plane,
>  					   struct drm_atomic_commit
> *atomic_state)
>  {
> @@ -69,13 +63,8 @@ static void vs_primary_plane_atomic_enable(struct
> drm_plane *plane,
>  	unsigned int output = vcrtc->id;
>  	struct vs_dc *dc = vcrtc->dc;
>  
> -	regmap_set_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			VSDC_FB_CONFIG_EX_FB_EN);
> -	regmap_update_bits(dc->regs, VSDC_FB_CONFIG_EX(output),
> -			   VSDC_FB_CONFIG_EX_DISPLAY_ID_MASK,
> -			   VSDC_FB_CONFIG_EX_DISPLAY_ID(output));
> -
> -	vs_primary_plane_commit(dc, output);
> +	if (dc->funcs->plane_enable_ex)
> +		dc->funcs->plane_enable_ex(dc, output);

Please note that all theae codes are for primary planes, maybe the
helper should be named mentioning primary. Overlay planes will need a
different codepath because they change different registers.

**Reply:** Understood. To avoid confusion, I will rename `plane_enable_ex`, `plane_disable_ex`, and `plane_update_ex` to `primary_plane_enable_ex`, `primary_plane_disable_ex`, and `primary_plane_update_ex` in `vs_dc_funcs`, `vs_dc8200.c`, and `vs_primary_plane.c`.

Thanks,
Icenowy


## Re: [PATCH v3 4/5]

> The Nuvoton MA35D1 SoC integrates a Verisilicon DCUltraLite display
> controller whose register layout differs from the DC8200 in several
> important ways:
>
> 1. No CONFIG_EX commit path: framebuffer updates use the enable (bit
> 0)
>    and reset (bit 4) bits in FB_CONFIG instead of the DC8200 staging
>    registers (FB_CONFIG_EX, FB_TOP_LEFT, FB_BOTTOM_RIGHT,
>    FB_BLEND_CONFIG, PANEL_CONFIG_EX).
>
> 2. No PANEL_START register: panel output starts when
>    PANEL_CONFIG.RUNNING is set; there is no multi-display sync start
>    register.
>
> 3. Different IRQ registers: DCUltraLite uses DISP_IRQ_STA (0x147C) /
>    DISP_IRQ_EN (0x1480) versus DC8200's TOP_IRQ_ACK (0x0010) /
>    TOP_IRQ_EN (0x0014).
>
> 4. Per-frame commit cycle: DCUltraLite requires the VALID bit in
>    FB_CONFIG to be set at the start of each atomic commit
> (crtc_begin)
>    and cleared after (crtc_flush).
>
> 5. Simpler clock topology: only 'core' (bus gate) and 'pix0' (pixel
>    divider) clocks; no axi or ahb clocks required.  Make axi_clk and
>    ahb_clk optional (devm_clk_get_optional_enabled) so DCUltraLite
>    nodes without those clocks are handled gracefully.
>
> Add vs_dcu_lite.c implementing the vs_dc_funcs vtable for the above

Nitpick: could you use vs_dc8000 to make things more aligned? (Although
I must admit that DCUltraLite is the first revision to be supported in
this codepath).

**Reply:** Understood. I will rename `vs_dcu_lite.c` to `vs_dc8000.c`, all internal functions from `vs_dcu_lite_*` to `vs_dc8000_*`, the exported symbol from `vs_dcu_lite_funcs` to `vs_dc8000_funcs`, and update the Makefile accordingly.

> differences.  The probe now selects vs_dcu_lite_funcs when the
> identified generation is VSDC_GEN_DC8000 (DCUltraLite reads model
> 0x0,
> revision 0x5560, customer_id 0x305).
>
> Extend Kconfig to allow building on ARCH_MA35 platforms.

Maybe the Kconfig change could be in the last commit or a dedicated
commit before current ones? Because it's only meaningful after the HWDB
item is added.

**Reply:** Understood. The Kconfig change adding `ARCH_MA35` will be moved to a separate commit placed after the HWDB entry is added, or as the final commit in the series.

Thanks,
Icenowy


# TODOs:

I have prepared changes for v4 upstream, help me commit the changes (read above reviewer's comments and our replies, and you can mention the important parts in what changes from v3) and generate the patch series for v4. The source code is at `/home/joeylu/Documents/upstream2026/drm/kernel` branch `upstream-v4`

You can refer for v3's patch series at `/home/joeylu/Documents/upstream2026/drm/mail/patch-v3` (there are 5 patches, it's fine to increase the number of patches if needed), then generate v4 patch series to `/home/joeylu/Documents/upstream2026/drm/mail/patch-v4` with cover-letter and changes from v3

