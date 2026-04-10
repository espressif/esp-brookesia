/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * Test-only: inject one Light device into the current group so host UI screens
 * (e.g. Select Devices) have non-empty lists without cloud login.
 * Call after rm_device_group_manager_init().
 */
void rm_app_backend_layout_test_inject_minimal_home(void);
