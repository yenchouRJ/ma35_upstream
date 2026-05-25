The first replied part is the discussed change plan for /home/joeylu/Documents/upstream2026/ma35_upstream/yaml/verisilicon,dc.yaml which is v2 of the patch series.
The driver part looks okay for reviewer, but yaml part is still under discussion, i.e. continued part
You can refer to the original yaml /home/joeylu/Documents/upstream2026/ma35_upstream/verisilicon,dc-original.yaml and reason the follow-up comments by reviewer.

please help me:
1. - satisfy the comments and update the driver at /home/joeylu/Documents/upstream2026/ma35_upstream/drm/verisilicon and yaml at /home/joeylu/Documents/upstream2026/ma35_upstream/yaml accordingly.
2. - reply to the comments one by one(below each comment) with brief descriptions of the changes I'll make, e.g. Understood, I will fix it

# Reviewers' comments on the v2 of the patch series (replied)

## Re: [PATCH v2 1/4]

On Tue, May 19, 2026 at 03:26:58PM +0800, Icenowy Zheng wrote:
> 在 2026-05-19二的 13:51 +0800，Joey Lu写道：
>> The existing schema assumes a fixed clock/reset topology and dual-
>> output
>> port structure matching the DC8200 IP block.  This prevents reuse for
>> single-output variants such as the Verisilicon DCU Lite used in the
>> Nuvoton MA35D1 SoC.
>>
>> Rework the schema so that variant-specific constraints are expressed
>> via allOf/if-then-else:
>>
>> - The thead,th1520-dc8200 compatible keeps its existing five-clock,
>>   three-reset, dual-port requirements.
>>
>> - A standalone verisilicon,dc compatible covers IPs whose identity is
>>   discovered entirely through hardware registers; these have flexible
>>   clock and reset counts, a single 'port' property, and no 'ports'
>>   requirement.
>>
>> Changes to the base schema:
>> - Replace the fixed clock/reset items lists with minItems/maxItems
>>   ranges; variant sub-schemas tighten the constraints via if-then-
>> else.
>> - Add a 'port' property (graph.yaml single-port alias) alongside the
>>   existing 'ports', for single-output variants.
>> - Drop the unconditional 'ports' requirement; each if-branch enforces
>>   its own port topology.
>> - Tighten additionalProperties to unevaluatedProperties to allow
>>   per-variant schemas to add their own constraints cleanly.
>> - Fix a stray space in the port@0 description.
>> - Add a DT example for the generic verisilicon,dc compatible
>>   (Nuvoton MA35D1 DCU Lite).
>>
>> Signed-off-by: Joey Lu <a0987203069@gmail.com>
>> ---
>>  .../bindings/display/verisilicon,dc.yaml      | 135 ++++++++++++++--
>> --
>>  1 file changed, 108 insertions(+), 27 deletions(-)
>>
>> diff --git
>> a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> index 9dc35ab973f2..3a814c2e083e 100644
>> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> @@ -14,10 +14,12 @@ properties:
>>      pattern: "^display@[0-9a-f]+$"
>>  
>>    compatible:
>> -    items:
>> -      - enum:
>> -          - thead,th1520-dc8200
>
> You should add a fallback compatible here for your SoC, in case its
> integration gets something quirky; this compatible is usually not
> consumed by the driver (see how thead,th1520-dc8200 exists in the
> binding but not the diver).

s/fallback compatible/soc-specific compatible/, but yes.
NAK to what's been done here, especially after the discussions on
earlier versions of this verisilicon binding.
pw-bot: changes-requested

**Reply:** Understood. I will add `nuvoton,ma35d1-dcu` as the SoC-specific compatible string paired with `verisilicon,dc` as the generic fallback, matching the pattern used for `thead,th1520-dc8200`. The standalone `verisilicon,dc` compatible will be removed from the binding. The driver match table is not changed since hardware detection is done via ID registers.

>
>> -      - const: verisilicon,dc # DC IPs have discoverable ID/revision
>> registers
>> +    oneOf:
>> +      - items:
>> +          - enum:
>> +              - thead,th1520-dc8200
>> +          - const: verisilicon,dc
>> +      - const: verisilicon,dc  # DC IPs have discoverable
>> ID/revision registers
>>  
>>    reg:
>>      maxItems: 1
>> @@ -26,32 +28,24 @@ properties:
>>      maxItems: 1
>>  
>>    clocks:
>> -    items:
>> -      - description: DC Core clock
>> -      - description: DMA AXI bus clock
>> -      - description: Configuration AHB bus clock
>> -      - description: Pixel clock of output 0
>> -      - description: Pixel clock of output 1
>> +    minItems: 2
>> +    maxItems: 5
>>  
>>    clock-names:
>> -    items:
>> -      - const: core
>> -      - const: axi
>> -      - const: ahb
>> -      - const: pix0
>> -      - const: pix1
>> +    minItems: 2
>> +    maxItems: 5
>>  
>>    resets:
>> -    items:
>> -      - description: DC Core reset
>> -      - description: DMA AXI bus reset
>> -      - description: Configuration AHB bus reset
>> +    minItems: 1
>> +    maxItems: 3
>>  
>>    reset-names:
>> -    items:
>> -      - const: core
>> -      - const: axi
>> -      - const: ahb
>> +    minItems: 1
>> +    maxItems: 3
>> +
>> +  port:
>> +    $ref: /schemas/graph.yaml#/properties/port
>> +    description: Single video output port for single-output
>> variants.
>
> Maybe the endpoint numbering rule needs a move to here? (I am not very
> sure).

**Reply:** I will add a description to the `port` property noting that endpoint 0 is used for DPI output, which is the only output type for DCUltraLite.
>
>>  
>>    ports:
>>      $ref: /schemas/graph.yaml#/properties/ports
>> @@ -59,7 +53,7 @@ properties:
>>      properties:
>>        port@0:
>>          $ref: /schemas/graph.yaml#/properties/port
>> -        description: The first output channel , endpoint 0 should be
>> +        description: The first output channel, endpoint 0 should be
>>            used for DPI format output and endpoint 1 should be used
>>            for DP format output.
>>  
>> @@ -75,9 +69,75 @@ required:
>>    - interrupts
>>    - clocks
>>    - clock-names
>> -  - ports
>>  
>> -additionalProperties: false
>> +allOf:
>> +  - if:
>> +      properties:
>> +        compatible:
>> +          contains:
>> +            const: thead,th1520-dc8200
>> +    then:
>> +      properties:
>> +        clocks:
>> +          items:
>> +            - description: DC Core clock
>> +            - description: DMA AXI bus clock
>> +            - description: Configuration AHB bus clock
>> +            - description: Pixel clock of output 0
>> +            - description: Pixel clock of output 1
>> +
>> +        clock-names:
>> +          items:
>> +            - const: core
>> +            - const: axi
>> +            - const: ahb
>> +            - const: pix0
>> +            - const: pix1
>> +
>> +        resets:
>> +          items:
>> +            - description: DC Core reset
>> +            - description: DMA AXI bus reset
>> +            - description: Configuration AHB bus reset
>> +
>> +        reset-names:
>> +          items:
>> +            - const: core
>> +            - const: axi
>> +            - const: ahb
>> +
>> +      required:
>> +        - ports
>> +
>> +    else:
>> +      properties:
>> +        clocks:
>> +          items:
>> +            - description: Bus clock that gates register access
>> +            - description: Pixel clock divider for display timing
>
> Please don't make compatible-specific description strings for
> individual compatibles, and keep these descriptions outside of the if.
> The compatible-specific part should be used to specify what's required
> for the specific SoC, for dt validation purpose.
>
> BTW if the clock is both the working clock and bus clock for the
> controller, I suggest listing it twice, except if the IP core is
> provided without a dedicated core clock (in the case I suggest to use
> "bus" only).

I agree. If the same clock is provided to two+ ports on the IP, that
should still be two+ clocks in the devicetree.

>
> Here's an example for "listing it twice":
> ```
> clocks = <&clk DCU_GATE>, <&clk DCU_GATE>, <&clk DCUP_DIV>;
> clock-names = "core", "bus", "pix0";
> ```
>
> Well nonetheless the name "core" does not match the description "Bus
> clock that gates register access".
>
> Thanks,
> Icenowy

**Reply:** Understood. I will remove all description strings from the if/else branches; the if/then clauses will only constrain clock-names and reset-names items (name values only, no descriptions). Regarding clock naming: DCU_GATE on MA35D1 is a peripheral gate clock without a separate dedicated core working clock, so I will keep "core" as the name and drop the misleading description "Bus clock that gates register access". The description mismatch was entirely in the if/else strings which are now removed.
>
>> +
>> +        clock-names:
>> +          items:
>> +            - const: core
>> +            - const: pix0
>> +
>> +        resets:
>> +          maxItems: 1
>> +          description:
>> +            Reset line for the display controller.
>> +
>> +        reset-names:
>> +          items:
>> +            - const: core
>> +
>> +      required:
>> +        - port
>> +
>> +      not:
>> +        required:
>> +          - ports
>> +
>> +unevaluatedProperties: false
>>  
>>  examples:
>>    - |
>> @@ -120,3 +180,24 @@ examples:
>>          };
>>        };
>>      };
>> +
>> +  - |
>> +    #include <dt-bindings/interrupt-controller/arm-gic.h>
>> +    #include <dt-bindings/clock/nuvoton,ma35d1-clk.h>
>> +    #include <dt-bindings/reset/nuvoton,ma35d1-reset.h>
>> +
>> +    display@40260000 {
>> +        compatible = "verisilicon,dc";
>> +        reg = <0x40260000 0x20000>;
>> +        interrupts = <GIC_SPI 20 IRQ_TYPE_LEVEL_HIGH>;
>> +        clocks = <&clk DCU_GATE>, <&clk DCUP_DIV>;
>> +        clock-names = "core", "pix0";
>> +        resets = <&sys MA35D1_RESET_DISP>;
>> +        reset-names = "core";
>> +
>> +        port {
>> +            dpi_out: endpoint {
>> +                remote-endpoint = <&panel_in>;
>> +            };
>> +        };
>> +    };


## Re: [PATCH v2 2/4]

在 2026-05-19二的 13:51 +0800，Joey Lu写道：
> Introduce symbolic constants VSDC_MODEL_DC8200 and
> VSDC_MODEL_DCU_LITE
> to replace magic numbers in the hardware database and probe path.
>
> Register the DCU Lite chip identity (model 0x0, revision 0x5560,
> customer_id 0x305) in vs_chip_identities[], making the existing
> vs_fill_chip_identity() path able to recognise Nuvoton MA35D1
> hardware
> purely through register reads.

The HWDB change should be added in the end of the series, making it a
gate to the newly added changes that is finally opened when
everything's ready.

**Reply:** Understood. I will move the DCUltraLite entry in vs_chip_identities[] to the last patch in the series, so the new hardware is only reachable once the ops and DTS support are fully in place.

>
> Also add three register-level macros for forthcoming DCU Lite
> support:
> - VSDC_DISP_IRQ_VSYNC(n) in vs_crtc_regs.h, for per-output VSYNC IRQ
>   bits used by the DCU Lite IRQ enable/status registers.
> - VSDC_FB_CONFIG_ENABLE, VSDC_FB_CONFIG_VALID and
> VSDC_FB_CONFIG_RESET
>   in vs_primary_plane_regs.h, for the framebuffer enable and
>   commit-cycle bits used by the DCU Lite plane update path.

Maybe you can split the register change 

**Reply:** Understood. I will split the register macro additions into a separate patch: one for the new vs_crtc_regs.h IRQ macro and one for the vs_primary_plane_regs.h FB_CONFIG bits, keeping them independent of the HWDB identity change.

>
> No behaviour change for existing DC8200 platforms.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>
> ---
>  drivers/gpu/drm/verisilicon/vs_crtc_regs.h       |  1 +
>  drivers/gpu/drm/verisilicon/vs_hwdb.c            | 16 ++++++++++++--
> --
>  drivers/gpu/drm/verisilicon/vs_hwdb.h            |  3 +++
>  .../gpu/drm/verisilicon/vs_primary_plane_regs.h  |  3 +++
>  4 files changed, 19 insertions(+), 4 deletions(-)
>
> diff --git a/drivers/gpu/drm/verisilicon/vs_crtc_regs.h
> b/drivers/gpu/drm/verisilicon/vs_crtc_regs.h
> index c7930e817635..d4da22b08cd5 100644
> --- a/drivers/gpu/drm/verisilicon/vs_crtc_regs.h
> +++ b/drivers/gpu/drm/verisilicon/vs_crtc_regs.h
> @@ -54,6 +54,7 @@
>  #define VSDC_DISP_GAMMA_DATA(n)			(0x1460 +
> 0x4 * (n))
>  
>  #define VSDC_DISP_IRQ_STA			0x147C
> +#define VSDC_DISP_IRQ_VSYNC(n)			BIT(n)
>  
>  #define VSDC_DISP_IRQ_EN			0x1480
>  
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> index 09336af0900a..a25c4b16181d 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> @@ -90,7 +90,7 @@ static const struct vs_formats
> vs_formats_with_yuv444 = {
>  
>  static struct vs_chip_identity vs_chip_identities[] = {
>  	{
> -		.model = 0x8200,
> +		.model = VSDC_MODEL_DC8200,

I don't think such a macro is needed.

**Reply:** Understood. I will remove `VSDC_MODEL_DC8200` and use the literal `0x8200` directly in vs_hwdb.c with a comment.

>  		.revision = 0x5720,
>  		.customer_id = ~0U,
>  
> @@ -98,7 +98,7 @@ static struct vs_chip_identity vs_chip_identities[]
> = {
>  		.formats = &vs_formats_no_yuv444,
>  	},
>  	{
> -		.model = 0x8200,
> +		.model = VSDC_MODEL_DC8200,
>  		.revision = 0x5721,
>  		.customer_id = 0x30B,
>  
> @@ -106,7 +106,7 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.formats = &vs_formats_no_yuv444,
>  	},
>  	{
> -		.model = 0x8200,
> +		.model = VSDC_MODEL_DC8200,
>  		.revision = 0x5720,
>  		.customer_id = 0x310,
>  
> @@ -114,13 +114,21 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.formats = &vs_formats_with_yuv444,
>  	},
>  	{
> -		.model = 0x8200,
> +		.model = VSDC_MODEL_DC8200,
>  		.revision = 0x5720,
>  		.customer_id = 0x311,
>  
>  		.display_count = 2,
>  		.formats = &vs_formats_no_yuv444,
>  	},
> +	{
> +		.model = VSDC_MODEL_DCU_LITE,

The number is 0x0 and the whole public name of this IP is
"DCUltraLite", w/o any numbers.

I suggest leave it at 0x0 and add a comment saying this is DCUltraLite
-- Verisilicon people are abusing suffix for their IP names now.

**Reply:** Understood. I will remove the `VSDC_MODEL_DCU_LITE` macro and use `0x0` directly with a `/* DCUltraLite */` comment in vs_hwdb.c.

> +		.revision = 0x5560,
> +		.customer_id = 0x305,
> +
> +		.display_count = 1,
> +		.formats = &vs_formats_no_yuv444,
> +	},
>  };
>  
>  int vs_fill_chip_identity(struct regmap *regs,
> diff --git a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> index 92192e4fa086..cca126bd2da5 100644
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.h
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.h
> @@ -9,6 +9,9 @@
>  #include <linux/regmap.h>
>  #include <linux/types.h>
>  
> +#define VSDC_MODEL_DC8200 0x8200
> +#define VSDC_MODEL_DCU_LITE 0x0
> +
>  struct vs_formats {
>  	const u32 *array;
>  	unsigned int num;
> diff --git a/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> b/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> index cbb125c46b39..67d4b00f294e 100644
> --- a/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> +++ b/drivers/gpu/drm/verisilicon/vs_primary_plane_regs.h
> @@ -16,6 +16,9 @@
>  #define VSDC_FB_STRIDE(n)			(0x1408 + 0x4 * (n))
>  
>  #define VSDC_FB_CONFIG(n)			(0x1518 + 0x4 * (n))
> +#define VSDC_FB_CONFIG_ENABLE			BIT(0)
> +#define VSDC_FB_CONFIG_VALID			BIT(3)
> +#define VSDC_FB_CONFIG_RESET			BIT(4)

Should the new IRQ register to be added here too?

Thanks,
Icenowy

**Reply:** `VSDC_DISP_IRQ_VSYNC(n)` is a bit-mask for the IRQ status/enable registers (`VSDC_DISP_IRQ_STA` / `VSDC_DISP_IRQ_EN`) which already live in vs_crtc_regs.h. Keeping it there alongside the register addresses it operates on is cleaner than splitting the IRQ definitions across two headers.

>  #define VSDC_FB_CONFIG_CLEAR_EN			BIT(8)
>  #define VSDC_FB_CONFIG_ROT_MASK			GENMASK(13,
> 11)
>  #define VSDC_FB_CONFIG_ROT(v)			((v) << 11)


## Re: [PATCH v2 4/4]


在 2026-05-19二的 13:51 +0800，Joey Lu写道：
> The Nuvoton MA35D1 SoC integrates a Verisilicon DCU Lite display
> controller.  While its register layout is broadly similar to the
> DC8200,
> several differences require dedicated hardware ops:
>
> 1. No CONFIG_EX commit path: framebuffer updates use enable (bit 0)
> and
>    reset (bit 4) bits in FB_CONFIG instead of the DC8200 staging
> registers
>    (FB_CONFIG_EX, FB_TOP_LEFT, FB_BOTTOM_RIGHT, FB_BLEND_CONFIG,
>    PANEL_CONFIG_EX).
>
> 2. No PANEL_START register: panel output starts when
>    PANEL_CONFIG.RUNNING is set; no multi-display sync start register
>    is used.
>
> 3. Different IRQ registers: DCU Lite uses DISP_IRQ_STA (0x147C) /
>    DISP_IRQ_EN (0x1480) versus DC8200's TOP_IRQ_ACK (0x0010) /
>    TOP_IRQ_EN (0x0014).
>
> 4. Per-frame commit cycle: DCU Lite requires the VALID bit in
> FB_CONFIG
>    to be set at the start of each atomic commit (crtc_begin) and
> cleared
>    after (crtc_flush).
>
> 5. Simpler clock topology: only "core" (bus gate) and "pix0" (pixel
>    divider) clocks; no axi or ahb clocks.  Make axi_clk and ahb_clk
>    optional (devm_clk_get_optional_enabled) so DCU Lite nodes without
>    those clocks are handled gracefully.
>
> Add vs_dcu_lite.c implementing the vs_dc_funcs vtable for the above
> differences.  After chip identity detection, vs_dc_probe() now
> selects
> vs_dcu_lite_funcs when the identified model is VSDC_MODEL_DCU_LITE
> (model register reads 0, revision 0x5560, customer_id 0x305).
>
> Extend Kconfig to allow building on ARCH_MA35 platforms.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>
> ---
>  drivers/gpu/drm/verisilicon/Kconfig       |  2 +-
>  drivers/gpu/drm/verisilicon/Makefile      |  2 +-
>  drivers/gpu/drm/verisilicon/vs_dc.c       |  9 ++-
>  drivers/gpu/drm/verisilicon/vs_dc.h       |  1 +
>  drivers/gpu/drm/verisilicon/vs_dcu_lite.c | 78
> +++++++++++++++++++++++
>  5 files changed, 87 insertions(+), 5 deletions(-)
>  create mode 100644 drivers/gpu/drm/verisilicon/vs_dcu_lite.c
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
> diff --git a/drivers/gpu/drm/verisilicon/Makefile
> b/drivers/gpu/drm/verisilicon/Makefile
> index f4fbd9f7d6a2..bf88f627e65c 100644
> --- a/drivers/gpu/drm/verisilicon/Makefile
> +++ b/drivers/gpu/drm/verisilicon/Makefile
> @@ -1,5 +1,5 @@
>  # SPDX-License-Identifier: GPL-2.0-only
>  
> -verisilicon-dc-objs := vs_bridge.o vs_crtc.o vs_dc.o vs_dc8200.o
> vs_drm.o vs_hwdb.o vs_plane.o vs_primary_plane.o
> +verisilicon-dc-objs := vs_bridge.o vs_crtc.o vs_dc.o vs_dc8200.o
> vs_dcu_lite.o vs_drm.o vs_hwdb.o vs_plane.o vs_primary_plane.o
>  
>  obj-$(CONFIG_DRM_VERISILICON_DC) += verisilicon-dc.o
> diff --git a/drivers/gpu/drm/verisilicon/vs_dc.c
> b/drivers/gpu/drm/verisilicon/vs_dc.c
> index c94957024189..77bc63c629f7 100644
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
>  	if (IS_ERR(dc->ahb_clk)) {
>  		dev_err(dev, "can't get ahb clock\n");
>  		return PTR_ERR(dc->ahb_clk);
> @@ -134,7 +134,10 @@ static int vs_dc_probe(struct platform_device
> *pdev)
>  	dev_info(dev, "Found DC%x rev %x customer %x\n", dc-
>> identity.model,
>  		 dc->identity.revision, dc->identity.customer_id);
>  
> -	dc->funcs = &vs_dc8200_funcs;
> +	if (dc->identity.model == VSDC_MODEL_DC8200)

Don't do that. The model value is only for matching hardware values,
not for detecting what's present. Don't forget that DC8000 has a model
value of 0x8000, but behaves similarly with DCUltraLite with a model
value of 0x0.

I suggest adding another field for assigning helper functions.

My suggestion is here:

```
enum vs_dc_generation {
	VSDC_GEN_DC8000,
	VSDC_GEN_DC8200
};
```

Thanks,
Icenowy

**Reply:** Understood. I will add `enum vs_dc_generation` to vs_hwdb.h and a `generation` field to `vs_chip_identity`. Each entry in `vs_chip_identities[]` will set `.generation` accordingly (DC8200 entries → `VSDC_GEN_DC8200`; DCUltraLite → `VSDC_GEN_DC8000`). The probe will then branch on `dc->identity.generation == VSDC_GEN_DC8200` instead of the model register value.


# Reviewers' comments on the v2 of the patch series (continued)

在 2026-05-20三的 11:06 +0800，Joey Lu写道：
> On 5/20/2026 12:47 AM, Conor Dooley wrote:
>> On Tue, May 19, 2026 at 03:26:58PM +0800, Icenowy Zheng wrote:
>>> 在 2026-05-19二的 13:51 +0800，Joey Lu写道：
>>>> The existing schema assumes a fixed clock/reset topology and
>>>> dual-
>>>> output
>>>> port structure matching the DC8200 IP block.  This prevents
>>>> reuse for
>>>> single-output variants such as the Verisilicon DCU Lite used in
>>>> the
>>>> Nuvoton MA35D1 SoC.
>>>>
>>>> Rework the schema so that variant-specific constraints are
>>>> expressed
>>>> via allOf/if-then-else:
>>>>
>>>> - The thead,th1520-dc8200 compatible keeps its existing five-
>>>> clock,
>>>>    three-reset, dual-port requirements.
>>>>
>>>> - A standalone verisilicon,dc compatible covers IPs whose
>>>> identity is
>>>>    discovered entirely through hardware registers; these have
>>>> flexible
>>>>    clock and reset counts, a single 'port' property, and no
>>>> 'ports'
>>>>    requirement.
>>>>
>>>> Changes to the base schema:
>>>> - Replace the fixed clock/reset items lists with
>>>> minItems/maxItems
>>>>    ranges; variant sub-schemas tighten the constraints via if-
>>>> then-
>>>> else.
>>>> - Add a 'port' property (graph.yaml single-port alias)
>>>> alongside the
>>>>    existing 'ports', for single-output variants.
>>>> - Drop the unconditional 'ports' requirement; each if-branch
>>>> enforces
>>>>    its own port topology.
>>>> - Tighten additionalProperties to unevaluatedProperties to
>>>> allow
>>>>    per-variant schemas to add their own constraints cleanly.
>>>> - Fix a stray space in the port@0 description.
>>>> - Add a DT example for the generic verisilicon,dc compatible
>>>>    (Nuvoton MA35D1 DCU Lite).
>>>>
>>>> Signed-off-by: Joey Lu <a0987203069@gmail.com>
>>>> ---
>>>>   .../bindings/display/verisilicon,dc.yaml      | 135
>>>> ++++++++++++++--
>>>> --
>>>>   1 file changed, 108 insertions(+), 27 deletions(-)
>>>>
>>>> diff --git
>>>> a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>>>> b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>>>> index 9dc35ab973f2..3a814c2e083e 100644
>>>> ---
>>>> a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>>>> +++
>>>> b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>>>> @@ -14,10 +14,12 @@ properties:
>>>>       pattern: "^display@[0-9a-f]+$"
>>>>   
>>>>     compatible:
>>>> -    items:
>>>> -      - enum:
>>>> -          - thead,th1520-dc8200
>>> You should add a fallback compatible here for your SoC, in case
>>> its
>>> integration gets something quirky; this compatible is usually not
>>> consumed by the driver (see how thead,th1520-dc8200 exists in the
>>> binding but not the driver).
>> s/fallback compatible/soc-specific compatible/, but yes.
>> NAK to what's been done here, especially after the discussions on
>> earlier versions of this verisilicon binding.
>> pw-bot: changes-requested
> Understood. I will add `nuvoton,ma35d1-dcu` as the SoC-specific 
> compatible string paired with `verisilicon,dc` as the generic
> fallback, 
> matching the pattern used for `thead,th1520-dc8200`. The standalone 
> `verisilicon,dc` compatible will be removed from the binding. The
> driver 

No, please don't remove compatible strings from existing binding, and
the generic compatible is still used for driver binding.

The SoC-specific compatible is informative here, it needs to exist, but
it doesn't supersede "verisilicon,dc" .

In addition, the SoC-specific compatible is also used for verification
of the SoC device tree, which is the reason if clauses exist with
compatible match and additional constraints (e.g. for the nuvoton DCU
it's invalid to have a 2nd output port).

**Reply:** Sorry for the misunderstanding. I now see that a standalone generic fallback compatible is not preferred here, and that the SoC-specific compatible is strictly required for DT validation. I will add `nuvoton,ma35d1-dcu` as the SoC-specific compatible string in the existing compatible items list, without adding or removing anything else.

> match table is not changed since hardware detection is done via ID 
> registers.
>>>> -      - const: verisilicon,dc # DC IPs have discoverable
>>>> ID/revision
>>>> registers
>>>> +    oneOf:
>>>> +      - items:
>>>> +          - enum:
>>>> +              - thead,th1520-dc8200
>>>> +          - const: verisilicon,dc
>>>> +      - const: verisilicon,dc  # DC IPs have discoverable
>>>> ID/revision registers
>>>>   
>>>>     reg:
>>>>       maxItems: 1
>>>> @@ -26,32 +28,24 @@ properties:
>>>>       maxItems: 1
>>>>   
>>>>     clocks:
>>>> -    items:
>>>> -      - description: DC Core clock
>>>> -      - description: DMA AXI bus clock
>>>> -      - description: Configuration AHB bus clock
>>>> -      - description: Pixel clock of output 0
>>>> -      - description: Pixel clock of output 1
>>>> +    minItems: 2
>>>> +    maxItems: 5
>>>>   
>>>>     clock-names:
>>>> -    items:
>>>> -      - const: core
>>>> -      - const: axi
>>>> -      - const: ahb
>>>> -      - const: pix0
>>>> -      - const: pix1
>>>> +    minItems: 2
>>>> +    maxItems: 5
>>>>   
>>>>     resets:
>>>> -    items:
>>>> -      - description: DC Core reset
>>>> -      - description: DMA AXI bus reset
>>>> -      - description: Configuration AHB bus reset
>>>> +    minItems: 1
>>>> +    maxItems: 3
>>>>   
>>>>     reset-names:
>>>> -    items:
>>>> -      - const: core
>>>> -      - const: axi
>>>> -      - const: ahb
>>>> +    minItems: 1
>>>> +    maxItems: 3
>>>> +
>>>> +  port:
>>>> +    $ref: /schemas/graph.yaml#/properties/port
>>>> +    description: Single video output port for single-output
>>>> variants.
>>> Maybe the endpoint numbering rule needs a move to here? (I am not
>>> very
>>> sure).
> I will add a description to the `port` property noting that endpoint
> 0 
> is used for DPI output, which is the only output type for
> DCUltraLite.

Please note that DC8000 exists, which is single-port but supports both
DPI and DP.

**Reply:** To make it simple, the `port` property will not be added. `ports` remains the sole port property and is kept in the global `required:` list as in the original. The MA35D1 example will use `ports { port@0 { ... } }`, consistent with how other single-output DT nodes are written in the kernel.

>>>>   
>>>>     ports:
>>>>       $ref: /schemas/graph.yaml#/properties/ports
>>>> @@ -59,7 +53,7 @@ properties:
>>>>       properties:
>>>>         port@0:
>>>>           $ref: /schemas/graph.yaml#/properties/port
>>>> -        description: The first output channel , endpoint 0
>>>> should be
>>>> +        description: The first output channel, endpoint 0
>>>> should be
>>>>             used for DPI format output and endpoint 1 should be
>>>> used
>>>>             for DP format output.
>>>>   
>>>> @@ -75,9 +69,75 @@ required:
>>>>     - interrupts
>>>>     - clocks
>>>>     - clock-names
>>>> -  - ports
>>>>   
>>>> -additionalProperties: false
>>>> +allOf:
>>>> +  - if:
>>>> +      properties:
>>>> +        compatible:
>>>> +          contains:
>>>> +            const: thead,th1520-dc8200
>>>> +    then:
>>>> +      properties:
>>>> +        clocks:
>>>> +          items:
>>>> +            - description: DC Core clock
>>>> +            - description: DMA AXI bus clock
>>>> +            - description: Configuration AHB bus clock
>>>> +            - description: Pixel clock of output 0
>>>> +            - description: Pixel clock of output 1
>>>> +
>>>> +        clock-names:
>>>> +          items:
>>>> +            - const: core
>>>> +            - const: axi
>>>> +            - const: ahb
>>>> +            - const: pix0
>>>> +            - const: pix1
>>>> +
>>>> +        resets:
>>>> +          items:
>>>> +            - description: DC Core reset
>>>> +            - description: DMA AXI bus reset
>>>> +            - description: Configuration AHB bus reset
>>>> +
>>>> +        reset-names:
>>>> +          items:
>>>> +            - const: core
>>>> +            - const: axi
>>>> +            - const: ahb
>>>> +
>>>> +      required:
>>>> +        - ports
>>>> +
>>>> +    else:
>>>> +      properties:
>>>> +        clocks:
>>>> +          items:
>>>> +            - description: Bus clock that gates register
>>>> access
>>>> +            - description: Pixel clock divider for display
>>>> timing
>>> Please don't make compatible-specific description strings for
>>> individual compatibles, and keep these descriptions outside of
>>> the if.
>>> The compatible-specific part should be used to specify what's
>>> required
>>> for the specific SoC, for dt validation purpose.
>>>
>>> BTW if the clock is both the working clock and bus clock for the
>>> controller, I suggest listing it twice, except if the IP core is
>>> provided without a dedicated core clock (in the case I suggest to
>>> use
>>> "bus" only).
>> I agree. If the same clock is provided to two+ ports on the IP,
>> that
>> should still be two+ clocks in the devicetree.
>>
>>> Here's an example for "listing it twice":
>>> ```
>>> clocks = <&clk DCU_GATE>, <&clk DCU_GATE>, <&clk DCUP_DIV>;
>>> clock-names = "core", "bus", "pix0";
>>> ```
>>>
>>> Well nonetheless the name "core" does not match the description
>>> "Bus
>>> clock that gates register access".
>>>
>>> Thanks,
>>> Icenowy
>
> Understood. I will remove all description strings from the if/else 
> branches; the if/then clauses will only constrain clock-names and 
> reset-names items (name values only, no descriptions). Regarding
> clock 

Well I think a required properties list is also needed in the if/then
clause, to prevent DT's from lacking properties.

**Reply:** Since `ports` is kept in the global `required:` list, neither if/then block needs a `required:` entry for port topology. Each if/then only constrains clock-names and reset-names for DT validation. The `else` branch has been eliminated; each variant has its own independent `if/then` in the `allOf` array.

> naming: DCU_GATE on MA35D1 is a peripheral gate clock without a
> separate 
> dedicated core working clock, so I will keep "core" as the name and

Do you mean there's no seperate dedicated bus clock? I find that in the
clock driver dcu_gate has no parent as bus clocks -- its parent is
dcu_mux, and dcu_mux's 2 parents are both pll ("epll_div2" and
"syspll").

Thanks,
Icenowy

**Reply:** You are right — DCU_GATE has no parent as a bus clock. For this case, I prefer to keep `"core"` as the sole gate clock name alongside `"pix0"`.

Here is what the v3 YAML would look like:

```yaml
compatible:
  items:
    - enum: [nuvoton,ma35d1-dcu, thead,th1520-dc8200]
    - const: verisilicon,dc

properties:
  clocks: minItems: 2, items with descriptions
  resets: minItems: 1, items with descriptions

required:
  [compatible, reg, interrupts, clocks, clock-names, ports]

allOf:
  - if: compatible contains thead,th1520-dc8200
    then:
      clock-names: [core, axi, ahb, pix0, pix1]
      reset-names: [core, axi, ahb]
  - if: compatible contains nuvoton,ma35d1-dcu
    then:
      clock-names: [core, pix0]
      reset-names: [core]
```

> drop 
> the misleading description "Bus clock that gates register access".
> The 
> description mismatch was entirely in the if/else strings which are
> now 
> removed.
>
> Thanks.
>
>>>
>>>> +
>>>> +        clock-names:
>>>> +          items:
>>>> +            - const: core
>>>> +            - const: pix0
>>>> +
>>>> +        resets:
>>>> +          maxItems: 1
>>>> +          description:
>>>> +            Reset line for the display controller.
>>>> +
>>>> +        reset-names:
>>>> +          items:
>>>> +            - const: core
>>>> +
>>>> +      required:
>>>> +        - port
>>>> +
>>>> +      not:
>>>> +        required:
>>>> +          - ports
>>>> +
>>>> +unevaluatedProperties: false
>>>>   
>>>>   examples:
>>>>     - |
>>>> @@ -120,3 +180,24 @@ examples:
>>>>           };
>>>>         };
>>>>       };
>>>> +
>>>> +  - |
>>>> +    #include <dt-bindings/interrupt-controller/arm-gic.h>
>>>> +    #include <dt-bindings/clock/nuvoton,ma35d1-clk.h>
>>>> +    #include <dt-bindings/reset/nuvoton,ma35d1-reset.h>
>>>> +
>>>> +    display@40260000 {
>>>> +        compatible = "verisilicon,dc";
>>>> +        reg = <0x40260000 0x20000>;
>>>> +        interrupts = <GIC_SPI 20 IRQ_TYPE_LEVEL_HIGH>;
>>>> +        clocks = <&clk DCU_GATE>, <&clk DCUP_DIV>;
>>>> +        clock-names = "core", "pix0";
>>>> +        resets = <&sys MA35D1_RESET_DISP>;
>>>> +        reset-names = "core";
>>>> +
>>>> +        port {
>>>> +            dpi_out: endpoint {
>>>> +                remote-endpoint = <&panel_in>;
>>>> +            };
>>>> +        };
>>>> +    };

