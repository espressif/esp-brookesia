/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/format.hpp"
#include "brookesia/hal_interface/interfaces/storage/key_value.hpp"

namespace esp_brookesia::hal {

class StorageKeyValueNvsImpl: public storage::KeyValueIface {
public:
    StorageKeyValueNvsImpl();
    ~StorageKeyValueNvsImpl() override;

    bool init() override;
    void deinit() override;
    bool list(const std::string &nspace, std::vector<EntryInfo> &entries) override;
    bool set(const std::string &nspace, const KeyValueMap &key_value_map) override;
    bool get(const std::string &nspace, const std::vector<std::string> &keys, KeyValueMap &key_value_map) override;
    bool erase(const std::string &nspace, const std::vector<std::string> &keys) override;

private:
    void set_last_error(const boost::format &format);
    void set_last_error(std::string error);
    bool get_one(const std::string &nspace, const std::string &key, Value &value);

    bool is_initialized_ = false;
};

} // namespace esp_brookesia::hal
