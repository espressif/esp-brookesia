/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <utility>

#include "esp_heap_caps.h"

#if defined(BROOKESIA_GUI_GSP_HOST_TEST)
#   include "sdk_stubs.hpp"
#   include "host_log_stubs.hpp"
#else
#   include "private/utils.hpp"
#endif

#include "brookesia/gui_gsp/app_host.hpp"
#include "brookesia/gui_gsp/bundle.hpp"

namespace esp_brookesia::gui::gsp {

namespace {

AppHost *s_current = nullptr;

} // namespace

class AppHost::Impl {
public:
    Session session{};
    std::shared_ptr<const Bundle> bundle{};
    std::byte *owned_bytes = nullptr;
    void *event_user_ctx = nullptr;
};

AppHost::AppHost()
    : impl_(std::make_unique<Impl>())
{
}

AppHost::~AppHost()
{
    if (running()) {
        BROOKESIA_LOGE("Destroying a GSP app host that is still running");
        std::terminate();
    }
}

std::expected<void, std::string> AppHost::start(
    std::span<const std::byte> bytes, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
)
{
    if (running()) {
        return std::unexpected("GSP app host is already running");
    }

    auto loaded = Bundle::load(bytes);
    if (!loaded) {
        return std::unexpected(loaded.error());
    }

    impl_->bundle = std::move(loaded.value());
    impl_->event_user_ctx = user_ctx;
    if (!impl_->session.start(impl_->bundle, display, on_event, user_ctx)) {
        if (impl_->session.running()) {
            s_current = this;
            return std::unexpected("GSP session event registration failed and the session is still running");
        }
        impl_->bundle.reset();
        impl_->event_user_ctx = nullptr;
        return std::unexpected("Failed to start GSP session");
    }
    s_current = this;
    return {};
}

std::expected<void, std::string> AppHost::start(
    const esp_gsp_config_t &config, const DisplayLease &display, Session::EventCallback on_event, void *user_ctx
)
{
    if (running()) {
        return std::unexpected("GSP app host is already running");
    }
    if ((config.bundle == nullptr) || (config.bundle_size == 0)) {
        return std::unexpected("GSP compiler config has no bundle");
    }
    auto loaded = Bundle::load(std::span<const std::byte>(
                                   reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size
                               ));
    if (loaded) {
        impl_->bundle = std::move(loaded.value());
        impl_->event_user_ctx = user_ctx;
        if (!impl_->session.start(impl_->bundle, display, on_event, user_ctx)) {
            if (impl_->session.running()) {
                s_current = this;
                return std::unexpected("GSP session event registration failed and the session is still running");
            }
            impl_->bundle.reset();
            impl_->event_user_ctx = nullptr;
            return std::unexpected("Failed to start GSP session");
        }
        s_current = this;
        return {};
    }

    // The embedded compiler output is not a self-describing deployable. Start
    // it in place. Copying the bytes breaks the generated directory tables.
    BROOKESIA_LOGW("GSP deployable open failed (%1%); using the compiler config in place", loaded.error());
    impl_->event_user_ctx = user_ctx;
    if (!impl_->session.start(config, display, on_event, user_ctx)) {
        if (impl_->session.running()) {
            s_current = this;
            return std::unexpected("GSP session event registration failed and the session is still running");
        }
        impl_->event_user_ctx = nullptr;
        return std::unexpected("Failed to start GSP session");
    }
    s_current = this;
    return {};
}

std::expected<void, std::string> AppHost::stop()
{
    if (!running()) {
        impl_->bundle.reset();
        impl_->event_user_ctx = nullptr;
        return {};
    }
    if (!impl_->session.stop()) {
        return std::unexpected("Failed to stop GSP session");
    }
    impl_->bundle.reset();
    if (impl_->owned_bytes != nullptr) {
        heap_caps_free(impl_->owned_bytes);
        impl_->owned_bytes = nullptr;
    }
    impl_->event_user_ctx = nullptr;
    if (s_current == this) {
        s_current = nullptr;
    }
    return {};
}

bool AppHost::running() const
{
    return (impl_ != nullptr) && impl_->session.running();
}

esp_gsp_handle_t AppHost::handle() const
{
    return running() ? impl_->session.handle() : nullptr;
}

AppHost *AppHost::current()
{
    return s_current;
}

const std::shared_ptr<const Bundle> &AppHost::bundle() const
{
    return impl_->bundle;
}

void *AppHost::event_user_ctx() const
{
    return impl_->event_user_ctx;
}

} // namespace esp_brookesia::gui::gsp
