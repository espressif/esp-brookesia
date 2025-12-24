/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "service_manager/macro_configs.h"
#include "service_manager/common.hpp"
/* Event */
#include "service_manager/event/dispatcher.hpp"
#include "service_manager/event/definition.hpp"
#include "service_manager/event/registry.hpp"
/* Function */
#include "service_manager/function/definition.hpp"
#include "service_manager/function/registry.hpp"
/* Service */
#include "service_manager/service/base.hpp"
#include "service_manager/service/manager.hpp"
#include "service_manager/service/local_runner.hpp"
/* RPC */
#include "service_manager/rpc/data_link_base.hpp"
#include "service_manager/rpc/data_link_client.hpp"
#include "service_manager/rpc/data_link_server.hpp"
#include "service_manager/rpc/server.hpp"
#include "service_manager/rpc/connection.hpp"
#include "service_manager/rpc/client.hpp"
