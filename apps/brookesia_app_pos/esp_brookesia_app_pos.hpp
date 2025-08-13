/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "esp_timer.h"
#include <atomic>
#include <string>

namespace esp_brookesia::apps {

typedef enum {
    POS_SCREEN_S1,      // Welcome screen
    POS_SCREEN_S2,      // Input amount
    POS_SCREEN_S3,      // Select payment method
    POS_SCREEN_S4,      // Process payment
    POS_SCREEN_S5,      // Payment result
    POS_SCREEN_MAX,
} pos_screen_t;

typedef enum {
    POS_STATE_IDLE,
    POS_STATE_AMOUNT_INPUT,
    POS_STATE_PAYMENT_METHOD,
    POS_STATE_PROCESSING,
    POS_STATE_SUCCESS,
    POS_STATE_FAILED,
} pos_state_t;

typedef struct {
    double amount;
    pos_state_t state;
    std::string payment_method;
    uint64_t transaction_id;
} pos_info_t;

class Pos: public systems::speaker::App {
public:
    /**
     * @brief Get the singleton instance of Pos
     *
     * @return Pointer to the singleton instance
     */
    static Pos *requestInstance();

    /**
     * @brief Destructor for the pos app
     *
     */
    ~Pos();

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;

    using App::startRecordResource;
    using App::endRecordResource;

protected:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     */
    Pos();

private:
    static void pos_event_cb(lv_event_t *e);
    static void textarea_event_cb(lv_event_t *e);
    static void btnm_event_cb(lv_event_t *e);
    static void next_button_event_cb(lv_event_t *e);
    static void continue_button_event_cb(lv_event_t *e);
    static void panel_list_bg_event_cb(lv_event_t *e);
    static void qr_code_event_cb(lv_event_t *e);
    static void cancel_button_event_cb(lv_event_t *e);
    static void auto_advance_callback(void *arg);
    static void toast_timer_callback(void *arg);

    void createUI();
    void switchToScreen(pos_screen_t screen);
    void showToast(const char *message, uint32_t duration_ms = 3000);
    void hideToast();
    void processPayment();
    void resetPosState();
    const char *getStateString(pos_state_t state);
    bool isValidStateTransition(pos_state_t from, pos_state_t to);
    void updateState(pos_state_t new_state);

    // UI element pointers
    lv_obj_t *main_container;
    lv_obj_t *custom_keyboard;  // Custom keyboard

    // Screen related
    pos_screen_t current_screen;
    pos_info_t pos_info;
    uint16_t _height = 400;
    uint16_t _width = 400;

    std::atomic<bool> _is_starting = false;
    std::atomic<bool> _is_stopping = false;
    static Pos *_instance;

    esp_timer_handle_t _auto_advance_timer = nullptr;
    esp_timer_handle_t _toast_timer = nullptr;

    lv_obj_t *_toast_container = nullptr;
    lv_obj_t *_toast_label = nullptr;
};

} // namespace esp_brookesia::apps
