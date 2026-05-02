# Photonicat 2 Factory Image Findings

These notes summarize what was found on the running official Photonicat 2 image and what should be represented in the custom OpenWrt image.

## Board Identity

- Model: `Ariaboard photonicat2`
- Compatible: `ariaboard,photonicat2`
- Kernel: `6.12.28`
- Distro: `photonicatWrt 25.02.0`

## Hardware Observed

## Factory Storage Layout

The official Photonicat 2 image was inspected on the running router.

- eMMC device: `/dev/mmcblk0`, 115.3 GiB
- Boot partition: `/dev/mmcblk0p1`, 64 MiB, ext4, label `kernel`, mounted at `/boot`
- Root partition: `/dev/mmcblk0p2`, 4 GiB, squashfs, mounted read-only at `/rom`
- Writable overlay: `/dev/loop0`, 3.8 GiB, F2FS label `rootfs_data`, mounted at `/overlay`
- Overlay root: `overlayfs:/overlay`, 3.8 GiB available as `/`
- Kernel command line root: `root=PARTUUID=5452574f-02`

The read-only squashfs content was about 194 MiB used, but the partition that contains it is 4 GiB. The custom image should therefore build a 64 MiB boot partition and a 4096 MiB root filesystem partition so first boot can create a similarly large writable overlay.

### PCIe WiFi

- Device: Qualcomm QCNFA765 / WCN6855
- PCI ID: `17cb:1103`
- Driver: `ath11k_pci`
- Needed OpenWrt packages:
  - `kmod-ath11k-pci`
  - `ath11k-firmware-wcn6855`

### Built-In USB WiFi

- Device: AIC8800D80 USB WiFi
- USB ID: `a69c:8d81`
- Factory package: `kmod-aic8800u`
- Needed OpenWrt package:
  - `kmod-aic8800u`

### 5G Modem

- Device: Quectel RM510Q-GLHA
- USB ID: `2c7c:0800`
- Interfaces observed:
  - `/dev/cdc-wdm0`
  - `/dev/ttyUSB0`
  - `/dev/ttyUSB1`
  - `/dev/ttyUSB2`
  - `/dev/ttyUSB3`
- Needed OpenWrt packages:
  - `kmod-usb-net-qmi-wwan`
  - `kmod-usb-serial-option`
  - `kmod-usb-serial-qualcomm`
  - `libqmi`
  - `qmi-utils`
  - `quectel-cm`
  - `uqmi`
  - `wwan`
  - `luci-proto-qmi`

The running official Photonicat 2 image uses `quectel-cm -4 -6` and DHCP/DHCPv6 interfaces on `wwan0`, rather than ModemManager.

Official network interface layout observed:

- `wan`: DHCP on `eth0`, metric `1`
- `wan6`: DHCPv6 on `eth0`, metric `2`
- `wwan_5g`: DHCP on `usb0`, metric `10`, disabled by default with `auto 0`
- `wwan_5g_v6`: DHCPv6 on `usb0`, metric `11`, disabled by default with `auto 0`
- `wwan_lte`: DHCP on `wwan0`, metric `12`
- `wwan_lte_v6`: DHCPv6 on `wwan0`, metric `13`

The official firewall WAN zone includes `wan`, `wan6`, `wwan_5g`, `wwan_lte`, `wwan_5g_v6`, and `wwan_lte_v6`.

### NVMe

- Device observed: SK hynix Gold P31
- Needed OpenWrt package:
  - `kmod-nvme`

### MCU / Power Management

- Device node observed:
  - `/dev/pcat-pm-ctl`
- Factory behavior includes:
  - Battery reporting
  - Charger reporting
  - Temperature reporting
  - Fan reporting/control
  - RTC exposure through `/dev/rtc0`
- Needed OpenWrt packages:
  - `kmod-photonicat-pm`
  - `pcat2-mcu`

### Display

- Device node observed:
  - `/dev/spidev1.0`
- Needed OpenWrt packages:
  - `kmod-spi-dev`
  - `pcat2-display`

### USB Hub Watchdog

- Official Photonicat tree contains a Photonicat USB watchdog driver.
- Needed OpenWrt package added to this fork:
  - `kmod-photonicat-usb-wdt`
