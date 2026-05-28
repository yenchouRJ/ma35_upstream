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
Note: HSTCKSTB/CK12MSTB and DEVCKSTB are mutually exclusive status bits;
the PHY settles into one mode based on the connected cable.

**MISCFCR0 (+0x70)**
- bit 12: `UHOVRCURH` — over-current detect polarity (0 = active-low, 1 = active-high); shared by both host ports

**MISCIER (+0x78)**
- bit 1: `USB0IDCHGIEN` — USB0 ID pin change interrupt enable
- bit 2: `VBUSCHGIEN` — USB0 VBUS valid change interrupt enable

**MISCISR (+0x7C)**
- bit 1: `USB0IDCHGIF` — USB0 ID pin change interrupt flag (clear by writing 1)
- bit 2: `VBUSCHGIF` — USB0 VBUS valid change interrupt flag

SYS interrupt: GIC_SPI 0 (shared, requires SYS node as interrupt parent — not
yet modeled; ID pin change detection falls back to sysfs polling)

### Board-level signals (SOM-256M)

| Pin | Function | Description |
|-----|----------|-------------|
| PL12 | `PWREN` | EHCI port power enable output → VBUS power switch enable |
| PL13 | `OVC` | Over-current detect input from VBUS power switch |
| PE15 | `OTG_VBUS` (VBUSVLD) | VBUS valid status input (R/O sense) |

**VBUSVLD (PF15) behavior:**
- High when connected to a USB host (board acts as device, VBUS supplied by host)
- High when OTG cable is plugged in (board acts as host, VBUS supplied by board)
- Low when nothing is connected

The VBUS power switch on the SOM-256M includes over-current protection and
reverse-current blocking. Combined with VBUSVLD hardware sensing, a VBUS
collision during host→device mode transition is handled by the board circuitry
without requiring software PM intervention.

### USB0 hardware topology

```
  USB0 physical port
        │
        │  ID pin → PWRONOTP[16] (R/O)
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
DWC2's `usb-role-switch` / `dwc2_drd_role_sw_set()` cannot be used for OTG
switching — it only overrides GOTGCTL bits inside DWC2, which has no effect
on the physical mux or EHCI0.

At end of `dwc2_probe()` (peripheral mode), `dwc2_lowlevel_hw_disable()` is
called → `phy_power_off()` → `ma35_usb_phy_power_off()` → clock off. This
is **expected behavior**; the clock is re-enabled when a gadget function driver
loads via the UDC framework.

## Driver architecture

```
  usb_phy0                 usb_hphy0              usb_hphy1
  (gadget PHY)             (host PHY0 + OTG)      (host PHY1)
  phy-ma35d1-usb2.c        phy-ma35d1-otg.c       phy-ma35d1-otg.c
  clk: USBD_GATE           nuvoton,sys=<&sys 0>   nuvoton,sys=<&sys 1>
  nuvoton,sys=<&sys>       clk: HUSBH0_GATE       clk: HUSBH1_GATE
        │                  role-switch (sysfs)           │
   ┌────┴────┐            ┌──────┴──────┐         ┌──────┴──────┐
   │  dwc2   │            │ehci0  ohci0 │         │ehci1  ohci1 │
   │(gadget) │            │generic-ehci │         │generic-ehci │
   └─────────┘            │generic-ohci │         │generic-ohci │
                          └─────────────┘         └─────────────┘
       USB0 (OTG)              USB0 (host)            USB1 (host)
```

Both DWC2 and EHCI0/OHCI0 are probed at boot and remain active simultaneously.
The hardware mux routes USB0 physical signals to DWC2 (ID=0) or EHCI0/OHCI0
(ID=1). No kernel driver binding/unbinding occurs on role change.

## Files

| File | Status | Notes |
|------|--------|-------|
| `phy/nuvoton/phy-ma35d1-usb2.c` | mainline | Gadget PHY, PHY0 device mode only |
| `phy/nuvoton/phy-ma35d1-otg.c` | **new** | Host PHY (PHY0+1) + OTG role switch (PHY0) |
| `phy/nuvoton/Kconfig` | modified | `PHY_MA35_USB_OTG`: selects GENERIC_PHY, MFD_SYSCON, USB_ROLE_SWITCH |
| `phy/nuvoton/Makefile` | modified | Added `phy-ma35d1-otg.o` |
| `dts/ma35d1.dtsi` | modified | `usb_phy0`, `usb_hphy0/1`, `usb`(DWC2), `ehci0/1`, `ohci0/1` |
| `dts/ma35d1-som-256m.dts` | modified | Enables `usb_hphy0/1` with `nuvoton,rcalcode=<7>` |
| `dts/ma35d1-iot-512m.dts` | modified | Enables `usb_hphy0` |
| `usb/host/ehci-ma35.c` | **dropped** | Replaced by `generic-ehci` + `phy-ma35d1-otg.c` |
| `usb/host/ohci-ma35.c` | **dropped** | Replaced by `generic-ohci` + `phy-ma35d1-otg.c` |

## Key design decisions

- **No custom EHCI/OHCI drivers.** `generic-ehci` / `generic-ohci` + USB core
  (`drivers/usb/core/phy.c`) automatically calls `phy_init()` + `phy_power_on()`
  for any PHY in the `phys` DT property.
- **`phy-ma35d1-otg.c`** reads PHY index from `nuvoton,sys = <&sys N>` via
  `of_parse_phandle_with_fixed_args()` + `syscon_node_to_regmap()` (same
  pattern as MA35D1 GMAC).
- **PHY init guard**: checks "already operational" before running POR, protecting
  PHY0 from being reset if DWC2 has already initialized it.
- **OTG role switch (PHY0 only)**: `usb_role_switch` registered with
  `allow_userspace_control = true`. `get()` reads `PWRONOTP[16]` live.
  DWC2 is NOT a consumer (its DT node has no `usb-role-switch` property).
  Role switch is sysfs-only; IRQ fallback in place but SYS interrupt demux
  not yet modeled in DT.
- **VBUS collision**: mitigated by board hardware (overcurrent regulator +
  reverse-current protection on VBUS power switch). No software PM needed.

## Kconfig (minimum for USB0 OTG + USB1 host)

```
# Host
CONFIG_USB=y
CONFIG_USB_OTG=y
CONFIG_USB_EHCI_HCD=y
CONFIG_USB_EHCI_HCD_PLATFORM=y
CONFIG_USB_OHCI_HCD=y
CONFIG_USB_OHCI_HCD_PLATFORM=y
CONFIG_USB_STORAGE=y
CONFIG_USB_UAS=y

# PHY
CONFIG_PHY_MA35_USB_OTG=y

# Gadget
CONFIG_USB_DWC2=y
CONFIG_USB_GADGET=y
CONFIG_USB_LIBCOMPOSITE=y
CONFIG_USB_SNP_UDC_PLAT=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_MASS_STORAGE=m
```

## Testing

### Host mode (USB1 or USB0 with OTG cable)

```shell
# Plug USB device into USB1 port (or OTG cable into USB0)
dmesg | grep -E 'usb|ehci|ohci'
# Expected: new USB device strings, product/manufacturer, speed
lsusb
```

### Device mode (USB0, standard USB cable to PC)

Connect a standard USB cable (not OTG) from USB0 to a Linux/Windows PC.
The ID pin floats → `PWRONOTP[16]` = 0 → device mode.

```shell
# Verify role
cat /sys/class/usb_role/soc:usb-host-phy@0-role-switch/role
# device

# Verify UDC is present
ls /sys/class/udc/
# 40200000.usb

# Load gadget
dd if=/dev/zero of=/tmp/disk.img bs=1M count=64
modprobe g_mass_storage file=/tmp/disk.img removable=1

# PC should enumerate the board as a USB mass storage device (dmesg -w)
# On board: dmesg should show g_mass_storage activated

# Unload
rmmod g_mass_storage
```

### OTG role sysfs

```shell
# Read current role (reads PWRONOTP[16] live)
cat /sys/class/usb_role/soc:usb-host-phy@0-role-switch/role
# host  (OTG cable plugged in, ID grounded)
# device  (standard cable or nothing)
```
