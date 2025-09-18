/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

bool system_init();

bool system_check_is_developer_mode();

void restart_usb_serial_jtag();
