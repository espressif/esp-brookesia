/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include "doa_angle_control.h"

#define FRAME_HEADER_1  0xAA
#define FRAME_HEADER_2  0x55
#define CMD_SET_ANGLE   0x01

static const char *TAG = "DOA_Angle";

static void build_angle_frame(uint16_t angle, uint8_t frame[8]);

__attribute__((weak))
esp_err_t doa_angle_control_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(UART_NUM_1, 512 * 2, 512 * 2, 20, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, DOA_ANGLE_CONTROL_UART_TX, DOA_ANGLE_CONTROL_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    return ESP_OK;
}

__attribute__((weak))
esp_err_t doa_angle_control_set_angle(int angle)
{
    ESP_LOGI(TAG, "Set DOA angle: %d", angle);
    uint8_t frame[8];
    build_angle_frame(angle, frame);
    uart_write_bytes(UART_NUM_1, (const char *)frame, sizeof(frame));

    return ESP_OK;
}

static void build_angle_frame(uint16_t angle, uint8_t frame[8])
{
    uint8_t data_high = (angle >> 8) & 0xFF;
    uint8_t data_low  = angle & 0xFF;

    uint8_t length = 3; // 命令码(1) + 数据(2)

    frame[0] = FRAME_HEADER_1;
    frame[1] = FRAME_HEADER_2;
    frame[2] = 0x00;          // 长度高字节（固定为0）
    frame[3] = length;        // 长度低字节
    frame[4] = CMD_SET_ANGLE; // 命令码
    frame[5] = data_high;     // 数据高字节
    frame[6] = data_low;      // 数据低字节
    frame[7] = CMD_SET_ANGLE + data_high + data_low; // 校验和
}
