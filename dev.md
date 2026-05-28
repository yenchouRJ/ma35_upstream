## Project structure

- `./dts/` — DTS files, copied to `arch/arm64/boot/dts/nuvoton/`
- `./phy/nuvoton/` — USB PHY drivers
- `./usb/dwc2/` — DWC2 gadget driver (USB0, device-only)
- `./usb/host/` — reference only; host controllers now use upstream platform drivers
- `/home/joeylu/Documents/upstream2026/usbh/usb` — reference kernel

## Hardware background

### SYS registers (base 0x40460000)

**PWRONOTP (+0x04)**
- `USBP0ID` bit 16 (R/O): 0 = USB0 is device, 1 = USB0 is host

**USBPMISCR (+0x60)** — PHY control (n = 0 or 1, all PHY1 fields are +16 bits above PHY0)

| Field | PHY0 bits | PHY1 bits | Description |
|-------|-----------|-----------|-------------|
| POR | 0 | 16 | Power-On Reset (assert to reset) |
| SUSPEND | 1 | 17 | 1 = operational, 0 = suspended |
| COMN | 2 | 18 | Common block power-down |
| HSTCKSTB | 8 | 24 | 60 MHz host clock stable |
| CK12MSTB | 9 | 25 | 12 MHz clock stable |
| DEVCKSTB | 10 | — | 60 MHz device clock stable (PHY0 only) |
| RCALCODE | 15:12 | 31:28 | Resistor calibration trim (4-bit) |

Ready condition for host mode: SUSPEND + HSTCKSTB + CK12MSTB all set.
Ready condition for device mode: SUSPEND + DEVCKSTB set (PHY0 only).
Observation: (HSTCKSTB | CK12MSTB) and (DEVCKSTB) are mutually exclusive.

**MISCFCR0 (+0x70)**
- bit 12: `UHOVRCURH` — over-current detect polarity (0 = active-low, 1 = active-high); shared by both host ports

**MISCIER (+0x78)**
- bit 1: `USB0IDCHGIEN` — USB0 ID pin change interrupt enable
- bit 2: `VBUSCHGIEN` — USB0 VBUS valid change interrupt enable

**MISCISR (+0x7C)**
- bit 1: `USB0IDCHGIF` — USB0 ID pin change interrupt flag (clear by writing 1)
- bit 2: `VBUSCHGIF` — USB0 VBUS valid change interrupt flag

SYS interrupt: GIC_SPI 0

### USB0 hardware topology

```
  USB0 physical port
        │
        │  ID pin (PWRONOTP[16], R/O)
        │  0 = device,  1 = host
        │
   ┌────┴────────────────────────┐
   │  Hardware mux (automatic)   │
   └────┬────────────────┬───────┘
        │                │
   ┌────┴────┐     ┌─────┴──────┐
   │  DWC2   │     │ EHCI0/OHCI0│
   │ (device │     │  (host IP, │
   │  only)  │     │ 0x40140000)│
   └─────────┘     └────────────┘
```

**DWC2 is device-only in hardware.** `GHWCFG2.OTG_MODE` reports device-only,
so DWC2 forces `dr_mode = peripheral` and prints:
```
dwc2 40200000.usb: Configuration mismatch. dr_mode forced to device
```
DWC2's `usb-role-switch` / `dwc2_drd_role_sw_set()` cannot be relied on for
OTG switching — it only overrides GOTGCTL bits inside DWC2, which has no
effect on the physical mux or EHCI0.

At end of `dwc2_probe()` (peripheral mode), `dwc2_lowlevel_hw_disable()` is
called → `phy_power_off()` → `ma35_usb_phy_power_off()` → clock off. This
is **expected behavior**; the clock is re-enabled when a gadget function driver
loads via the UDC framework.

## Current state (tested, working)

### Driver architecture

```
  usb_phy0                 usb_hphy0              usb_hphy1
  (gadget PHY)             (host PHY0)            (host PHY1)
  phy-ma35d1-usb2.c        phy-ma35d1-otg.c       phy-ma35d1-otg.c
  clk: USBD_GATE           nuvoton,sys=<&sys 0>   nuvoton,sys=<&sys 1>
  nuvoton,sys=<&sys>       clk: HUSBH0_GATE       clk: HUSBH1_GATE
        │                        │                      │
   ┌────┴────┐            ┌──────┴──────┐        ┌──────┴──────┐
   │  dwc2   │            │ehci0  ohci0 │        │ehci1  ohci1 │
   │(gadget) │            │generic-ehci │        │generic-ehci │
   └─────────┘            │generic-ohci │        │generic-ohci │
                          └─────────────┘        └─────────────┘
       USB0 (OTG)               USB0 (host)           USB1 (host)
```

### Files

| File | Status | Notes |
|------|--------|-------|
| `phy/nuvoton/phy-ma35d1-usb2.c` | mainline | Gadget PHY, PHY0 device mode only |
| `phy/nuvoton/phy-ma35d1-otg.c` | **new** | Host PHY, PHY0+1, `nuvoton,sys=<&sys N>` |
| `phy/nuvoton/Kconfig` | modified | Added `PHY_MA35_USB_OTG` |
| `phy/nuvoton/Makefile` | modified | Added `phy-ma35d1-otg.o` |
| `dts/ma35d1.dtsi` | modified | `usb_phy0`, `usb_hphy0/1`, `usb`(DWC2), `ehci0/1`, `ohci0/1` |
| `dts/ma35d1-som-256m.dts` | modified | Enables `usb_hphy0/1` |
| `dts/ma35d1-iot-512m.dts` | modified | Enables `usb_hphy0` |
| `usb/host/ehci-ma35.c` | **dropped** | Use `generic-ehci` + `phy-ma35d1-otg.c` |
| `usb/host/ohci-ma35.c` | **dropped** | Use `generic-ohci` + `phy-ma35d1-otg.c` |

### Key design decisions

- EHCI/OHCI use `generic-ehci` / `generic-ohci`. The USB core
  (`drivers/usb/core/phy.c`) automatically calls `phy_init()` + `phy_power_on()`
  for any PHY listed in the `phys` DT property — no custom host controller
  driver needed.
- `phy-ma35d1-otg.c` reads the PHY index from `nuvoton,sys = <&sys N>` using
  `of_parse_phandle_with_fixed_args()` + `syscon_node_to_regmap()` (same
  pattern as the MA35D1 GMAC driver).
- PHY init checks "already operational" before running POR, protecting PHY0
  from being reset if DWC2 already initialized it.

## Next: OTG role switch in `phy-ma35d1-otg.c`

When `phy_idx == 0` (USB0), the driver should also act as the OTG role switch
controller, monitoring the hardware ID pin.

### What to implement

1. **Request the SYS interrupt** (GIC_SPI 0) and enable the ID-change bit:
   ```c
   regmap_update_bits(sysreg, MA35_SYS_MISCIER,
                      MISCIER_USB0IDCHGIEN, MISCIER_USB0IDCHGIEN);
   ```

2. **Register a `usb_role_switch`** so that DWC2 can consume it:
   ```c
   role_sw_desc.set = ma35_otg_role_sw_set;
   role_sw_desc.fwnode = dev_fwnode(dev);
   p->role_sw = usb_role_switch_register(dev, &role_sw_desc);
   ```

3. **IRQ handler** — clear flag, read `PWRONOTP[16]`, notify role switch:
   ```c
   regmap_read(sysreg, MA35_SYS_PWRONOTP, &val);
   role = (val & PWRONOTP_USBP0ID) ? USB_ROLE_HOST : USB_ROLE_DEVICE;
   usb_role_switch_set_role(p->role_sw, role);
   ```

4. **`ma35_otg_role_sw_set()`** — called back by DWC2 when role changes:
   - `USB_ROLE_DEVICE`: DWC2 re-connects gadget pull-up
   - `USB_ROLE_HOST`: DWC2 disconnects gadget pull-up (hardware mux takes over)

### DT additions needed

```dts
/* dtsi: link usb_hphy0 as the role switch provider */
usb_hphy0: usb-host-phy@0 {
    compatible = "nuvoton,ma35d1-usb2-phy-otg";
    ...
    #usb-role-switch-cells = <0>;   /* or use port/endpoint */
};

/* DWC2 node: consume the role switch */
usb: usb@40200000 {
    ...
    usb-role-switch;
    role-switch-default-mode = "peripheral";
    port {
        usb0_ep: endpoint {
            remote-endpoint = <&usb_hphy0_ep>;
        };
    };
};
```

### Interrupt node

The SYS interrupt (GIC_SPI 0) covers multiple sources. The `usb_hphy0` node
needs to reference it:
```dts
usb_hphy0: usb-host-phy@0 {
    ...
    interrupts = <GIC_SPI 0 IRQ_TYPE_LEVEL_HIGH>;
};
```
The handler must check `MISCISR[1]` (ID change) vs `MISCISR[2]` (VBUS change)
and clear only the relevant flag.
