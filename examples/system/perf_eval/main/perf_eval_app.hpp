/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <atomic>
#include <expected>
#include <string>

#include "brookesia/system_core/app/iapp.hpp"

namespace esp_brookesia {
namespace perf_eval {

// v1.8 section 2.2 App C/D require the binding path to be driven the way a real
// product drives it: through an installed app whose GUI document the System binding
// queue resolves via record.document_id. The carrier's own load_json document has no
// app record, so binding pushes against it are silently dropped by flush. This app
// owns a separate document loaded from the same embedded root.json, mounted on the
// /debug screen, and exposes its running state to the harness once started.
class PerfEvalApp : public esp_brookesia::system::core::IApp {
public:
    PerfEvalApp() = default;
    ~PerfEvalApp() override = default;

    esp_brookesia::system::core::AppManifest get_manifest() const override;
    esp_brookesia::system::core::AppGuiDescriptor get_gui_descriptor() const override;

    std::expected<void, std::string> on_start(
        esp_brookesia::system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(
        esp_brookesia::system::core::AppContext &context) override;

    bool is_running() const
    {
        return running_.load(std::memory_order_acquire);
    }

    void set_document(const std::string &root_json, const std::string &resource_dir)
    {
        root_json_ = root_json;
        resource_dir_ = resource_dir;
    }

private:
    std::atomic<bool> running_{false};
    std::string root_json_;
    std::string resource_dir_;
};

} // namespace perf_eval
} // namespace esp_brookesia
