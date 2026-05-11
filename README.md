## First step
- find devicetree info in ./nuvoton/dts/*
- start with primary plane support
- it's fine to modify the existing vc driver, try to add DCUltra support
- add platform support like vc_ma35d1.c if needed
- write development log under this file

## Second step
- add overlay plane support (future)

## Development Log

### Step 1: Add DCUltra Lite (MA35D1) primary plane support

**Approach**: Instead of a separate platform file, added a `vs_dc_info` platform data
structure to differentiate DC8000 vs DCUltra Lite at runtime. This avoids code duplication
since the register offsets are nearly identical.

**Key differences between DC8000 and DCUltra Lite handled**:

1. **No chip identity registers** (0x0020-0x0030) - DCUltra Lite uses static platform data
   instead of reading model/revision/customer_id from HW
2. **No CONFIG_EX commit mechanism** - DC8000 has registers at 0x1CC0 (FB_CONFIG_EX),
   0x24D8 (FB_TOP_LEFT), 0x24E0 (FB_BOTTOM_RIGHT), 0x2510 (FB_BLEND_CONFIG),
   0x2518 (PANEL_CONFIG_EX). DCUltra Lite doesn't have these - it uses direct writes
   with enable/reset bits in FB_CONFIG (bit 0 = enable, bit 4 = reset)
3. **No PANEL_START register** (0x1CCC) - DCUltra Lite panel runs when PANEL_CONFIG.RUNNING is set
4. **Different IRQ registers** - DCUltra Lite uses 0x147C (IRQ_STA) / 0x1480 (IRQ_EN);
   DC8000 uses 0x0010 (IRQ_ACK) / 0x0014 (IRQ_EN)
5. **Different clock topology** - DCUltra Lite: "bus" + "pixel" (2 clocks, no resets);
   DC8000: "core" + "axi" + "ahb" + "pix0..N" (3+N clocks, 3 resets)
6. **Single output only** - DCUltra Lite has 1 display output, no per-output indexing needed
7. **Max register 0x2000** vs DC8000's 0x2544

**Files modified**:
- `vs_hwdb.h` - Added `vs_dc_info` struct with `family`, `has_chip_id`, `has_config_ex` flags
- `vs_hwdb.c` - Exported `vs_formats_no_yuv444` for use by platform data
- `vs_dc.h` - Added `info` field to `vs_dc`
- `vs_dc.c` - Added `nuvoton,ma35d1-dcu` compatible, conditional clock/reset/IRQ handling
- `vs_primary_plane.c` - Guarded CONFIG_EX writes, added ENABLE/RESET bits for DCUltra Lite
- `vs_primary_plane_regs.h` - Added `VSDC_FB_CONFIG_ENABLE` and `VSDC_FB_CONFIG_RESET`
- `vs_crtc.c` - Conditional vblank IRQ enable/disable for different IRQ register locations
- `vs_bridge.c` - Guarded PANEL_START and PANEL_CONFIG_EX writes
- `vs_drm.c` - Max resolution 1920x1080 for DCUltra Lite
- `Kconfig` - Added `ARCH_MA35` to depends