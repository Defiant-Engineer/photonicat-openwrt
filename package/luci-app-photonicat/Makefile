# SPDX-License-Identifier: GPL-2.0

include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-photonicat
PKG_VERSION:=1.1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=Brandon Cleary <cleary.brandon@gmail.com>
PKG_LICENSE:=GPL-2.0

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-photonicat
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=Photonicat 2 System Dashboard
  DEPENDS:=+luci-base +pcat2-mcu +ucode-mod-ubus +jsonfilter
  PKGARCH:=all
endef

define Build/Compile
endef

define Package/luci-app-photonicat/install
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(CP) ./root/usr/share/luci/menu.d/* $(1)/usr/share/luci/menu.d/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(CP) ./root/usr/share/rpcd/acl.d/* $(1)/usr/share/rpcd/acl.d/

	$(INSTALL_DIR) $(1)/usr/share/rpcd/ucode
	$(CP) ./root/usr/share/rpcd/ucode/* $(1)/usr/share/rpcd/ucode/

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) ./root/usr/sbin/* $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./root/etc/config/photonicat $(1)/etc/config/photonicat

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./root/etc/init.d/photonicat-fan $(1)/etc/init.d/photonicat-fan

	$(INSTALL_DIR) $(1)/www/luci-static/resources/view/photonicat
	$(CP) ./htdocs/luci-static/resources/view/photonicat/* $(1)/www/luci-static/resources/view/photonicat/
endef

define Package/luci-app-photonicat/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache.*
	rm -rf /tmp/luci-modulecache/
	/etc/init.d/rpcd reload 2>/dev/null
	/etc/init.d/photonicat-fan enable
	/etc/init.d/photonicat-fan start
	exit 0
}
endef

$(eval $(call BuildPackage,luci-app-photonicat))
