#!/usr/bin/env ucode
// Photonicat 2 — rpcd ucode backend for LuCI
// Provides system status and hardware control via ubus RPC.

'use strict';

import { readfile, writefile, glob, access, popen } from 'fs';
import { cursor } from 'uci';
import { connect } from 'ubus';

const ubus = connect();

function read_int(path) {
        let val = readfile(path);
        return val ? +trim(val) : null;
}

const methods = {
        get_status: {
                call: function() {
                        let result = {};

                        // ── Thermal zones (SoC internal) ──
                        let zones = [];
                        let tz_dirs = glob('/sys/class/thermal/thermal_zone*');
                        for (let dir in tz_dirs) {
                                let temp = read_int(dir + '/temp');
                                let type = trim(readfile(dir + '/type') || '');
                                if (temp != null)
                                        push(zones, { type: type, temp: temp / 1000.0 });
                        }
                        result.thermal_zones = zones;

                        // ── MCU Status via ubus ──
                        let mcu = ubus.call('photonicat', 'status');
                        if (mcu) {
                                result.board_temp = mcu.board_temp;
                                result.fan_rpm = mcu.fan_rpm;
                                result.battery = {
                                        status:   mcu.battery.capacity > 0 ? (mcu.charger.online ? 'Charging' : 'Discharging') : 'Unknown',
                                        voltage:  mcu.battery.voltage / 1000.0,
                                        current:  mcu.battery.current / 1000.0,
                                        capacity: mcu.battery.capacity
                                };
                                result.charger = {
                                        online: mcu.charger.online
                                };
                                if (mcu.battery.capacity == 100 && mcu.charger.online)
                                        result.battery.status = 'Full';
                        }

                        // ── CPU policies ──
                        let cpu = {};
                        let policies = glob('/sys/devices/system/cpu/cpufreq/policy*');
                        for (let dir in policies) {
                                let parts = split(dir, '/');
                                let name = parts[length(parts) - 1];
                                cpu[name] = {
                                        governor:  trim(readfile(dir + '/scaling_governor') || ''),
                                        governors: split(trim(readfile(dir + '/scaling_available_governors') || ''), ' '),
                                        cur_freq:  (read_int(dir + '/scaling_cur_freq') || 0) / 1000.0,
                                        max_freq:  (read_int(dir + '/scaling_max_freq') || 0) / 1000.0,
                                        min_freq:  (read_int(dir + '/scaling_min_freq') || 0) / 1000.0
                                };
                        }
                        result.cpu = cpu;

                        // ── Fan config from UCI ──
                        let uci = cursor();
                        uci.load('photonicat');
                        result.fan_config = {
                                mode:         uci.get('photonicat', 'fan', 'mode') || 'auto',
                                manual_level: +(uci.get('photonicat', 'fan', 'manual_level') || '3'),
                                min_temp:     +(uci.get('photonicat', 'fan', 'min_temp') || '45'),
                                max_temp:     +(uci.get('photonicat', 'fan', 'max_temp') || '85'),
                                hysteresis:   +(uci.get('photonicat', 'fan', 'hysteresis') || '3')
                        };
                        uci.unload();

                        return result;
                }
        },

        set_fan_level: {
                args: { level: 0 },
                call: function(req) {
                        let level = +req.args.level;
                        if (level < 0 || level > 9)
                                return { error: 'Invalid level: must be 0-9' };

                        ubus.call('photonicat', 'set_fan', { level: level });

                        let uci = cursor();
                        uci.load('photonicat');
                        uci.set('photonicat', 'fan', 'manual_level', '' + level);
                        uci.commit('photonicat');
                        uci.unload();
                        return { success: true };
                }
        },

        // ... rest of the methods can stay mostly the same or be adapted ...
};

// Simplified return for brevity in this example, but I should keep the full file structure.
// I'll read the full file again to make sure I don't break other methods.
