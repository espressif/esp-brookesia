/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:POS"

#include <math.h>
#include <string>
#include <cstring>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_brookesia_app_pos.hpp"
#include "ui/ui.h"
#include "esp_lib_utils.h"

using namespace esp_brookesia::systems::speaker;

#define APP_NAME "POS"

// Custom digit keyboard mapping
static const char *custom_digit_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    LV_SYMBOL_BACKSPACE, "0", ".", LV_SYMBOL_NEW_LINE, ""
};

LV_IMG_DECLARE(img_app_pos);

#define POS_AUTO_ADVANCE_INTERVAL_MS    3000

namespace esp_brookesia::apps {

Pos *Pos::_instance = nullptr;

Pos *Pos::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new Pos();
    }
    return _instance;
}

Pos::Pos():
    App( {
    .name = APP_NAME,
    .launcher_icon = gui::StyleImage::IMAGE(&img_app_pos),
    .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
    .flags = {
        .enable_default_screen = 0,
        .enable_recycle_resource = 1,
        .enable_resize_visual_area = 1,
    },
},
{
    .app_launcher_page_index = 0,
    .flags = {
        .enable_navigation_gesture = 1,
    },
}
   ),
   main_container(nullptr),
   custom_keyboard(nullptr),
   current_screen(POS_SCREEN_S1)
{
    // Initialize POS info
    pos_info.amount = 0.0;
    pos_info.state = POS_STATE_IDLE;
    pos_info.payment_method = "";
    pos_info.transaction_id = 0;
}

Pos::~Pos()
{
    // Clean up toast to prevent memory leaks
    hideToast();

    if (_auto_advance_timer) {
        esp_timer_stop(_auto_advance_timer);
        esp_timer_delete(_auto_advance_timer);
        _auto_advance_timer = nullptr;
    }

    if (_toast_timer) {
        esp_timer_stop(_toast_timer);
        esp_timer_delete(_toast_timer);
        _toast_timer = nullptr;
    }

    _instance = nullptr;
}

bool Pos::init(void)
{
    return true;
}

bool Pos::deinit(void)
{
    return true;
}

bool Pos::run(void)
{
    _is_starting = true;
    current_screen = POS_SCREEN_S1;

    try {
        createUI();
    } catch (...) {
        ESP_UTILS_LOGE("POS app initialization failed");
        _is_starting = false;
        return false;
    }

    _is_starting = false;
    return true;
}

bool Pos::back(void)
{
    if (current_screen == POS_SCREEN_S1) {
        ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");
        return true;
    } else {
        // Return to previous screen
        if (current_screen > POS_SCREEN_S1) {
            switchToScreen((pos_screen_t)(current_screen - 1));
        }
        return true;
    }
}

bool Pos::close(void)
{
    _is_stopping = true;

    hideToast();

    if (_auto_advance_timer) {
        esp_timer_stop(_auto_advance_timer);
    }

    // Clean up custom keyboard
    if (custom_keyboard) {
        if (lv_obj_is_valid(custom_keyboard)) {
            lv_obj_del(custom_keyboard);
        }
        custom_keyboard = nullptr;
    }

    // No need to manually clean screens due to enable_recycle_resource
    main_container = nullptr;

    _is_stopping = false;
    return true;
}

void Pos::createUI()
{
    try {
        // Initialize all screens at startup
        ui_Screen_POS_S1_screen_init();
        ui_Screen_POS_S2_screen_init();
        ui_Screen_POS_S3_screen_init();
        ui_Screen_POS_S4_screen_init();
        ui_Screen_POS_S5_screen_init();

        // Add event callbacks to all screens once at initialization
        if (ui_POS_S1_Panel_BTN_Next) {
            lv_obj_add_event_cb(ui_POS_S1_Panel_BTN_Next, next_button_event_cb, LV_EVENT_CLICKED, this);
        }

        if (ui_POS_S2_Textarea_TextArea1) {
            lv_obj_add_event_cb(ui_POS_S2_Textarea_TextArea1, textarea_event_cb, LV_EVENT_CLICKED, this);
        }
        if (ui_POS_S2_Panel_BTN_Continue) {
            lv_obj_add_event_cb(ui_POS_S2_Panel_BTN_Continue, continue_button_event_cb, LV_EVENT_CLICKED, this);
        }

        if (ui_POS_S3_Panel_Panel_List_Bg) {
            lv_obj_add_event_cb(ui_POS_S3_Panel_Panel_List_Bg, panel_list_bg_event_cb, LV_EVENT_CLICKED, this);
        }

        if (ui_POS_S4_Image_IMG_QR_Code) {
            lv_obj_add_event_cb(ui_POS_S4_Image_IMG_QR_Code, qr_code_event_cb, LV_EVENT_CLICKED, this);
        }
        if (ui_POS_S4_Label_Label_Cancel) {
            lv_obj_add_event_cb(ui_POS_S4_Label_Label_Cancel, cancel_button_event_cb, LV_EVENT_CLICKED, this);
        }

        if (ui_POS_S5_Panel_BTN_Next_2) {
            lv_obj_add_event_cb(ui_POS_S5_Panel_BTN_Next_2, next_button_event_cb, LV_EVENT_CLICKED, this);
        }

        // Switch to initial screen
        switchToScreen(current_screen);

    } catch (...) {
        ESP_UTILS_LOGE("POS UI creation failed");
    }
}

void Pos::switchToScreen(pos_screen_t screen)
{
    if (_is_stopping || screen >= POS_SCREEN_MAX) {
        return;
    }

    hideToast();

    // Clean up custom keyboard if switching away from S2
    if (current_screen == POS_SCREEN_S2 && custom_keyboard) {
        if (lv_obj_is_valid(custom_keyboard)) {
            lv_obj_del(custom_keyboard);
        }
        custom_keyboard = nullptr;
    }

    // Switch to target screen
    current_screen = screen;

    // Simply load the target screen without destroying anything
    switch (current_screen) {
    case POS_SCREEN_S1:
        main_container = ui_Screen_POS_S1;
        lv_scr_load(ui_Screen_POS_S1);
        // Clear pos info
        resetPosState();
        break;

    case POS_SCREEN_S2:
        main_container = ui_Screen_POS_S2;
        lv_scr_load(ui_Screen_POS_S2);
        // Clear textarea
        lv_textarea_set_text(ui_POS_S2_Textarea_TextArea1, "");
        // Update state to amount input
        updateState(POS_STATE_AMOUNT_INPUT);
        // Create custom digit keyboard for S2 screen
        if (!custom_keyboard) {
            custom_keyboard = lv_buttonmatrix_create(ui_Screen_POS_S2);
            lv_buttonmatrix_set_map(custom_keyboard, custom_digit_map);
            lv_obj_set_width(custom_keyboard, lv_pct(80));
            lv_obj_set_height(custom_keyboard, lv_pct(40));
            lv_obj_set_x(custom_keyboard, 0);
            lv_obj_set_y(custom_keyboard, -50);
            lv_obj_set_align(custom_keyboard, LV_ALIGN_BOTTOM_MID);
            lv_obj_set_style_blend_mode(custom_keyboard, LV_BLEND_MODE_NORMAL, LV_PART_MAIN | LV_STATE_DEFAULT);

            // Add event handler for custom keyboard
            lv_obj_add_event_cb(custom_keyboard, btnm_event_cb, LV_EVENT_VALUE_CHANGED, this);
            lv_obj_remove_flag(custom_keyboard, LV_OBJ_FLAG_CLICK_FOCUSABLE); // Keep textarea focus
        }
        break;

    case POS_SCREEN_S3:
        main_container = ui_Screen_POS_S3;
        lv_scr_load(ui_Screen_POS_S3);
        // Update state to payment method selection
        updateState(POS_STATE_PAYMENT_METHOD);
        break;

    case POS_SCREEN_S4:
        main_container = ui_Screen_POS_S4;
        lv_scr_load(ui_Screen_POS_S4);
        // Update amount label with current value
        if (ui_POS_S4_Label_Label_Amount_Pirce2) {
            lv_label_set_text_fmt(ui_POS_S4_Label_Label_Amount_Pirce2, "$ %.2f", pos_info.amount);
        }
        // Re-enable QR code clickability for payment
        if (ui_POS_S4_Image_IMG_QR_Code) {
            lv_obj_add_flag(ui_POS_S4_Image_IMG_QR_Code, LV_OBJ_FLAG_CLICKABLE);
        }
        break;

    case POS_SCREEN_S5:
        main_container = ui_Screen_POS_S5;
        lv_scr_load(ui_Screen_POS_S5);
        // Update display based on payment state
        if (ui_POS_S5_Label_Label_Succesful) {
            if (pos_info.state == POS_STATE_SUCCESS) {
                lv_label_set_text(ui_POS_S5_Label_Label_Succesful, "Payment Successful");
                lv_obj_set_style_text_color(ui_POS_S5_Label_Label_Succesful, lv_color_hex(0x00FF00), 0);
            } else if (pos_info.state == POS_STATE_FAILED) {
                lv_label_set_text(ui_POS_S5_Label_Label_Succesful, "Payment Failed");
                lv_obj_set_style_text_color(ui_POS_S5_Label_Label_Succesful, lv_color_hex(0xFF0000), 0);
            }
        }
        // Update continue message based on state
        if (ui_POS_S5_Label_Label_press_contimue) {
            if (pos_info.state == POS_STATE_SUCCESS) {
                lv_label_set_text(ui_POS_S5_Label_Label_press_contimue, "Press continue to start new transaction");
            } else if (pos_info.state == POS_STATE_FAILED) {
                lv_label_set_text(ui_POS_S5_Label_Label_press_contimue, "Press continue to retry or start new transaction");
            }
        }
        break;

    default:
        ESP_UTILS_LOGE("Unknown POS screen type: %d", current_screen);
        return;
    }
}

void Pos::processPayment()
{
    if (!_auto_advance_timer) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = auto_advance_callback;
        timer_args.arg = this;
        timer_args.name = "pos_auto_advance";

        esp_err_t ret = esp_timer_create(&timer_args, &_auto_advance_timer);
        if (ret != ESP_OK) {
            ESP_UTILS_LOGE("Auto advance timer create failed: %s", esp_err_to_name(ret));
            return;
        }
    }

    // Simulate payment processing time
    esp_timer_start_once(_auto_advance_timer, POS_AUTO_ADVANCE_INTERVAL_MS * 1000);

    // Update state
    updateState(POS_STATE_PROCESSING);
    pos_info.transaction_id = esp_timer_get_time() / 1000; // Use timestamp as transaction ID
    ESP_UTILS_LOGI("Transaction ID: %llu", pos_info.transaction_id);

    showToast("Processing payment...", POS_AUTO_ADVANCE_INTERVAL_MS);
}

void Pos::resetPosState()
{
    pos_state_t old_state = pos_info.state;
    pos_info.amount = 0.0;
    pos_info.state = POS_STATE_IDLE;
    pos_info.payment_method = "";
    pos_info.transaction_id = 0;
    ESP_UTILS_LOGI("State reset: %s -> %s", getStateString(old_state), getStateString(pos_info.state));
}

const char *Pos::getStateString(pos_state_t state)
{
    switch (state) {
    case POS_STATE_IDLE: return "IDLE";
    case POS_STATE_AMOUNT_INPUT: return "AMOUNT_INPUT";
    case POS_STATE_PAYMENT_METHOD: return "PAYMENT_METHOD";
    case POS_STATE_PROCESSING: return "PROCESSING";
    case POS_STATE_SUCCESS: return "SUCCESS";
    case POS_STATE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

bool Pos::isValidStateTransition(pos_state_t from, pos_state_t to)
{
    // Define valid state transitions
    switch (from) {
    case POS_STATE_IDLE:
        return (to == POS_STATE_AMOUNT_INPUT);
    case POS_STATE_AMOUNT_INPUT:
        return (to == POS_STATE_PAYMENT_METHOD || to == POS_STATE_IDLE);
    case POS_STATE_PAYMENT_METHOD:
        return (to == POS_STATE_PROCESSING || to == POS_STATE_AMOUNT_INPUT || to == POS_STATE_IDLE);
    case POS_STATE_PROCESSING:
        return (to == POS_STATE_SUCCESS || to == POS_STATE_FAILED);
    case POS_STATE_SUCCESS:
    case POS_STATE_FAILED:
        return (to == POS_STATE_IDLE);
    default:
        return false;
    }
}

void Pos::updateState(pos_state_t new_state)
{
    pos_state_t old_state = pos_info.state;

    if (isValidStateTransition(old_state, new_state)) {
        pos_info.state = new_state;
        ESP_UTILS_LOGI("State transition: %s -> %s", getStateString(old_state), getStateString(new_state));
    } else {
        ESP_UTILS_LOGW("Invalid state transition: %s -> %s", getStateString(old_state), getStateString(new_state));
    }
}

void Pos::textarea_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        // Show custom keyboard when textarea is clicked
        if (pos->custom_keyboard && lv_obj_is_valid(pos->custom_keyboard)) {
            lv_obj_clear_flag(pos->custom_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void Pos::btnm_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *ta = ui_POS_S2_Textarea_TextArea1;

    // Check if objects are valid
    if (!lv_obj_is_valid(obj) || !lv_obj_is_valid(ta)) {
        return;
    }

    const char *txt = lv_buttonmatrix_get_button_text(obj, lv_buttonmatrix_get_selected_button(obj));

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_delete_char(ta);
    } else if (strcmp(txt, LV_SYMBOL_NEW_LINE) == 0) {
        // Confirm key, hide keyboard
        if (pos->custom_keyboard && lv_obj_is_valid(pos->custom_keyboard)) {
            lv_obj_add_flag(pos->custom_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_textarea_add_text(ta, txt);
    }
}

void Pos::next_button_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        // Switch to next screen when Next button is clicked
        pos->switchToScreen((pos_screen_t)((pos->current_screen + 1) % POS_SCREEN_MAX));
    }
}

void Pos::continue_button_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        // Validate input amount
        if (ui_POS_S2_Textarea_TextArea1 && lv_obj_is_valid(ui_POS_S2_Textarea_TextArea1)) {
            const char *amount_text = lv_textarea_get_text(ui_POS_S2_Textarea_TextArea1);

            if (amount_text && strlen(amount_text) > 0) {
                // Check if it's a valid number format
                bool is_valid = true;
                int decimal_count = 0;
                int digit_count = 0;

                for (int i = 0; amount_text[i] != '\0'; i++) {
                    if (amount_text[i] >= '0' && amount_text[i] <= '9') {
                        digit_count++;
                    } else if (amount_text[i] == '.') {
                        decimal_count++;
                        if (decimal_count > 1) {
                            is_valid = false;
                            break;
                        }
                    } else {
                        is_valid = false;
                        break;
                    }
                }

                // Check if there are digits and format is correct
                if (is_valid && digit_count > 0) {
                    // Convert to double for further validation
                    double amount = atof(amount_text);
                    if (amount > 0.0 && amount <= 999999.99) {
                        // Amount is valid, save and switch to next screen
                        pos->pos_info.amount = amount;
                        ESP_UTILS_LOGI("Valid amount: %.2f, switching to POS_SCREEN_S3", amount);
                        pos->switchToScreen(POS_SCREEN_S3);
                    } else {
                        // Amount out of range
                        pos->showToast("Amount must be between 0.01 and 999999.99", 3000);
                        ESP_UTILS_LOGW("Invalid amount range: %.2f", amount);
                    }
                } else {
                    // Invalid format
                    pos->showToast("Please enter a valid amount", 3000);
                    ESP_UTILS_LOGW("Invalid amount format: %s", amount_text);
                }
            } else {
                // Empty input
                pos->showToast("Please enter an amount", 3000);
                ESP_UTILS_LOGW("Empty amount input");
            }
        } else {
            // Invalid textarea
            pos->showToast("Input error", 3000);
            ESP_UTILS_LOGE("Textarea is invalid");
        }
    }
}

void Pos::panel_list_bg_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        // Set payment method (could be determined by which part was clicked)
        pos->pos_info.payment_method = "QR Code";
        ESP_UTILS_LOGI("Payment method selected: %s, switching to POS_SCREEN_S4", pos->pos_info.payment_method.c_str());
        pos->switchToScreen(POS_SCREEN_S4);
    }
}

void Pos::qr_code_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        ESP_UTILS_LOGI("QR Code clicked, processing payment");
        // Disable click to prevent repeated triggering
        lv_obj_remove_flag((lv_obj_t *)lv_event_get_target(e), LV_OBJ_FLAG_CLICKABLE);
        pos->processPayment();
    }
}

void Pos::cancel_button_event_cb(lv_event_t *e)
{
    Pos *pos = (Pos *)lv_event_get_user_data(e);
    if (!pos || pos->_is_stopping) {
        return;
    }

    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED) {
        ESP_UTILS_LOGI("Cancel button clicked, switching to POS_SCREEN_S1");
        pos->switchToScreen(POS_SCREEN_S1);
    }
}

void Pos::auto_advance_callback(void *arg)
{
    Pos *pos = static_cast<Pos *>(arg);
    if (!pos || pos->_is_stopping) {
        return;
    }

    // Execute in LVGL context
    lv_async_call([](void *user_data) {
        Pos *p = static_cast<Pos *>(user_data);
        if (p && !p->_is_stopping) {
            // Simulate payment result (90% success rate for demo)
            uint32_t random_val = esp_timer_get_time() % 100;
            if (random_val < 90) {
                // Payment success
                p->updateState(POS_STATE_SUCCESS);
                p->switchToScreen(POS_SCREEN_S5);
                p->showToast("Payment successful!", 2000);
                ESP_UTILS_LOGI("Payment successful for amount: %.2f", p->pos_info.amount);
            } else {
                // Payment failed
                p->updateState(POS_STATE_FAILED);
                p->switchToScreen(POS_SCREEN_S5);
                p->showToast("Payment failed! Please try again.", 3000);
                ESP_UTILS_LOGW("Payment failed for amount: %.2f", p->pos_info.amount);
            }
        }
    }, pos);
}

void Pos::showToast(const char *message, uint32_t duration_ms)
{
    if (!message || _is_stopping) {
        return;
    }

    if (_toast_container) {
        hideToast();
    }

    _toast_container = lv_obj_create(lv_scr_act());
    if (!_toast_container) {
        ESP_UTILS_LOGE("Toast container create failed");
        return;
    }

    lv_obj_set_size(_toast_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(_toast_container, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(_toast_container, LV_OPA_90, 0);
    lv_obj_set_style_radius(_toast_container, 8, 0);
    lv_obj_set_style_border_width(_toast_container, 0, 0);
    lv_obj_set_style_pad_all(_toast_container, 12, 0);
    lv_obj_set_style_shadow_width(_toast_container, 8, 0);
    lv_obj_set_style_shadow_opa(_toast_container, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(_toast_container, lv_color_black(), 0);

    _toast_label = lv_label_create(_toast_container);
    if (!_toast_label) {
        ESP_UTILS_LOGE("Toast label create failed");
        lv_obj_del(_toast_container);
        _toast_container = nullptr;
        return;
    }

    lv_label_set_text(_toast_label, message);
    lv_obj_set_style_text_color(_toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_toast_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(_toast_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_toast_label, LV_MIN(280, _width - 40));
    lv_obj_align(_toast_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_align(_toast_container, LV_ALIGN_BOTTOM_MID, 0, -60);

    if (duration_ms > 0) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = toast_timer_callback;
        timer_args.arg = this;
        timer_args.name = "pos_toast_timer";

        esp_err_t ret = esp_timer_create(&timer_args, &_toast_timer);
        if (ret == ESP_OK) {
            esp_timer_start_once(_toast_timer, duration_ms * 1000);
        } else {
            ESP_UTILS_LOGE("Toast timer create failed: %s", esp_err_to_name(ret));
        }
    }
}

void Pos::hideToast()
{
    if (_toast_timer) {
        esp_timer_stop(_toast_timer);
        esp_timer_delete(_toast_timer);
        _toast_timer = nullptr;
    }

    if (_toast_container) {
        lv_obj_del(_toast_container);
        _toast_container = nullptr;
        _toast_label = nullptr;
    }
}

void Pos::toast_timer_callback(void *arg)
{
    Pos *pos = static_cast<Pos *>(arg);
    if (pos && !pos->_is_stopping) {
        lv_async_call([](void *user_data) {
            Pos *p = static_cast<Pos *>(user_data);
            if (p && !p->_is_stopping) {
                p->hideToast();
            }
        }, pos);
    }
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, Pos, APP_NAME, []()
{
    return std::shared_ptr<Pos>(Pos::requestInstance(), [](Pos * p) {});
})

} // namespace esp_brookesia::apps
