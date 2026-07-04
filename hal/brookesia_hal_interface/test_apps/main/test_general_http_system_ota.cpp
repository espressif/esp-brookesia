/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "unity.h"
#include "common.hpp"
#include "brookesia/hal_interface/device.hpp"

using namespace esp_brookesia;

namespace {

class TestHttpTransaction: public hal::network::HttpClientIface::Transaction {
public:
    hal::network::HttpClientIface::ErrorCode open(
        const hal::network::HttpClientIface::Request &request,
        hal::network::HttpClientIface::Response &response,
        uint32_t &content_length
    ) override
    {
        if (request.url.empty()) {
            error_message_ = "URL is empty";
            return hal::network::HttpClientIface::ErrorCode::InvalidArgument;
        }

        response.status_code = 200;
        response.headers = {{"content-type", "text/plain"}};
        response.error = hal::network::HttpClientIface::ErrorCode::Ok;
        response.error_message.clear();
        content_length = body_.size();
        offset_ = 0;
        canceled_ = false;
        return hal::network::HttpClientIface::ErrorCode::Ok;
    }

    int read(char *buffer, size_t size, std::string &error_message) override
    {
        if (canceled_) {
            error_message = "Canceled";
            return -1;
        }
        if (buffer == nullptr || size == 0) {
            return 0;
        }

        const auto remaining = body_.size() - offset_;
        const auto read_size = std::min(remaining, size);
        std::memcpy(buffer, body_.data() + offset_, read_size);
        offset_ += read_size;
        error_message.clear();
        return static_cast<int>(read_size);
    }

    bool is_complete() const override
    {
        return canceled_ || offset_ >= body_.size();
    }

    void cancel() override
    {
        canceled_ = true;
    }

private:
    std::string body_ = "mock response";
    std::string error_message_;
    size_t offset_ = 0;
    bool canceled_ = false;
};

class TestHttpClientIface: public hal::network::HttpClientIface {
public:
    static constexpr const char *NAME = "TestGeneralPlatform:HTTP";

    std::shared_ptr<Transaction> create_transaction() override
    {
        return std::make_shared<TestHttpTransaction>();
    }
};

class TestOtaUpdaterIface: public hal::system::OtaUpdaterIface {
public:
    static constexpr const char *NAME = "TestGeneralPlatform:SystemOTA";

    TargetIdentity get_target_identity(const std::string &system) override
    {
        return {
            .system = system,
            .chip = CONFIG_IDF_TARGET,
            .board = "ESP-Test-Board",
        };
    }

    std::string get_running_firmware_version() override
    {
        return "9.9.9";
    }

    std::string get_running_boot_partition_label() override
    {
        return "ota_0";
    }

    bool get_next_update_partition_label(std::string &label) override
    {
        label = "ota_1";
        return true;
    }

    bool write_firmware(
        const std::string &staged_path,
        FirmwareProgressCallback progress_cb,
        std::string &boot_partition
    ) override
    {
        if (staged_path.empty()) {
            last_error_ = "Staged path is empty";
            return false;
        }
        if (progress_cb && (!progress_cb(0, 4) || !progress_cb(4, 4))) {
            last_error_ = "Write canceled";
            return false;
        }
        boot_partition = "ota_1";
        last_error_.clear();
        return true;
    }

    bool confirm_boot() override
    {
        return true;
    }
};

class TestGeneralPlatformDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestGeneralPlatform";

    TestGeneralPlatformDevice()
        : hal::Device(NAME)
    {
    }

    bool probe() override
    {
        return true;
    }

    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {
            {hal::network::HttpClientIface::NAME, TestHttpClientIface::NAME},
            {hal::system::OtaUpdaterIface::NAME, TestOtaUpdaterIface::NAME},
        };
    }

protected:
    bool on_init() override
    {
        interfaces_.emplace(TestHttpClientIface::NAME, std::make_shared<TestHttpClientIface>());
        interfaces_.emplace(TestOtaUpdaterIface::NAME, std::make_shared<TestOtaUpdaterIface>());
        return true;
    }

    void on_deinit() override
    {
        interfaces_.erase(TestHttpClientIface::NAME);
        interfaces_.erase(TestOtaUpdaterIface::NAME);
    }
};

} // namespace

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestGeneralPlatformDevice, std::string(TestGeneralPlatformDevice::NAME));

TEST_CASE("network::HttpClientIface: mock transaction lifecycle", "[hal][interface]")
{
    auto iface_handle = hal::acquire_first_interface<hal::network::HttpClientIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(iface_handle));
    TEST_ASSERT_EQUAL_STRING(TestHttpClientIface::NAME, std::string(iface_handle.instance_name()).c_str());

    auto transaction = iface_handle->create_transaction();
    TEST_ASSERT_TRUE(static_cast<bool>(transaction));

    hal::network::HttpClientIface::Response response;
    uint32_t content_length = 0;
    hal::network::HttpClientIface::Request request;
    request.url = "http://localhost/mock";
    request.method = hal::network::HttpClientIface::Method::Get;
    const auto open_result = transaction->open(request, response, content_length);
    TEST_ASSERT_EQUAL(
        static_cast<int>(hal::network::HttpClientIface::ErrorCode::Ok),
        static_cast<int>(open_result)
    );
    TEST_ASSERT_EQUAL_UINT32(13, content_length);
    TEST_ASSERT_EQUAL_INT(200, response.status_code);

    char buffer[16] = {};
    std::string error_message;
    const int read_size = transaction->read(buffer, sizeof(buffer), error_message);
    TEST_ASSERT_EQUAL_INT(13, read_size);
    TEST_ASSERT_EQUAL_STRING("mock response", buffer);
    TEST_ASSERT_TRUE(transaction->is_complete());
}

TEST_CASE("system::OtaUpdaterIface: mock platform operations", "[hal][interface]")
{
    auto iface_handle = hal::acquire_first_interface<hal::system::OtaUpdaterIface>();
    TEST_ASSERT_TRUE(static_cast<bool>(iface_handle));
    TEST_ASSERT_EQUAL_STRING(TestOtaUpdaterIface::NAME, std::string(iface_handle.instance_name()).c_str());

    const auto identity = iface_handle->get_target_identity("test-system");
    TEST_ASSERT_EQUAL_STRING("test-system", identity.system.c_str());
    TEST_ASSERT_EQUAL_STRING(CONFIG_IDF_TARGET, identity.chip.c_str());
    TEST_ASSERT_EQUAL_STRING("ESP-Test-Board", identity.board.c_str());
    TEST_ASSERT_EQUAL_STRING("9.9.9", iface_handle->get_running_firmware_version().c_str());
    TEST_ASSERT_EQUAL_STRING("ota_0", iface_handle->get_running_boot_partition_label().c_str());

    std::string update_label;
    TEST_ASSERT_TRUE(iface_handle->get_next_update_partition_label(update_label));
    TEST_ASSERT_EQUAL_STRING("ota_1", update_label.c_str());

    std::vector<uint64_t> progress;
    std::string boot_partition;
    TEST_ASSERT_TRUE(iface_handle->write_firmware(
                         "/tmp/firmware.bin",
    [&progress](uint64_t written, uint64_t total) {
        progress.push_back(written + total);
        return true;
    },
    boot_partition
                     ));
    TEST_ASSERT_EQUAL_STRING("ota_1", boot_partition.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, progress.size());
    TEST_ASSERT_TRUE(iface_handle->confirm_boot());
}
