# Photonicat 2 TODO

This is the remaining work before the custom image should be considered ready for testing on hardware.

## Build Host Blockers

The current macOS checkout still fails OpenWrt prerequisite checks.

Observed blockers:

- The OpenWrt build tree is on a case-insensitive filesystem.
- GNU `patch` is missing or not first in `PATH`.
- GNU `diffutils` is missing or not first in `PATH`.
- Extended `getopt` with `--long` support is missing or not first in `PATH`.

Options:

- Build inside a Linux VM/container.
- Move the checkout to a case-sensitive APFS volume and ensure GNU tools are installed and first in `PATH`.
- Use `FORCE=1` only for limited metadata checks. Do not treat a forced build as a clean final validation.

## Validation Still Needed

- Run `./scripts/feeds update -a`.
- Run `./scripts/feeds install -a`.
- Run `gmake defconfig`.
- Build the Photonicat 2 image.
- Confirm the final image contains:
  - `luci`
  - `opkg`
  - `tailscale`
  - `luci-app-tailscale-community`
  - `adguardhome`
  - `luci-app-adguardhome`
  - `docker`
  - `dockerd`
  - `docker-compose`
  - `luci-app-dockerman`
- Boot test on Photonicat 2.

## Hardware Boot Checks

After flashing or booting the image, verify:

- The board boots without kernel panic.
- Ethernet comes up.
- LuCI is reachable.
- The default LAN address is `192.168.80.1/24`.
- WAN remains configured for DHCP.
- WAN6 is configured for DHCPv6.
- LAN is assigned to `eth1`.
- WAN is assigned to `eth0`.
- LAN DHCP leases are handed out from `192.168.80.100` through `192.168.80.160`.
- `opkg` exists and can update package lists if network/DNS is configured.
- PCIe WiFi binds to `ath11k_pci`.
- WCN6855 firmware loads.
- Built-in USB WiFi binds to the AIC8800 driver.
- NVMe appears.
- QMI modem creates `/dev/cdc-wdm0`.
- Quectel modem serial ports appear as `/dev/ttyUSB*`.
- `quectel-cm -4 -6` starts.
- `wwan0` appears.
- `wwan_lte` receives an IPv4 lease on `wwan0`.
- `wwan_lte_v6` receives IPv6 information on `wwan0` when the carrier/SIM supports it.
- The WAN firewall zone includes `wwan_5g`, `wwan_lte`, `wwan_5g_v6`, and `wwan_lte_v6`.
- MCU device appears as `/dev/pcat-pm-ctl`.
- Battery, charger, fan, temperature, and RTC data are exposed.
- Display SPI device appears as `/dev/spidev1.0`.
- USB hub watchdog does not hold the hub in reset and recovers the hub if needed.

## Service Checks

- Confirm Tailscale daemon starts and can authenticate.
- Confirm the Tailscale LuCI page loads.
- Confirm AdGuard Home starts and the LuCI page loads.
- Confirm Docker daemon starts.
- Confirm `docker ps` works.
- Confirm the Dockerman LuCI page loads.

## Risk Items

- Docker and AdGuard Home increase image size significantly. Confirm the target partition has enough space.
- The AIC8800 package came from the official Photonicat tree and should be build-tested against this fork's kernel version.
- The Photonicat PM and USB watchdog DTS bindings should be checked against the actual driver expectations during build and boot testing.
- The LuCI Tailscale package in this feed is named `luci-app-tailscale-community`, not `luci-app-tailscale`.
