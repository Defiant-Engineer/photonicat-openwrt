# Photonicat 2 Changes Made

This file records the modifications made in this fork so far.

## Device Package List

Updated:

- `target/linux/rockchip/image/armv8.mk`
- `target/linux/rockchip/image/Makefile`

The `ariaboard_photonicat-2` device profile now includes the packages needed for the observed factory hardware and the requested user-facing services.

The Photonicat 2 image now also overrides the Rockchip image partition sizes to match the official image layout:

- Boot partition: `64` MiB
- Root filesystem partition: `4096` MiB

This replaces the small default OpenWrt Rockchip rootfs partition for this device only.

Hardware and modem packages added or confirmed:

- `ath11k-firmware-wcn6855`
- `kmod-ath11k-pci`
- `kmod-rfkill`
- `kmod-aic8800u`
- `kmod-nvme`
- `kmod-photonicat-pm`
- `kmod-photonicat-usb-wdt`
- `kmod-spi-dev`
- `pcat2-display`
- `pcat2-mcu`
- `wpad-basic-mbedtls`
- `kmod-usb-net-qmi-wwan`
- `kmod-usb-serial-option`
- `kmod-usb-serial-qualcomm`
- `libqmi`
- `luci-proto-qmi`
- `qmi-utils`
- `uqmi`
- `wwan`

LuCI, package management, and requested services added:

- `luci`
- `opkg`
- `adguardhome`
- `luci-app-adguardhome`
- `tailscale`
- `luci-app-tailscale-community`
- `docker`
- `dockerd`
- `docker-compose`
- `containerd`
- `runc`
- `luci-app-dockerman`
- `mwan3`
- `luci-app-mwan3`
- `quectel-cm`

## AIC8800 Driver Package

Added:

- `package/lean/aic8800/`

This package was copied from the official Photonicat OpenWrt tree. It provides:

- `kmod-aic8800u`
- `kmod-aic8800s`

The Photonicat 2 factory image uses `kmod-aic8800u` for the built-in USB WiFi adapter.

## Kernel Module Definitions

Updated:

- `package/kernel/linux/modules/other.mk`

Changes:

- Extended `kmod-rfkill` to include `CONFIG_RFKILL_GPIO`.
- Added `rfkill-gpio.ko` to the `kmod-rfkill` file list.
- Added a new `kmod-photonicat-usb-wdt` package for the Photonicat USB hub watchdog driver.

## Device Tree Patch

Updated:

- `target/linux/rockchip/patches-6.12/102-arm64-dts-rockchip-rk3576-photonicat-2.patch`

Changes:

- Added `pcat-usb-wdt` node for the Photonicat USB hub watchdog.
- Set the DTS compatible to `ariaboard,photonicat2`, matching the running official Photonicat 2 image.
- Removed the previous static GPIO hog for USB hub reset so the watchdog driver can own that reset GPIO.
- Added `pcat-pm` child node under `uart10` for the Photonicat MCU/power-management controller.
- Confirmed the patch hunk line count matches the declared `+1,787` lines.

## Default Network Configuration

Updated:

- `target/linux/rockchip/armv8/base-files/etc/board.d/02_network`

Changes:

- Set the Photonicat 2 LAN interface to `192.168.80.1/24`.
- Kept WAN on DHCP.
- Left the physical port mapping as LAN on `eth1` and WAN on `eth0`.
- Added `wan6` DHCPv6 on `eth0`.
- Added official-style cellular interfaces:
  - `wwan_5g` DHCP on `usb0`, metric `10`, `auto 0`
  - `wwan_5g_v6` DHCPv6 on `usb0`, metric `11`, `auto 0`
  - `wwan_lte` DHCP on `wwan0`, metric `12`
  - `wwan_lte_v6` DHCPv6 on `wwan0`, metric `13`

Added:

- `target/linux/rockchip/armv8/base-files/etc/uci-defaults/99-photonicat2-network`

Changes:

- Set LAN DHCP start to `100`.
- Set LAN DHCP limit to `61`.
- This yields DHCP leases from `192.168.80.100` through `192.168.80.160` inclusive.
- Added the cellular interfaces to the WAN firewall zone.
- Enabled IPv6 masquerading on the WAN firewall zone, matching the official Photonicat 2 image.
- Starts `quectel-cm -4 -6` on first boot if the binary is present.

## Quectel Modem Controller

Added:

- `package/lean/quectel-cm/`

This package was copied from the official Photonicat OpenWrt tree. The running official Photonicat 2 image uses `quectel-cm -4 -6` to bring up the Quectel modem and then obtains addresses through DHCP/DHCPv6 on `wwan0`.

Local addition:

- Added `/etc/init.d/quectel-cm` packaging so the modem controller starts under `procd` and respawns if it exits.
- The Photonicat 2 UCI defaults enable and start that init service on first boot.
