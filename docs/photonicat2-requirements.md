# Photonicat 2 Requirements

This file records the user goals for the Photonicat 2 OpenWrt image work.

## Main Goal

Build a closer-to-vanilla OpenWrt image for the Photonicat 2 while preserving the hardware support and user-facing packages that are present or needed on the official Photonicat image.

## Source Repositories Compared

- Official Photonicat OpenWrt tree: <https://github.com/photonicat/photonicat_openwrt>
- th3cavalry Photonicat 2 tree: <https://github.com/th3cavalry/photonicat2>
- User fork being modified: <https://github.com/Defiant-Engineer/photonicat-openwrt>

Local checkout currently being modified:

- `/Users/brandon/Projects/codex/defiant_photonicat_openwrt`
- Branch: `add-ariaboard-photonicat-2`

## User Requests

- Include the drivers needed for built-in WiFi, PCIe WiFi, and the MCU controller.
- Use the official Photonicat image as the reference for custom packages and hardware behavior.
- Keep the image closer to base OpenWrt than the official vendor image.
- Include LuCI by default.
- Include `opkg` by default.
- Include Tailscale and its LuCI package.
- Include Docker and its LuCI package.
- Include AdGuard Home and its LuCI package.
- Include necessary dependencies for the requested services.
- Use `192.168.80.1/24` as the default LAN address.
- Keep WAN as DHCP.
- Use `192.168.80.100` through `192.168.80.160` as the LAN DHCP pool.
- Configure the built-in cellular modem to come up automatically.
- Keep the Ethernet role assignment aligned with the official Photonicat 2 image: WAN on `eth0`, LAN on `eth1`.

## Router Access Used For Discovery

- SSH target: `root@172.16.0.1`
- Password: `photonicat`
- Factory image observed: `photonicatWrt 25.02.0`
- Kernel observed: `6.12.28`
