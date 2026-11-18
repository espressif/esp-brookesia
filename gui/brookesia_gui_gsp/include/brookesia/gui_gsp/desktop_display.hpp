/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>

namespace esp_brookesia::gui::gsp {

/** Linux renderer configuration shared by successive application sessions. */
class DesktopDisplay {
public:
    struct Config {
        std::string simulator;
        std::string work_dir;
        std::string dynamic_font;
        bool headless = false;
    };

    explicit DesktopDisplay(Config config);
    ~DesktopDisplay();
    DesktopDisplay(const DesktopDisplay &) = delete;
    DesktopDisplay &operator=(const DesktopDisplay &) = delete;

    /** Register the original compiled bytes and their matching GSPC schema-2 sidecar.
     * The renderer performs authoritative GSPB validation when a session starts. */
    std::expected<void, std::string> register_bundle(
        const std::string &bundle_path, const std::string &api_sidecar_path
    );
    /** Preview URL with a unique session fragment. Replace the preview document when it changes;
     * fragment-only navigation within an existing document does not reset the renderer frontend. */
    std::string browser_url() const;
    bool alive() const;
    std::string error() const;

    class Impl;
    Impl &implementation();

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::gui::gsp
