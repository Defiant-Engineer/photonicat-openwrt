include $(TOPDIR)/rules.mk

PKG_NAME:=pcat2-mcu
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Brandon Cleary <cleary.brandon@gmail.com>
PKG_LICENSE:=GPL-2.0-or-later

include $(INCLUDE_DIR)/package.mk

define Package/pcat2-mcu
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=PhotoniCAT 2 MCU Management Daemon
  DEPENDS:=@TARGET_rockchip_armv8 +libubus +libubox +libblobmsg-json
endef

define Package/pcat2-mcu/description
  Userspace daemon for managing the PhotoniCAT 2 MCU via UART.
  Provides a ubus interface for battery status, fan control, and RTC sync.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		$(TARGET_CONFIGURE_OPTS) \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/pcat2-mcu/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pcat2-mcud $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/photonicat-fan $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/pcat2-mcu.init $(1)/etc/init.d/pcat2-mcu
	$(INSTALL_BIN) ./files/photonicat-fan.init $(1)/etc/init.d/photonicat-fan

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/photonicat.config $(1)/etc/config/photonicat
endef

$(eval $(call BuildPackage,pcat2-mcu))
