/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lib_utils.h"
#include "bsp/esp-bsp.h"
#include "imu_gesture.h"
#include "common/common.h"

static const char *TAG = "IMUGesture";

IMUGesture::IMUGesture()
    : i2c_bus_(nullptr)
    , bmi_handle_(nullptr)
    , event_group_(nullptr)
{
}

IMUGesture::~IMUGesture()
{
    if (gesture_thread_.joinable()) {
        gesture_thread_.interrupt();
        gesture_thread_.join();
    }

    if (event_group_) {
        vEventGroupDelete(event_group_);
    }

    if (bmi_handle_) {
        bmi270_sensor_del(bmi_handle_);
    }

    if (i2c_bus_) {
        i2c_bus_delete(&i2c_bus_);
    }
}

void IRAM_ATTR IMUGesture::gpioIsrHandler(void *arg)
{
    IMUGesture *self = static_cast<IMUGesture *>(arg);

    BaseType_t higher_priority_task_woken = pdFALSE;
    xEventGroupSetBitsFromISR(self->event_group_, BIT0, &higher_priority_task_woken);
    if (higher_priority_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void IMUGesture::gestureThread(void *arg)
{
    bmi270_handle_t bmi_handle = (bmi270_handle_t)arg;
    uint16_t int_status = 0;

    while (true) {
        xEventGroupWaitBits(event_group_, BIT0, pdTRUE, pdTRUE, portMAX_DELAY);

        bmi2_get_int_status(&int_status, bmi_handle);
        if (int_status & BMI270_ANY_MOT_STATUS_MASK) {
            ESP_LOGI(TAG, "Any-motion interrupt is generated");
            gesture_signal(ANY_MOTION);
            continue;
        }
    }
}


int8_t IMUGesture::setAnyMotionConfig(bmi2_dev *bmi2_dev)
{
    int8_t rslt = BMI2_OK;
    uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_ANY_MOTION };
    struct bmi2_sens_int_config sens_int = { .type = BMI2_ANY_MOTION, .hw_int_pin = BMI2_INT1 };

    rslt = bmi270_sensor_enable(sens_list, 2, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        {
            struct bmi2_sens_config config;
            struct bmi2_int_pin_config pin_config = { 0 };
            config.type = BMI2_ANY_MOTION;

            rslt = bmi270_get_sensor_config(&config, 1, bmi2_dev);
            bmi2_error_codes_print_result(rslt);

            rslt = bmi2_get_int_pin_config(&pin_config, bmi2_dev);
            bmi2_error_codes_print_result(rslt);

            if (rslt == BMI2_OK) {
                /* NOTE: The user can change the following configuration parameters according to their requirement. */
                /* 1LSB equals 20ms. Default is 100ms*/
                config.cfg.any_motion.duration = 50;
                /* 1LSB equals to 0.48mg. Default is 83mg */
                config.cfg.any_motion.threshold = 1000;

                /* Set new configurations. */
                rslt = bmi270_set_sensor_config(&config, 1, bmi2_dev);
                bmi2_error_codes_print_result(rslt);

                /* Interrupt pin configuration */
                pin_config.pin_type = BMI2_INT1;
                pin_config.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
                pin_config.pin_cfg[0].lvl = BMI2_INT_ACTIVE_LOW;
                pin_config.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
                pin_config.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
                pin_config.int_latch = BMI2_INT_NON_LATCH;

                rslt = bmi2_set_int_pin_config(&pin_config, bmi2_dev);
                bmi2_error_codes_print_result(rslt);
            }
        }

        if (rslt == BMI2_OK) {
            /* Map the feature interrupt for any-motion. */
            rslt = bmi270_map_feat_int(&sens_int, 1, bmi2_dev);
            bmi2_error_codes_print_result(rslt);
        }
    }
    return rslt;
}

bool IMUGesture::init()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    int8_t rslt;
    gpio_config_t io_conf;
    bmi270_i2c_config_t bmi_conf;

    // I2C配置和初始化
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 400000},
        .clk_flags = 0,
    };

    i2c_bus_ = i2c_bus_create(I2C_NUM_0, &i2c_conf);
    if (!i2c_bus_) {
        ESP_LOGE(TAG, "i2c bus create failed");
        goto err;
    }

    // BMI270初始化
    bmi_conf = {
        .i2c_handle = i2c_bus_,
        .i2c_addr = BMI270_I2C_ADDRESS,
    };

    if (bmi270_sensor_create(&bmi_conf, &bmi_handle_) != ESP_OK) {
        ESP_LOGE(TAG, "bmi270 create failed");
        goto err_i2c;
    }

    // 事件组创建
    event_group_ = xEventGroupCreate();
    if (!event_group_) {
        ESP_LOGE(TAG, "event group create failed");
        goto err_bmi;
    }

    // GPIO中断配置
    io_conf = {
        .pin_bit_mask = (1ULL << BSP_IMU_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);

    if (gpio_isr_handler_add(BSP_IMU_INT, gpioIsrHandler, this) != ESP_OK) {
        ESP_LOGE(TAG, "isr handler add failed");
        gpio_uninstall_isr_service();
        goto err_event;
    }

    // 配置手势识别
    rslt = setAnyMotionConfig(bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "gesture config failed");
        gpio_isr_handler_remove(BSP_IMU_INT);
        gpio_uninstall_isr_service();
        goto err_event;
    }

    // 启动手势检测线程
    {
        esp_utils::thread_config_guard thread_config(esp_utils::ThreadConfig{
            .name = "gesture_thread",
            .stack_size = 1024 * 5,
            .stack_in_ext = true,
        });
        try {
            gesture_thread_ = boost::thread(&IMUGesture::gestureThread, this, bmi_handle_);
        } catch (const std::exception &e) {
            ESP_LOGE(TAG, "Failed to create gesture thread: %s", e.what());
            gpio_isr_handler_remove(BSP_IMU_INT);
            gpio_uninstall_isr_service();
            goto err_event;
        }
    }

    ESP_LOGI(TAG, "Gesture detection started");
    return true;

err_event:
    vEventGroupDelete(event_group_);
    event_group_ = nullptr;
err_bmi:
    bmi270_sensor_del(bmi_handle_);
    bmi_handle_ = nullptr;
err_i2c:
    i2c_bus_delete(&i2c_bus_);
err:
    return false;
}
