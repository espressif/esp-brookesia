/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#ifndef _IMU_GESTURE_H_
#define _IMU_GESTURE_H_

#include <string>
#include "esp_err.h"
#include "boost/signals2/signal.hpp"
#include "boost/thread.hpp"
#include "freertos/event_groups.h"
#include "bmi270.h"


/**
 * @brief IMU手势检测类
 * 使用BMI270传感器实现手势检测功能
 */
class IMUGesture {
public:
    IMUGesture();
    ~IMUGesture();

    enum GestureType {
        UNKNOWN_GESTURE = 0,
        ANY_MOTION,
        CIRCLE_CLOCKWISE,
        CIRCLE_ANTICLOCKWISE,
    };

    /**
     * @brief 初始化IMU手势检测
     * @return true if success, false if failed
     */
    bool init();

    /**
     * @brief 手势事件信号
     * 当检测到手势时触发
     */
    boost::signals2::signal<void(GestureType type)> gesture_signal;

private:
    static void IRAM_ATTR gpioIsrHandler(void *arg);
    void gestureThread(void *arg);
    int8_t bmi2SetConfig(bmi2_dev *dev);
    int8_t setWristGestureConfig(bmi2_dev *dev);
    int8_t setAnyMotionConfig(bmi2_dev *bmi2_dev);
    int8_t setCircleGestureConfig(bmi2_dev *bmi2_dev);

    i2c_bus_handle_t i2c_bus_;
    bmi270_handle_t bmi_handle_;
    EventGroupHandle_t event_group_;
    boost::thread gesture_thread_;
};

#endif
