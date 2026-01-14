/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service::helper {

class AgentOpenai {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr std::string_view NAME = "Openai";

    struct Info {
        std::string model;
        std::string api_key;
    };
};

BROOKESIA_DESCRIBE_STRUCT(AgentOpenai::Info, (), (model, api_key));

} // namespace esp_brookesia::service::helper
