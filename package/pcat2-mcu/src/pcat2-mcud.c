// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * pcat2-mcud — Photonicat 2 MCU management daemon
 *
 * Provides a ubus interface for interacting with the Photonicat 2 MCU.
 * Handles periodic status reports, heartbeats, and hardware control.
 *
 * Copyright (C) 2026 Brandon Cleary <cleary.brandon@gmail.com>
 */

#include <errno.h>
#include <libubox/blobmsg_json.h>
#include <libubox/uloop.h>
#include <libubus.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcat2-mcu.h"

#define MCU_DEVICE "/dev/ttyS10"
#define MCU_BAUD 115200

static struct ubus_context *ctx;
static mcu_ctx_t mcu;
static mcu_status_t last_status;
static bool status_valid = false;

static struct uloop_timeout hb_timer;
static struct uloop_fd mcu_fd;

/* ── Ubus methods ────────────────────────────────────────────────── */

enum { FAN_LEVEL, __FAN_MAX };

static const struct blobmsg_policy fan_policy[__FAN_MAX] = {
    [FAN_LEVEL] = {.name = "level", .type = BLOBMSG_TYPE_INT32},
};

static int pcat_status(struct ubus_context *ctx, struct ubus_object *obj,
		       struct ubus_request_data *req, const char *method,
		       struct blob_attr *msg)
{
	struct blob_buf b = {};

	if (!status_valid)
		return UBUS_STATUS_NOT_FOUND;

	blob_buf_init(&b, 0);

	void *bat = blobmsg_open_table(&b, "battery");
	blobmsg_add_u32(&b, "voltage", last_status.battery_mv);
	if (last_status.has_energy) {
		blobmsg_add_u32(&b, "capacity", last_status.soc);
		blobmsg_add_u32(&b, "energy_now", last_status.energy_now_uwh);
		blobmsg_add_u32(&b, "energy_full", last_status.energy_full_uwh);
	}
	if (last_status.has_temp) {
		blobmsg_add_i32(&b, "current", last_status.battery_current_ma);
	}
	blobmsg_close_table(&b, bat);

	void *chg = blobmsg_open_table(&b, "charger");
	blobmsg_add_u32(&b, "voltage", last_status.charger_mv);
	blobmsg_add_u8(&b, "online", last_status.on_charger);
	blobmsg_close_table(&b, chg);

	if (last_status.has_temp)
		blobmsg_add_i32(&b, "board_temp", last_status.board_temp_c);

	if (last_status.has_motion) {
		blobmsg_add_u32(&b, "fan_rpm", last_status.fan_rpm);
		void *acc = blobmsg_open_table(&b, "accel");
		blobmsg_add_i32(&b, "x", last_status.accel_x);
		blobmsg_add_i32(&b, "y", last_status.accel_y);
		blobmsg_add_i32(&b, "z", last_status.accel_z);
		blobmsg_add_u8(&b, "ready", last_status.accel_ready);
		blobmsg_close_table(&b, acc);
	}

	char rtc_str[32];
	snprintf(rtc_str, sizeof(rtc_str), "%04u-%02u-%02u %02u:%02u:%02u",
		 last_status.rtc_year, last_status.rtc_month,
		 last_status.rtc_day, last_status.rtc_hour, last_status.rtc_min,
		 last_status.rtc_sec);
	blobmsg_add_string(&b, "rtc", rtc_str);
	blobmsg_add_u8(&b, "rtc_valid", last_status.rtc_status == 0);

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

static int pcat_set_fan(struct ubus_context *ctx, struct ubus_object *obj,
			struct ubus_request_data *req, const char *method,
			struct blob_attr *msg)
{
	struct blob_attr *tb[__FAN_MAX];
	int level;

	blobmsg_parse(fan_policy, __FAN_MAX, tb, blob_data(msg), blob_len(msg));

	if (!tb[FAN_LEVEL])
		return UBUS_STATUS_INVALID_ARGUMENT;

	level = blobmsg_get_u32(tb[FAN_LEVEL]);
	if (level < 0 || level > 9)
		return UBUS_STATUS_INVALID_ARGUMENT;

	mcu_set_fan(&mcu, level);

	return 0;
}

static const struct ubus_method pcat_methods[] = {
    UBUS_METHOD_NOARG("status", pcat_status),
    UBUS_METHOD("set_fan", pcat_set_fan, fan_policy),
};

static struct ubus_object_type pcat_object_type =
    UBUS_OBJECT_TYPE("photonicat", pcat_methods);

static struct ubus_object pcat_object = {
    .name = "photonicat",
    .type = &pcat_object_type,
    .methods = pcat_methods,
    .n_methods = ARRAY_SIZE(pcat_methods),
};

/* ── MCU Event Handling ──────────────────────────────────────────── */

static void mcu_read_cb(struct uloop_fd *u, unsigned int events)
{
	mcu_frame_t frame;

	while (mcu_recv(&mcu, &frame) == 1) {
		if (frame.command == MCU_CMD_STATUS_REPORT &&
		    frame.extra_len >= 16) {
			if (mcu_parse_status(frame.extra_data, frame.extra_len,
					     &last_status) == 0) {
				status_valid = true;
			}
		}

		if (frame.need_ack) {
			mcu_send(&mcu, frame.command + 1, NULL, 0, false);
		}
	}
}

static void hb_timer_cb(struct uloop_timeout *t)
{
	mcu_send_heartbeat(&mcu);
	uloop_timeout_set(t, 2000);
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
	const char *ubus_socket = NULL;
	int ret;

	uloop_init();

	if (mcu_open(&mcu, MCU_DEVICE, MCU_BAUD) < 0) {
		fprintf(stderr, "Failed to open MCU device %s: %s\n",
			MCU_DEVICE, strerror(errno));
		return 1;
	}

	mcu_fd.fd = mcu.fd;
	mcu_fd.cb = mcu_read_cb;
	uloop_fd_add(&mcu_fd, ULOOP_READ);

	ctx = ubus_connect(ubus_socket);
	if (!ctx) {
		fprintf(stderr, "Failed to connect to ubus\n");
		return 1;
	}

	ubus_add_uloop(ctx);

	ret = ubus_add_object(ctx, &pcat_object);
	if (ret) {
		fprintf(stderr, "Failed to add ubus object: %s\n",
			ubus_strerror(ret));
		return 1;
	}

	hb_timer.cb = hb_timer_cb;
	uloop_timeout_set(&hb_timer, 1000);

	uloop_run();

	ubus_free(ctx);
	mcu_close(&mcu);
	uloop_done();

	return 0;
}
