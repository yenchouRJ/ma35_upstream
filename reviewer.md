# Background
This drm dirver patch series is according to the our last patch at
/home/joeylu/Documents/upstream2026/drm/kernel branch upstream-v5

Reviewer and maintainer are discuess my patch and give me some suggestions, I need to reply them and reflect their suggestions in my patch.

# First part: yaml

## Re: [PATCH v5 1/7] dt-bindings: display: verisilicon,dc: generalize for single-output variants
> The verisilicon,dc binding was originally written for the T-Head TH1520
> SoC carrying a DC8200, and hard-codes five clocks, three resets and two
> output ports.
>
> Add the Nuvoton MA35D1 DCUltraLite (nuvoton,ma35d1-dcu) to the binding.
> The DCUltraLite uses only two clocks (core, pix0) and one reset (core),
> with a single output port.
>
> Use allOf/if blocks to express per-variant constraints rather than
> hard-coding the DC8200 topology at the top level.  Each compatible's
> block constrains the clock and reset item counts; the nuvoton block
> additionally overrides clock-names to the two names it actually uses.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>
> ---
>  .../bindings/display/verisilicon,dc.yaml      | 57 +++++++++++++++++++
>  1 file changed, 57 insertions(+)
>
> diff --git a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> index 9dc35ab973f2..1e751f3c7ce8 100644
> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
> @@ -17,6 +17,7 @@ properties:
>      items:
>        - enum:
>            - thead,th1520-dc8200
> +          - nuvoton,ma35d1-dcu
>        - const: verisilicon,dc # DC IPs have discoverable ID/revision registers
>  
>    reg:
> @@ -77,6 +78,62 @@ required:
>    - clock-names
>    - ports
>  
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
> +          minItems: 5
> +          maxItems: 5

All the maxItems here repeat the maximum constraint and do nothing.

Since you didn't change the minimum constraint at the top level, your
minItems also do nothing.

> +
> +        resets:
> +          minItems: 3
> +          maxItems: 3
> +
> +        reset-names:
> +          minItems: 3
> +          maxItems: 3
> +
> +      required:
> +        - resets
> +        - reset-names

Both conditional sections have this, but the original binding doesn't
require these for the thead device. This is a functional change
therefore and shouldn't be in a patch calling itself "generalise for
single ended variants".

FWIW, adding your new compatible shouldn't really be in a patch with
that subject either, it really should say "add support for nuvoton
ma35d1" or something.

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

Anything that updates the minimum constraint should be done at the top
level of this schema. The conditional section should then tighten the
constraint, in this case that means only having maxItems.

> +          maxItems: 2
> +
> +        clock-names:
> +          items:
> +            - const: core
> +            - const: pix0

Does this even work when the top level schema thinks clock 2 should be
called axi?

> +
> +        resets:
> +          minItems: 1
> +          maxItems: 1
> +
> +        reset-names:
> +          items:
> +            - const: core

This is just maxItems: 1.

pw-bot: changes-requested

Thanks,
Conor.

> +
> +      required:
> +        - resets
> +        - reset-names
> +
>  additionalProperties: false


## Re: [PATCH v5 1/7] dt-bindings: display: verisilicon,dc: generalize for single-output variants (followup)
>> diff --git a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> index 9dc35ab973f2..1e751f3c7ce8 100644
>> --- a/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> +++ b/Documentation/devicetree/bindings/display/verisilicon,dc.yaml
>> @@ -17,6 +17,7 @@ properties:
>>      items:
>>        - enum:
>>            - thead,th1520-dc8200
>> +          - nuvoton,ma35d1-dcu
>>        - const: verisilicon,dc # DC IPs have discoverable ID/revision registers
>>  
>>    reg:
>> @@ -77,6 +78,62 @@ required:
>>    - clock-names
>>    - ports
>>  
>> +allOf:
>> +  - if:
>> +      properties:
>> +        compatible:
>> +          contains:
>> +            const: thead,th1520-dc8200
>> +    then:
>> +      properties:
>> +        clocks:
>> +          minItems: 5
>> +          maxItems: 5
>> +
>> +        clock-names:
>> +          minItems: 5
>> +          maxItems: 5
>
> All the maxItems here repeat the maximum constraint and do nothing.
>
> Since you didn't change the minimum constraint at the top level, your
> minItems also do nothing.
>
>> +
>> +        resets:
>> +          minItems: 3
>> +          maxItems: 3
>> +
>> +        reset-names:
>> +          minItems: 3
>> +          maxItems: 3
>> +
>> +      required:
>> +        - resets
>> +        - reset-names
>
> Both conditional sections have this, but the original binding doesn't
> require these for the thead device. This is a functional change
> therefore and shouldn't be in a patch calling itself "generalise for
> single ended variants".
>
> FWIW, adding your new compatible shouldn't really be in a patch with
> that subject either, it really should say "add support for nuvoton
> ma35d1" or something.
>
>> +
>> +  - if:
>> +      properties:
>> +        compatible:
>> +          contains:
>> +            const: nuvoton,ma35d1-dcu
>> +    then:
>> +      properties:
>> +        clocks:
>> +          minItems: 2
>
> Anything that updates the minimum constraint should be done at the top
> level of this schema. The conditional section should then tighten the
> constraint, in this case that means only having maxItems.
>
>> +          maxItems: 2
>> +
>> +        clock-names:
>> +          items:
>> +            - const: core
>> +            - const: pix0
>
> Does this even work when the top level schema thinks clock 2 should be
> called axi?

Additionally here, only have core and pix0 seems like it might be an
oversimplification. I doubt removing the second output port means that
the axi and ahb clocks are no longer needed.
Is it the case that your device supplies the same clock to core, ahb and
axi? If so, then you should fill those clocks in in your devicetree and
this can just constrain the number of clocks/clock-names to 4.

>
>> +
>> +        resets:
>> +          minItems: 1
>> +          maxItems: 1
>> +
>> +        reset-names:
>> +          items:
>> +            - const: core
>
> This is just maxItems: 1.
>
> pw-bot: changes-requested
>
> Thanks,
> Conor.


## Reply #1
> Additionally here, only have core and pix0 seems like it might be an
> oversimplification. I doubt removing the second output port means
> that
> the axi and ahb clocks are no longer needed.
> Is it the case that your device supplies the same clock to core, ahb
> and
> axi? If so, then you should fill those clocks in in your devicetree
> and
> this can just constrain the number of clocks/clock-names to 4.

The clock controller of that SoC is quite weird -- it has only a single
gate bit, but controlling 3 clock gates. All core, ahb and axi clocks
have gates controlled by this single bit, so it's why currently it's
modelled as only core clock supplied.

Well it might be worthful to supply the bus clock before the gate as
ahb/axi, especially axi, because both the AXI clock and the core clock
constraints the maximum pixel clock.

Thanks,
Icenowy

## Reply #2
> The clock controller of that SoC is quite weird -- it has only a single
> gate bit, but controlling 3 clock gates. All core, ahb and axi clocks
> have gates controlled by this single bit, so it's why currently it's
> modelled as only core clock supplied.

Yeah, then what's in the binding is definitely wrong.
Even if the same clock was provided to all clock inputs in the IP, all
individual clock should be listed in the devicetree - although it will
look a little silly to see clocks = <&foo 2>, <&foo 2>, <&foo 2>, <&foo 2>;
In this case, 3 clocks controlled by 1 gate bit is an implementation detail
of the SoC's clocking hardware, and not relevant to how the dc instance
should be described.

> Well it might be worthful to supply the bus clock before the gate as
> ahb/axi, especially axi, because both the AXI clock and the core clock
> constraints the maximum pixel clock.

Right. And looking at patch 4/7, and the wording:
| The Nuvoton MA35D1 SoC integrates a DCUltraLite display controller whose
| AXI and AHB bus clocks share a single gate enable bit with the display
| core clock, so the clock driver does not expose them separately. This
| patch makes the axi and ahb clocks optional in the probe.

It sounds like there's probably some issues with how things are modelled
clock wise in this device, unless this is not an accurate statement and
there's actually one clock provided to all three inputs. If they're
distinct clocks, with different rates, only having one exposed has a lot
of potential to be problematic!

## Reply #3
>
> It sounds like there's probably some issues with how things are
> modelled
> clock wise in this device, unless this is not an accurate statement
> and
> there's actually one clock provided to all three inputs. If they're
> distinct clocks, with different rates, only having one exposed has a
> lot
> of potential to be problematic!

Yes, I agree with this, they're different clocks according to the
manual.

I added the clk people to the CC list in a reply of the previous
revision, but they didn't react yet. I don't know how to represent
multiple clock gates sharing a single control bit in the clock
framework...

Maybe just supplying the ungated AXI/AHB clocks here, and let the core
clock manage the gate?

Thanks,
Icenowy

## Reply #4
> Yes, I agree with this, they're different clocks according to the
> manual.
>
> I added the clk people to the CC list in a reply of the previous
> revision, but they didn't react yet. I don't know how to represent
> multiple clock gates sharing a single control bit in the clock
> framework...

Yeah, I have absolutely no idea. Maybe it requires custom refcounting?
Surely this cannot be the only device that does something like this
though.

> Maybe just supplying the ungated AXI/AHB clocks here, and let the core
> clock manage the gate?

I guess, but that seems incorrect and would require commentary about why
it's being done. Feel like they (the missing axi/ahb clocks) should be
added to the clock driver and binding, and any special workarounds done
there.
Of course letting the core clock manage the gate and making the enable
method for the gated AXI/AHB clocks be a NOP is one way of handling it
in the clock driver. Still a bit of a hack compared to refcounting it,
but it makes me happier to have the correct clock tree modelled in DT.

## Reply #5
> Both conditional sections have this, but the original binding doesn't
> require these for the thead device. This is a functional change
> therefore and shouldn't be in a patch calling itself "generalise for
> single ended variants".

Well yes they're required.

Should I send a patch adding the `thead,th1520-dc8200` part of the
schema?

>
> FWIW, adding your new compatible shouldn't really be in a patch with
> that subject either, it really should say "add support for nuvoton
> ma35d1" or something.
>
>> +
>> +  - if:
>> +      properties:
>> +        compatible:
>> +          contains:
>> +            const: nuvoton,ma35d1-dcu
>> +    then:
>> +      properties:
>> +        clocks:
>> +          minItems: 2
>
> Anything that updates the minimum constraint should be done at the
> top
> level of this schema. The conditional section should then tighten the
> constraint, in this case that means only having maxItems.
>
>> +          maxItems: 2
>> +
>> +        clock-names:
>> +          items:
>> +            - const: core
>> +            - const: pix0
>
> Does this even work when the top level schema thinks clock 2 should
> be
> called axi?
>
>> +
>> +        resets:
>> +          minItems: 1
>> +          maxItems: 1
>> +
>> +        reset-names:
>> +          items:
>> +            - const: core
>
> This is just maxItems: 1.

Well the implicit rules of DT binding schemas are quite weird...

Thanks,
Icenowy

## Reply #6
> Should I send a patch adding the `thead,th1520-dc8200` part of the
> schema?

If you mean the code above, no. Adding a conditional section when
there's only that compatible doesn't make sense.

What you could do is just add it at the top level though, which would
also benefit this patch since it'd not have to be conditionally added
for the new nuvoton device.
Just note in your commit message about what the ABI impact of the change
to required properties is (effectively nothing because it's optional in
the driver and the only user has the properties).

>
> Well the implicit rules of DT binding schemas are quite weird...

I don't think it is that strange, as the binding has
  reset-names:
    items:
      - const: core
      - const: axi
      - const: ahb
so just constraining to one item is the simplest way to do this without
duplication.

## Reply # 7
> If you mean the code above, no. Adding a conditional section when
> there's only that compatible doesn't make sense.
>
> What you could do is just add it at the top level though, which would
> also benefit this patch since it'd not have to be conditionally added
> for the new nuvoton device.
> Just note in your commit message about what the ABI impact of the
> change
> to required properties is (effectively nothing because it's optional
> in
> the driver and the only user has the properties).

Okay, I will craft such a patch and send it.

> I don't think it is that strange, as the binding has
>   reset-names:
>     items:
>       - const: core
>       - const: axi
>       - const: ahb

Ah does the list constraint the order of items? If it constrains the
order, it partly breaks the intention of having names; if it does not
constrain the order, it needs to be clarified that the required 1 reset
is core instead of the other two.

Thanks,
Icenowy

## Reply #8
> If you mean the code above, no. Adding a conditional section when
> there's only that compatible doesn't make sense.
>
> What you could do is just add it at the top level though, which would
> also benefit this patch since it'd not have to be conditionally added
> for the new nuvoton device.
> Just note in your commit message about what the ABI impact of the
> change
> to required properties is (effectively nothing because it's optional
> in
> the driver and the only user has the properties).

Okay, I will craft such a patch and send it.

>
> Ah does the list constraint the order of items? If it constrains the

It does, yes.
Alternatively, using an enum permits free ordering.

> order, it partly breaks the intention of having names; if it does not
> constrain the order, it needs to be clarified that the required 1 reset
> is core instead of the other two.

Given the discussion we're having on the clocks, I wonder if this is
also an oversimplification and the IP has three resets inputs hooked up
to one output of the reset controller (or 3 outputs controlled by one
bit..).

## Reply #9
>
> It does, yes.
> Alternatively, using an enum permits free ordering.

Ah in this case this should be converted to an enum, I think.

Should I send a patch for converting it?

Thanks,
Icenowy

## Reply #10
>
> Ah in this case this should be converted to an enum, I think.
>
> Should I send a patch for converting it?

Why do you think it should be an enum? We don't currently have any users
of this that only provide no core or no axi reset.


## Our reply

> Ah in this case this should be converted to an enum, I think.
>
> Should I send a patch for converting it?

Thank you all for the detailed review and discussion, it really helped
clarify the right approach.

Since I will supply all four clocks with the same phandle for core/axi/ahb,
and only one reset "core" for MA35D1, the ordering constraint in the
`items` list is not a problem, "core" is already the first entry. There
is no need to convert to an enum.

Regarding the clock situation for the MA35D1: I agree with supplying all
four clocks (core, axi, ahb, pix0) in the devicetree, even though the
MA35D1 clock controller gates core/axi/ahb with a single bit. The DT will
use the same clock phandle for core, axi, and ahb:

  clocks = <&clk X>, <&clk X>, <&clk X>, <&pix_clk Y>;
  clock-names = "core", "axi", "ahb", "pix0";

The DRM driver only calls clk_prepare_enable() on core/axi/ahb and never
calls clk_set_rate() on them, so the driver works correctly with this
approach. A proper clock driver rework to expose the real AXI/AHB rates
will be handled in a separate patch series.
I will also revert the change in patch 4/7 that made axi and ahb clocks
optional, since they will now always be provided in the devicetree.

Regarding moving `resets` and `reset-names` to the top-level `required:`,
I will wait for Icenowy's patch to land before sending v6 to avoid
duplicating the work.

In v6 I will update patch 1/7 with:
- Update the subject to "dt-bindings: display: verisilicon,dc: add
  support for nuvoton,ma35d1-dcu"
- Lower `clocks`/`clock-names` `minItems` to 4 at the top level
- Remove the `thead,th1520-dc8200` conditional block entirely
- Keep only the `nuvoton,ma35d1-dcu` conditional block, using only
  `maxItems: 4` for clocks/clock-names and `maxItems: 1` for
  resets/reset-names to tighten the top-level constraints

BR,
Joey


# Second part: driver

## Re: [PATCH v5 3/7] drm/verisilicon: introduce per-variant hardware ops table
> +static u32 vs_dc8200_irq_ack(struct vs_dc *dc)
> +{
> +	u32 hw_irqs, unified = 0;
> +	unsigned int i;
> +
> +	regmap_read(dc->regs, VSDC_TOP_IRQ_ACK, &hw_irqs);
> +
> +	for (i = 0; i < VSDC_MAX_OUTPUTS; i++) {
> +		if (hw_irqs & VSDC_TOP_IRQ_VSYNC(i))
> +			unified |= VSDC_IRQ_VSYNC(i);
> +	}

Maybe add a drm_WARN_ONCE for unknown hardware IRQ bit?

Well, with this addressed,

```
Reviewed-by: Icenowy Zheng <zhengxingda@iscas.ac.cn>
```

Thanks,
Icenowy

I will add a `drm_WARN_ONCE` after the IRQ translation loop in
`vs_dc8200_irq_ack` to catch any unknown hardware IRQ bits:

  drm_WARN_ONCE(&dc->drm_dev->base, hw_irqs & ~known,
                "Unknown hardware IRQ bits: %#x\n", hw_irqs & ~known);

## Re: [PATCH v5 4/7] drm/verisilicon: make axi and ahb clocks optional
> The Nuvoton MA35D1 SoC integrates a DCUltraLite display controller
> whose
> AXI and AHB bus clocks share a single gate enable bit with the
> display
> core clock, so the clock driver does not expose them separately. This
> patch makes the axi and ahb clocks optional in the probe.
>
> Signed-off-by: Joey Lu <a0987203069@gmail.com>

```
Reviewed-by: Icenowy Zheng <zhengxingda@iscas.ac.cn>
```

Thanks,
Icenowy

Thank you for the review. I will revert this patch entirely. As discussed
in the binding review, axi and ahb clocks will now always be supplied in
the devicetree using the same phandle as the core clock gate, so making
them optional in the driver is no longer needed.

## Re: [PATCH v5 5/7] drm/verisilicon: add DC8000 (DCUltraLite) display controller support
> +static u32 vs_dc8000_irq_ack(struct vs_dc *dc)
> +{
> +	u32 hw_irqs, unified = 0;
> +	unsigned int i;
> +
> +	regmap_read(dc->regs, VSDC_DISP_IRQ_STA, &hw_irqs);
> +
> +	for (i = 0; i < VSDC_MAX_OUTPUTS; i++) {
> +		if (hw_irqs & VSDC_DISP_IRQ_VSYNC(i))
> +			unified |= VSDC_IRQ_VSYNC(i);
> +	}

Maybe a warning for unknown IRQ bits should be added here.

Thanks,
Icenowy

I will add the same `drm_WARN_ONCE` pattern in `vs_dc8000_irq_ack` for
unknown `VSDC_DISP_IRQ_STA` bits.

## Re: [PATCH v5 6/7] drm/verisilicon: add DCUltraLite chip identity to HWDB
> --- a/drivers/gpu/drm/verisilicon/vs_hwdb.c
> +++ b/drivers/gpu/drm/verisilicon/vs_hwdb.c
> @@ -129,6 +129,16 @@ static struct vs_chip_identity
> vs_chip_identities[] = {
>  		.max_cursor_size = 64,
>  		.formats = &vs_formats_no_yuv444,
>  	},
> +	{
> +		.model = 0x0,		/* DCUltraLite */
> +		.revision = 0x5560,
> +		.customer_id = 0x305,
> +
> +		.generation = VSDC_GEN_DC8000,
> +		.display_count = 1,
> +		.max_cursor_size = 32,
> +		.formats = &vs_formats_no_yuv444,
> +	},

Checked against the MA35D1 manual, and it looks okay.

```
Reviewed-by: Icenowy Zheng <zhengxingda@iscas.ac.cn>
```

Thanks,
Icenowy

# Followup discussion

> This correctly models the hardware topology. Since all three names
> resolve to the same underlying clock node...

No, this doesn't correctly model the hardware topology -- this will
lead to clk_get_rate() return the rate of DC core clock when checking
the AXI clock rate, which is problematic because both clocks are
limiting the performance of the DC.

You are right. The current MA35D1 clock driver does not correctly reflect
the real AXI/AHB clock rates, and I think it needs a more comprehensive
review and rework. I will address this in a separate clock driver patch
series in the future.

For this DRM patch series, the verisilicon driver only calls
clk_prepare_enable() on the core, axi, and ahb clocks, it never calls
clk_set_rate() on them (only the pixel clock is rate-controlled). So the
DRM driver works correctly regardless of what rates the clock driver
currently reports for axi/ahb, and the DRM driver itself does not need
to change when the clock driver is later reworked.

> In v6 I will update patch 1/7 with:
> - Update the subject to "dt-bindings: display: verisilicon,dc: add
>    support for nuvoton,ma35d1-dcu"
> - Lower `clocks`/`clock-names` `minItems` to 4 at the top level
> - Remove the `thead,th1520-dc8200` conditional block entirely

I think this conditional block will still be needed, because it will
need to constrain the minItems to ensure all clocks / resets are
populated.

Thanks,
Icenowy

Correct. When the outer constraints are relaxed to deal with the new
device the conditional block for the th1520 becomes required. Or having
an else, but if all devices are likely to be different in terms of
configuration specific conditional blocks is better.

Understood. I will keep the `thead,th1520-dc8200` conditional block.

TODOs (updated):
- Update patch 1/7 subject to "dt-bindings: display: verisilicon,dc: add support for nuvoton,ma35d1-dcu"
- Wait for Icenowy's patch (resets/reset-names to top-level required) before sending v6
- Keep thead conditional block with minItems: 5 (clocks) and minItems: 3 (resets)
- Move clocks/clock-names minItems: 4 and resets/reset-names minItems: 1 to top level
- nuvoton conditional: maxItems: 4 (clocks) and maxItems: 1 (resets) only
- Use same phandle for core/axi/ahb in MA35D1 DT (clk driver rework deferred to separate series)
- Drop patch 4/7 (make axi/ahb optional), axi/ahb will always be provided in DT



