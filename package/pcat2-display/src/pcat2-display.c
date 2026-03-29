/*
 * pcat2-display - PhotoniCAT 2 Mini Display Status Application
 *
 * Drives the GC9307 172x320 TFT LCD via SPI to show system status.
 * All configuration is read from UCI (/etc/config/photonicat).
 *
 * Hardware: GC9307 on SPI1.0 (6MHz), DC=GPIO3_PD1, RST=GPIO3_PD2, BL=GPIO3_C5
 * Display: 172x320 pixels, RGB565 color, rotation 180 deg, column offset 34
 * Backlight: active LOW (PWM polarity inverted per factory DTS)
 *
 * This version uses libubus to fetch status from pcat2-mcud.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Brandon Cleary <cleary.brandon@gmail.com>
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/spi/spidev.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <uci.h>

/* ================================================================== */
/*  Hardware constants                                                 */
/* ================================================================== */

#define SPI_DEVICE "/dev/spidev1.0"
#define SPI_SPEED_HZ 6000000
#define SPI_CHUNK 4096

#define DISP_W 172
#define DISP_H 320
#define COL_OFFSET 34

#define DC_BANK 3
#define DC_OFFSET 25
#define RST_BANK 3
#define RST_OFFSET 26
#define BL_BANK 3
#define BL_OFFSET 21

/* GC9307 / ST7789 registers */
#define CMD_SWRESET 0x01
#define CMD_SLPOUT 0x11
#define CMD_SLPIN 0x10
#define CMD_NORON 0x13
#define CMD_INVOFF 0x20
#define CMD_DISPON 0x29
#define CMD_CASET 0x2A
#define CMD_RASET 0x2B
#define CMD_RAMWR 0x2C
#define CMD_COLMOD 0x3A
#define CMD_MADCTL 0x36
#define MADCTL_MX 0x40

/* BGR565 colour helpers - display byte order is big-endian */
#define RGB565(r, g, b)                                                        \
	((uint16_t)((((uint16_t)((b) >> 3)) << 11) |                           \
		    (((uint16_t)((g) >> 2)) << 5) | ((uint16_t)((r) >> 3))))

/* ================================================================== */
/*  Theme colours                                                      */
/* ================================================================== */

static uint16_t COL_BG;
static uint16_t COL_FG;
static uint16_t COL_ACCENT;
static uint16_t COL_DIM;
static uint16_t COL_TOPBAR;
static uint16_t COL_GOOD;
static uint16_t COL_WARN;
static uint16_t COL_BAD;

struct theme {
	const char *name;
	uint16_t bg, fg, accent, dim, topbar, good, warn, bad;
};

static const struct theme themes[] = {
    {"dark", RGB565(0, 0, 0), RGB565(255, 255, 255), RGB565(0, 220, 220),
     RGB565(40, 40, 40), RGB565(0, 8, 30), RGB565(40, 255, 40),
     RGB565(255, 200, 0), RGB565(255, 40, 40)},
    {"light", RGB565(240, 240, 235), RGB565(20, 20, 20), RGB565(0, 100, 180),
     RGB565(180, 180, 170), RGB565(200, 200, 195), RGB565(0, 130, 0),
     RGB565(200, 150, 0), RGB565(200, 0, 0)},
    {NULL, 0, 0, 0, 0, 0, 0, 0, 0}};

static void apply_theme(const char *name)
{
	for (int i = 0; themes[i].name; i++) {
		if (strcmp(themes[i].name, name) == 0) {
			COL_BG = themes[i].bg;
			COL_FG = themes[i].fg;
			COL_ACCENT = themes[i].accent;
			COL_DIM = themes[i].dim;
			COL_TOPBAR = themes[i].topbar;
			COL_GOOD = themes[i].good;
			COL_WARN = themes[i].warn;
			COL_BAD = themes[i].bad;
			return;
		}
	}
	apply_theme("dark");
}

/* ================================================================== */
/*  Configuration                                                      */
/* ================================================================== */

#define MAX_PAGES 8

struct config {
	int backlight;
	int refresh;
	int poweroff_ms;
	char theme[16];
	float font_scale;
	int num_pages;
	char pages[MAX_PAGES][16];
};

static struct config cfg;

static void config_load(void)
{
	struct uci_context *u = uci_alloc_context();
	if (!u)
		return;
	struct uci_package *p = NULL;
	struct uci_section *s;
	const char *val;

	cfg.backlight = 1;
	cfg.refresh = 5;
	cfg.poweroff_ms = 2000;
	cfg.font_scale = 1.0f;
	strncpy(cfg.theme, "dark", sizeof(cfg.theme));
	cfg.num_pages = 0;

	if (uci_load(u, "photonicat", &p) == UCI_OK) {
		s = uci_lookup_section(u, p, "display");
		if (s) {
			val = uci_lookup_option_string(u, s, "backlight");
			if (val)
				cfg.backlight = atoi(val);
			val = uci_lookup_option_string(u, s, "refresh");
			if (val)
				cfg.refresh = atoi(val);
			val = uci_lookup_option_string(u, s, "theme");
			if (val) {
				strncpy(cfg.theme, val, sizeof(cfg.theme) - 1);
				cfg.theme[sizeof(cfg.theme) - 1] = '\0';
			}
			val = uci_lookup_option_string(u, s, "font_scale");
			if (val)
				cfg.font_scale = strtof(val, NULL);

			struct uci_element *e;
			struct uci_option *o = uci_lookup_option(u, s, "pages");
			if (o && o->type == UCI_TYPE_LIST) {
				uci_foreach_element(&o->v.list, e)
				{
					if (cfg.num_pages < MAX_PAGES) {
						strncpy(
						    cfg.pages[cfg.num_pages],
						    e->name,
						    sizeof(cfg.pages[0]) - 1);
						cfg.pages[cfg.num_pages]
							 [sizeof(cfg.pages[0]) -
							  1] = '\0';
						cfg.num_pages++;
					}
				}
			}
		}
		uci_unload(u, p);
	}
	uci_free_context(u);
	apply_theme(cfg.theme);
}

/* ================================================================== */
/*  Ubus Data Collection                                               */
/* ================================================================== */

static struct ubus_context *ubus_ctx;
static uint32_t photonicat_obj;

struct mcu_data {
	int battery_pct;
	int battery_mv;
	int battery_ma;
	int charger_online;
	int board_temp;
	int fan_rpm;
	int fan_level;
	bool valid;
};

static struct mcu_data mdata;

static void status_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct blob_attr *tb[8];
	static const struct blobmsg_policy status_policy[8] = {
	    {.name = "battery", .type = BLOBMSG_TYPE_TABLE},
	    {.name = "charger", .type = BLOBMSG_TYPE_TABLE},
	    {.name = "board_temp", .type = BLOBMSG_TYPE_INT32},
	    {.name = "fan_rpm", .type = BLOBMSG_TYPE_INT32},
	    {.name = "fan_level", .type = BLOBMSG_TYPE_INT32},
	};
	struct blob_attr *bat[4];
	static const struct blobmsg_policy bat_policy[4] = {
	    {.name = "capacity", .type = BLOBMSG_TYPE_INT32},
	    {.name = "voltage", .type = BLOBMSG_TYPE_INT32},
	    {.name = "current", .type = BLOBMSG_TYPE_INT32},
	};
	struct blob_attr *chg[2];
	static const struct blobmsg_policy chg_policy[2] = {
	    {.name = "online", .type = BLOBMSG_TYPE_INT32},
	};

	blobmsg_parse(status_policy, 8, tb, blobmsg_data(msg),
		      blobmsg_data_len(msg));

	if (tb[0]) {
		blobmsg_parse(bat_policy, 4, bat, blobmsg_data(tb[0]),
			      blobmsg_data_len(tb[0]));
		if (bat[0])
			mdata.battery_pct = blobmsg_get_u32(bat[0]);
		if (bat[1])
			mdata.battery_mv = blobmsg_get_u32(bat[1]);
		if (bat[2])
			mdata.battery_ma = blobmsg_get_i32(bat[2]);
	}
	if (tb[1]) {
		blobmsg_parse(chg_policy, 2, chg, blobmsg_data(tb[1]),
			      blobmsg_data_len(tb[1]));
		if (chg[0])
			mdata.charger_online = blobmsg_get_u32(chg[0]);
	}
	if (tb[2])
		mdata.board_temp = blobmsg_get_i32(tb[2]);
	if (tb[3])
		mdata.fan_rpm = blobmsg_get_u32(tb[3]);
	if (tb[4])
		mdata.fan_level = blobmsg_get_u32(tb[4]);
	mdata.valid = true;
}

static void update_mcu_data(void)
{
	if (!ubus_ctx)
		ubus_ctx = ubus_connect(NULL);
	if (!ubus_ctx)
		return;

	if (ubus_lookup_id(ubus_ctx, "photonicat", &photonicat_obj) ==
	    UBUS_STATUS_OK) {
		ubus_invoke(ubus_ctx, photonicat_obj, "status", NULL, status_cb,
			    NULL, 500);
	}
}

/* ================================================================== */
/*  Framebuffer & Rendering                                            */
/* ================================================================== */

static uint8_t fb[DISP_W * DISP_H * 2];
static int spi_fd = -1;
static int dc_line_fd = -1, rst_line_fd = -1, bl_line_fd = -1;
static int input_fd = -1;
static int page_idx = 0;

static void fb_pixel(int x, int y, uint16_t col)
{
	if (x < 0 || x >= DISP_W || y < 0 || y >= DISP_H)
		return;
	int off = (y * DISP_W + x) * 2;
	fb[off] = col >> 8;
	fb[off + 1] = col & 0xFF;
}

static void fb_clear(uint16_t col)
{
	uint8_t hi = col >> 8, lo = col & 0xFF;
	for (int i = 0; i < DISP_W * DISP_H; i++) {
		fb[i * 2] = hi;
		fb[i * 2 + 1] = lo;
	}
}

static void fb_hline(int x, int y, int w, uint16_t col)
{
	for (int i = 0; i < w; i++)
		fb_pixel(x + i, y, col);
}

static void fb_rect(int x, int y, int w, int h, uint16_t col)
{
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++)
			fb_pixel(x + i, y + j, col);
}

static const unsigned char font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00},
    {0x00, 0x07, 0x00, 0x07, 0x00}, {0x14, 0x7F, 0x14, 0x7F, 0x14},
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00},
    {0x00, 0x1C, 0x22, 0x41, 0x00}, {0x00, 0x41, 0x22, 0x1C, 0x00},
    {0x14, 0x08, 0x3E, 0x08, 0x14}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
    {0x00, 0x60, 0x60, 0x00, 0x00}, {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x00, 0x36, 0x36, 0x00, 0x00}, {0x00, 0x56, 0x36, 0x00, 0x00},
    {0x08, 0x14, 0x22, 0x41, 0x00}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x00, 0x41, 0x22, 0x14, 0x08}, {0x02, 0x01, 0x51, 0x09, 0x06},
    {0x32, 0x49, 0x79, 0x41, 0x3E}, {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01}, {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x7F, 0x41, 0x41, 0x00},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x00, 0x41, 0x41, 0x7F, 0x00},
    {0x04, 0x02, 0x01, 0x02, 0x04}, {0x40, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20},
    {0x38, 0x44, 0x44, 0x48, 0x7F}, {0x38, 0x54, 0x54, 0x54, 0x18},
    {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x0C, 0x52, 0x52, 0x52, 0x3E},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00},
    {0x20, 0x40, 0x44, 0x3D, 0x00}, {0x7F, 0x10, 0x28, 0x44, 0x00},
    {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38},
    {0x7C, 0x14, 0x14, 0x14, 0x08}, {0x08, 0x14, 0x14, 0x18, 0x7C},
    {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C},
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, {0x3C, 0x40, 0x30, 0x40, 0x3C},
    {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00},
    {0x00, 0x00, 0x7F, 0x00, 0x00}, {0x00, 0x41, 0x36, 0x08, 0x00},
    {0x10, 0x08, 0x08, 0x10, 0x08},
};

static void draw_char(int x, int y, char ch, uint16_t fg, uint16_t bg, float s)
{
	if (ch < 0x20 || ch > 0x7E)
		ch = '?';
	const unsigned char *glyph = font5x7[ch - 0x20];
	for (int col = 0; col < 6; col++) {
		uint8_t bits = (col < 5) ? glyph[col] : 0;
		for (int row = 0; row < 8; row++) {
			uint16_t c = (row < 7 && (bits & (1 << row))) ? fg : bg;
			if (c != bg || bg != 0)
				fb_rect(x + col * s, y + row * s, s, s, c);
		}
	}
}

static int draw_str(int x, int y, const char *str, uint16_t fg, uint16_t bg,
		    float s)
{
	while (*str) {
		draw_char(x, y, *str++, fg, bg, s);
		x += 6 * s;
	}
	return x;
}

static void draw_str_c(int y, const char *str, uint16_t fg, uint16_t bg,
		       float s)
{
	int w = strlen(str) * 6 * s;
	draw_str((DISP_W - w) / 2, y, str, fg, bg, s);
}

/* ================================================================== */
/*  Hardware & System Interaction                                      */
/* ================================================================== */

static int spi_init(void)
{
	spi_fd = open(SPI_DEVICE, O_RDWR);
	if (spi_fd < 0)
		return -1;
	uint8_t mode = SPI_MODE_0, bits = 8;
	uint32_t speed = SPI_SPEED_HZ;
	ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
	ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	return 0;
}

static void spi_write(const uint8_t *data, size_t len)
{
	struct spi_ioc_transfer tr = {.tx_buf = (unsigned long)data,
				      .len = len,
				      .speed_hz = SPI_SPEED_HZ,
				      .bits_per_word = 8};
	ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static int gpio_request_output(int bank, int offset, const char *name,
			       int initial)
{
	char path[64];
	snprintf(path, sizeof(path), "/dev/gpiochip%d", bank);
	int fd = open(path, O_RDWR);
	if (fd < 0)
		return -1;
	struct gpio_v2_line_request req = {.offsets[0] = offset,
					   .num_lines = 1,
					   .config.flags =
					       GPIO_V2_LINE_FLAG_OUTPUT};
	strncpy(req.consumer, name, sizeof(req.consumer) - 1);
	req.consumer[sizeof(req.consumer) - 1] = '\0';
	if (initial) {
		req.config.num_attrs = 1;
		req.config.attrs[0].attr.id =
		    GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
		req.config.attrs[0].attr.values = 1;
		req.config.attrs[0].mask = 1;
	}
	if (ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
		close(fd);
		return -1;
	}
	close(fd);
	return req.fd;
}

static void gpio_set(int fd, int val)
{
	struct gpio_v2_line_values vals = {.mask = 1, .bits = val ? 1 : 0};
	ioctl(fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &vals);
}

static void disp_init(void)
{
	gpio_set(rst_line_fd, 0);
	usleep(50000);
	gpio_set(rst_line_fd, 1);
	usleep(50000);
	uint8_t cmd, data;
	cmd = CMD_SWRESET;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	usleep(150000);
	cmd = CMD_SLPOUT;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	usleep(150000);
	cmd = CMD_COLMOD;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	data = 0x55;
	gpio_set(dc_line_fd, 1);
	spi_write(&data, 1);
	cmd = CMD_MADCTL;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	data = MADCTL_MX;
	gpio_set(dc_line_fd, 1);
	spi_write(&data, 1);
	cmd = CMD_INVOFF;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	cmd = 0x13;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1); /* NORON */
	cmd = 0x29;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1); /* DISPON */
}

/* ================================================================== */
/*  Main Application                                                   */
/* ================================================================== */

static void render_screen(void)
{
	fb_clear(COL_BG);
	char tmp[64];
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	/* Clock */
	snprintf(tmp, sizeof(tmp), "%02d:%02d", t->tm_hour, t->tm_min);
	draw_str_c(20, tmp, COL_FG, COL_BG, 4.0f);

	/* Battery */
	if (mdata.valid) {
		snprintf(tmp, sizeof(tmp), "BAT: %d%% %d.%02dV",
			 mdata.battery_pct, mdata.battery_mv / 1000,
			 (mdata.battery_mv % 1000) / 10);
		draw_str_c(80, tmp, COL_GOOD, COL_BG, 1.5f);
		snprintf(tmp, sizeof(tmp), "TEMP: %dC FAN: %d",
			 mdata.board_temp, mdata.fan_rpm);
		draw_str_c(100, tmp, COL_ACCENT, COL_BG, 1.5f);
	}

	/* System */
	FILE *f = fopen("/proc/loadavg", "r");
	if (f) {
		float l1;
		fscanf(f, "%f", &l1);
		fclose(f);
		snprintf(tmp, sizeof(tmp), "LOAD: %.2f", l1);
		draw_str_c(140, tmp, COL_DIM, COL_BG, 1.5f);
	}

	/* Flush to SPI */
	uint8_t win[] = {0, (uint8_t)COL_OFFSET, 0,
			 (uint8_t)(DISP_W - 1 + COL_OFFSET)};
	uint8_t cmd = CMD_CASET;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	gpio_set(dc_line_fd, 1);
	spi_write(win, 4);
	uint8_t rwin[] = {0, 0, 1, (uint8_t)(DISP_H - 1)};
	cmd = CMD_RASET;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	gpio_set(dc_line_fd, 1);
	spi_write(rwin, 4);
	cmd = CMD_RAMWR;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);
	gpio_set(dc_line_fd, 1);
	for (int i = 0; i < sizeof(fb); i += SPI_CHUNK) {
		spi_write(fb + i, (sizeof(fb) - i > SPI_CHUNK)
				      ? SPI_CHUNK
				      : sizeof(fb) - i);
	}
}

static volatile sig_atomic_t running = 1;
static void sig_handler(int sig) { running = 0; }

int main(void)
{
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	config_load();
	if (spi_init() < 0)
		return 1;
	dc_line_fd = gpio_request_output(DC_BANK, DC_OFFSET, "pcat2-dc", 0);
	rst_line_fd = gpio_request_output(RST_BANK, RST_OFFSET, "pcat2-rst", 1);
	bl_line_fd = gpio_request_output(BL_BANK, BL_OFFSET, "pcat2-bl", 0);
	disp_init();
	gpio_set(bl_line_fd, 0); /* ON */

	while (running) {
		update_mcu_data();
		render_screen();
		for (int i = 0; i < cfg.refresh * 10 && running; i++)
			usleep(100000);
	}

	uint8_t cmd = 0x28;
	gpio_set(dc_line_fd, 0);
	spi_write(&cmd, 1);	 /* DISPOFF */
	gpio_set(bl_line_fd, 1); /* OFF */
	close(spi_fd);
	close(dc_line_fd);
	close(rst_line_fd);
	close(bl_line_fd);
	return 0;
}
