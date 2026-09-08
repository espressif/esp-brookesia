/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "brookesia/hal_interface/interfaces/video/camera.hpp"

namespace esp_brookesia::hal {

class VideoCameraDeviceSession;

class VideoCameraImpl: public video::CameraIface {
public:
    VideoCameraImpl() = default;
    ~VideoCameraImpl() override;

    std::vector<DeviceInfo> get_device_infos() const override;

private:
    mutable std::mutex discovery_mutex_;
    mutable std::unique_ptr<VideoCameraDeviceSession> pending_cleanup_session_;
};

class VideoCameraDeviceSession {
public:
    VideoCameraDeviceSession() = default;
    VideoCameraDeviceSession(const VideoCameraDeviceSession &) = delete;
    VideoCameraDeviceSession &operator=(const VideoCameraDeviceSession &) = delete;
    ~VideoCameraDeviceSession();

    static bool is_board_camera_declared();
    static bool get_declared_device_path(std::string &device_path);

    bool open(std::string &error_message);
    bool close(std::string *error_message = nullptr);

    const std::string &get_device_path() const
    {
        return device_path_;
    }

private:
    bool open_locked(std::string &error_message);
    bool close_locked(std::string *error_message);
    bool retain_expansion_runtime(std::string &error_message);
    void release_expansion_runtime();

    bool camera_initialized_ = false;
    bool expansion_runtime_retained_ = false;
    std::string device_path_;
};

} // namespace esp_brookesia::hal
