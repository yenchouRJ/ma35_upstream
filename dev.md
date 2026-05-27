## Project structure

- ./dts/ : our dts files, which are copied to arch/arm64/boot/dts/nuvoton/
- ./yaml/ : referece yaml files
- ./phy/nuvoton : phy driver for usb0 which is gadget (for usb0 only, usb1 is host)
- ./usb/dwc2 : dwc2 driver, which is used for usb0 (gadget)
- ./usb/host : host controller driver, which is used for usb1 (host). Our host controller can be ehci or ohci
- /home/joeylu/Documents/upstream2026/usbh/usb: Refer this kernel if you need more details.

## Background
- For our platform, there is an usb ID pin to detect a connected one is a host or device, and inner circuit will be switched between two IPs according to this R/O status. So far we have dwc2 usb device and phy init driver for device upstreamed to mainline (only for usb0). I wrote ma35-ehci driver just because I also need to init 1. phy (for usb0 and 1) 2. over-current active level 3. resistor trim parameter. However, I think it's better to move phy control to a phy driver, and use generic ehci/ohci host controller driver. So I will drop ma35-ehci and ma35-ohci, and change compatible to generic ones. For phy driver, We can either create a new one or reuse the existing phy-ma35d1-usb2.c which is already upstreamed.

sysreg (0x40460000):
- PWRONOTP (+0x04):
  USBP0ID (bit 16 R/O): 0: USB0 is device, 1: USB0 is host
- USBPMISCR (+0x60) register layout (n = PHY index, 0 or 1):
  PHY0 (n=0): control bits 0-2, host stable bits 8-9, device stable bit 10, RCALCODE bits 12-15
  PHY1 (n=1): control bits 16-18, host stable bits 24-25, RCALCODE bits 28-31

  All PHY1 fields are exactly 16 bit-positions above PHY0 fields, enabling a single set of parameterized macros for both PHYs.

  SUSPEND bit semantics: 1 = operational mode, 0 = suspended.
  Ready condition: SUSPEND + HSTCKSTB + CK12MSTB all set.

- MISCIER (+0x78):
  USB0IDCHGIEN (bit 1): USB0_ID Pin Status Change Interrupt Enable Bit
  VBUSCHGIEN (bit 2): USUSB0_VBUSVLD Pin Status Change Interrupt Enable Bit

- MISCISR (+0x7C):
  USB0IDCHGIF (bit 1): USB0_ID Pin Status Change Interrupt Flag
  VBUSCHGIF (bit 2): USUSB0_VBUSVLD Pin Status Change Interrupt Flag

  Interrupt ID is: GIC_SPI 0

## Goal

- usb/host/ohci-ma35.c is used to be a ohci driver for ma35d1. Becuase PHY can be controlled by ehci driver, so I have dropped it and used platform driver for ohci. So in dtsi, I changed compatible to "generic-ohci". It works well.
- usb/host/ehci-ma35.c is ehci driver for ma35d1. This driver now do some PHY related control (ma35_ehci_phy_init). I'd like to move phy control to a phy/nuvoton/ with either a new driver, phy-ma35d1-otg, or reuse the existing phy-ma35d1-usb2.c. I will drop this driver and use generic ehci driver. So in dtsi, I will change compatible to "generic-ehci" and add "phys" and "phy-names" properties.
- Role switch: Please also see dwc2 driver and yaml, there are some phy control code about otg role switch, usb-role-switch, and dr_mode. I'm not sure whether we can use something like usb_role_switch_set_role to trigger role switch in phy driver. (see register description above)

## Results
