# MA35D1 Upstream Kernel (7.1.0-rc1) Migration — Change Log

Platform: Nuvoton MA35D1-SOM, Buildroot 2024.02, kernel at `../kernel7`

---

## Buildroot `.config` changes

| Setting | Value | Reason |
|---|---|---|
| `BR2_LINUX_KERNEL_MA35` | remove | Vendor-specific kernel option, not in Buildroot 2024.02 mainline |
| `BR2_LINUX_KERNEL_CUSTOM_GIT` | `y` | Switch kernel source to local git repo |
| `BR2_LINUX_KERNEL_CUSTOM_REPO_URL` | `/home/joeylu/Documents/upstream2026/drm/kernel7` | Path to local upstream kernel |
| `BR2_LINUX_KERNEL_CUSTOM_REPO_VERSION` | `drm-misc-next` | Use latest commit from the repo |
| `BR2_LINUX_KERNEL_DEFCONFIG` | `ma35d1` | Custom defconfig created in `kernel7/arch/arm64/configs/` |
| `BR2_LINUX_KERNEL_INTREE_DTS_NAME` | `nuvoton/ma35d1-som-256m` | DTS exists in upstream kernel, no change needed |
| `BR2_PACKAGE_HOST_LINUX_HEADERS_CUSTOM_6_6` | `y` | "6.6.x or later" covers kernel 7.x |
| `BR2_TARGET_ROOTFS_INITRAMFS` | `y` | Boot uses `rdinit=/sbin/init` which requires initramfs embedded in the kernel image; without this the kernel finds no `/sbin/init` and hangs waiting for a block device |
| `BR2_TARGET_GENERIC_GETTY_PORT` | `console` | Upstream `SERIAL_NUVOTON_MA35D1` driver registers as `ttyNVT*` not `ttyS*`; buildroot-generated inittab must match |

---

## Buildroot `Makefile` changes

Remove CUSTOM_5_10 related logic from `Makefile`:
```makefile
# Remove this block:
	@if grep -q "6_6_VERSION=y" $(@D)/.config; then \
		sed -i "s/CUSTOM_5_10=y/CUSTOM_6_6=y/" $(BR2_CONFIG); \
		sed -i "s/CUSTOM_6_6 is not set/CUSTOM_5_10 is not set/" $(BR2_CONFIG); \
	else \
		sed -i "s/CUSTOM_6_6=y/CUSTOM_5_10=y/" $(BR2_CONFIG); \
		sed -i "s/CUSTOM_5_10 is not set/CUSTOM_6_6 is not set/" $(BR2_CONFIG); \
	fi
```

## Buildroot `uboot-env.txt` changes

Replce board/nuvoton/ma35d1/uboot-env.txt with ramfs version.

## Kernel defconfig changes

File: `kernel7/arch/arm64/configs/ma35d1_defconfig`

| Config | Value | Reason |
|---|---|---|
| `CONFIG_CMA` | `y` | Required for DMA contiguous memory allocator |
| `CONFIG_DMA_CMA` | `y` | Display driver allocates framebuffers via CMA; without it `0K cma-reserved` and display may fail |
| `CONFIG_CMA_SIZE_MBYTES` | `32` | Match the 32 MiB reservation used by the working 6.11 build |

Already present and correct in the defconfig:
- `CONFIG_ARCH_MA35`, `CONFIG_DEVTMPFS`, `CONFIG_DEVTMPFS_MOUNT`
- `CONFIG_SERIAL_NUVOTON_MA35D1`, `CONFIG_SERIAL_NUVOTON_MA35D1_CONSOLE`
- `CONFIG_MMC`, `CONFIG_MMC_SDHCI`, `CONFIG_MMC_SDHCI_OF_MA35D1`
- `CONFIG_DRM`, `CONFIG_DRM_PANEL_SIMPLE`, `CONFIG_DRM_VERISILICON_DC`
- `CONFIG_EXT4_FS`, `CONFIG_TMPFS`
- `CONFIG_STMMAC_ETH`, `CONFIG_ICPLUS_PHY`, `CONFIG_REALTEK_PHY`
- `CONFIG_PINCTRL_MA35D1`, `CONFIG_MTD`, `CONFIG_MTD_RAW_NAND`
- `CONFIG_TEE`, `CONFIG_OPTEE`

---

## Build patches (already applied)

### 1. GCC 12.3 — `linux/scc.h` removed in kernel ≥5.17
- **Symptom:** `fatal error: linux/scc.h: No such file or directory` during `host-gcc-initial`/`host-gcc-final`
- **Cause:** GCC 12.3 `libsanitizer` still includes `<linux/scc.h>`, removed with the Z8530 driver in v5.17; fixed in GCC 13
- **Patch:** `package/gcc/12.3.0/0004-libsanitizer-remove-linux-scc.h-removed-in-kernel-5.17.patch`
- **Backup:** `modified/gcc-12.3.0-patches/`

### 2. BusyBox 1.36.1 — CBQ qdisc removed in kernel ≥6.9
- **Symptom:** `error: 'TCA_CBQ_MAX' undeclared` during busybox build
- **Cause:** CBQ qdisc and all `TCA_CBQ_*` / `tc_cbq_*` symbols removed from `linux/pkt_sched.h` in v6.9
- **Patch:** `package/busybox/0009-networking-tc-guard-CBQ-code-with-TCA-CBQ-MAX-removed-in-Linux-6.9.patch`
- **Backup:** `modified/busybox-1.36.1-patches/`

---

## Applying patches

Place patch files under `package/<pkgname>/` — Buildroot picks them up automatically on the next build.
To force re-patch an already-extracted package:
```bash
rm output/build/<pkg>/.stamp_patched
make
```
