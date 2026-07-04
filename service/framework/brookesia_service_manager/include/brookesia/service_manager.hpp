/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @brief Convenience umbrella header for the public service-manager API.
 */

#include "service_manager/macro_configs.h"
#include "service_manager/common.hpp"
/* Event */
#include "service_manager/event/definition.hpp"
#include "service_manager/event/registry.hpp"
/* Function */
#include "service_manager/function/definition.hpp"
#include "service_manager/function/registry.hpp"
/* Service */
#include "service_manager/service/base.hpp"
#include "service_manager/service/manager.hpp"
#include "service_manager/service/local_runner.hpp"
/* Helper */
#include "service_manager/helper/base.hpp"
