/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::lib_utils {

namespace detail {

// Out-of-line definition shared by every described struct/map serialization, so the
// boost::json::object insertion machinery is emitted once instead of being inlined
// into each describe_to_json<T> instantiation.
void json_object_set(boost::json::object &obj, boost::json::string_view name, boost::json::value value)
{
    obj[name] = std::move(value);
}

} // namespace detail

// Single out-of-line instantiation of the std::string specialization shared by every
// translation unit (see the matching `extern template` declarations in the header).
template boost::json::value describe_to_json<std::string>(const std::string &value);
template bool describe_from_json<std::string>(const boost::json::value &j, std::string &value);

} // namespace esp_brookesia::lib_utils
