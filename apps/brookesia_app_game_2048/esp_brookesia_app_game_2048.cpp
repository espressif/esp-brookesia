/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <unistd.h>
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:App:2048"
#include "esp_lib_utils.h"
#include "esp_brookesia_app_game_2048.hpp"

using namespace esp_brookesia::systems::speaker;

#define APP_NAME "2048"

#define ENABLE_CELL_DEBUG       (1)

#define BOARD_BG_COLOR          lv_color_white()
#define BOARD_TITLE_FONT        &lv_font_montserrat_20
#define BOARD_TITLE_COLOR       lv_palette_main(LV_PALETTE_BROWN)
#define SCORE_TITLE_FONT        &lv_font_montserrat_16
#define SCORE_TITLE_COLOR       lv_color_white()
#define SCORE_CONTEN_FONT       &lv_font_montserrat_18
#define SCORE_HEIGHT            60
#define SCORE_WIDTH             80

#define GRID_BG_COLOR           lv_palette_main(LV_PALETTE_BROWN)
#define GRID_FONT               &lv_font_montserrat_24
#define GRID_PAD                10

#define CELL_BG_COLOR           lv_color_make(255, 255, 255)
#define CELL_RADIUS             3
#define CELL_SIZE               ((short int)((_width - 5 * GRID_PAD) / 4))
#define CELL_OPA_1              LV_OPA_10
#define CELL_OPA_2              LV_OPA_COVER

#define ANIM_PERIOD             200

#define randint_between(min, max)       (rand() % (max - min) + min)
#define rand_1_2()                      (randint_between(1, 2))

LV_IMG_DECLARE(img_app_2048);

namespace esp_brookesia::apps {

constexpr systems::base::App::Config CORE_DATA = {
    .name = APP_NAME,
    .launcher_icon = gui::StyleImage::IMAGE(&img_app_2048),
    .screen_size = gui::StyleSize::RECT_PERCENT(100, 100),
    .flags = {
        .enable_default_screen = 1,
        .enable_recycle_resource = 0,
        .enable_resize_visual_area = 1,
    },
};
constexpr App::Config APP_DATA = {
    .app_launcher_page_index = 0,
    .flags = {
        .enable_navigation_gesture = 1,
    },
};

Game2048::Game2048()
    : App(CORE_DATA, APP_DATA)
{
}

Game2048::~Game2048()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();
}

bool Game2048::init()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    gui::StyleSize size;
    ESP_UTILS_CHECK_FALSE_RETURN(getSystemContext()->getDisplaySize(size), false, "Get display size failed");

    _width = size.width / 3 * 2;
    _height = _width;
    ESP_UTILS_CHECK_FALSE_RETURN(_width > 0, false, "Invalid width(%d)", _width);
    ESP_UTILS_CHECK_FALSE_RETURN(_height > 0, false, "Invalid height(%d)", _height);

    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].weight = 0;
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
        _foreground_cells[i / 4][i % 4] = NULL;
        _remove_ready_cells[i / 4][i % 4] = NULL;
    }
    _cell_colors[0] = CELL_BG_COLOR;
    // Yellow
    _cell_colors[1] = lv_color_make(0xFF, 0xFF, 0x99);
    _cell_colors[2] = lv_color_make(0xFF, 0xFF, 0x33);
    // Orange
    _cell_colors[3] = lv_color_make(0xFF, 0xCC, 0x99);
    _cell_colors[4] = lv_color_make(0xFF, 0xCC, 0x33);
    // Green
    _cell_colors[5] = lv_color_make(0x00, 0xCC, 0x99);
    _cell_colors[6] = lv_color_make(0x00, 0xCC, 0x66);
    // Blue
    _cell_colors[7] = lv_color_make(0x00, 0x66, 0xFF);
    _cell_colors[8] = lv_color_make(0x00, 0x33, 0x99);
    // Red
    _cell_colors[9] = lv_color_make(0xFF, 0x33, 0x99);
    _cell_colors[10] = lv_color_make(0xFF, 0x33, 0x00);

    return true;
}

bool Game2048::run()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    srand(time(nullptr));

    /* Set screen background color to match grid */
    lv_obj_set_style_bg_color(lv_scr_act(), GRID_BG_COLOR, 0);

    /* Setup title */
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(title, BOARD_TITLE_FONT, 0);
    lv_obj_set_style_text_color(title, BOARD_TITLE_COLOR, 0);
    lv_label_set_text(title, "2048");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    /* Setup score displays and button in same row */
    lv_obj_t *cur = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cur, SCORE_WIDTH, SCORE_HEIGHT);
    lv_obj_align(cur, LV_ALIGN_TOP_LEFT, 50, 35);
    lv_obj_set_style_radius(cur, CELL_RADIUS, 0);
    lv_obj_set_style_border_width(cur, 2, 0);
    lv_obj_set_style_border_color(cur, lv_color_white(), 0);
    lv_obj_set_style_pad_all(cur, 5, 0);
    lv_obj_set_style_bg_color(cur, GRID_BG_COLOR, 0);
    lv_obj_set_style_shadow_width(cur, 8, 0);
    lv_obj_set_style_shadow_color(cur, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(cur, LV_OPA_30, 0);

    lv_obj_t *score_title = lv_label_create(cur);
    lv_obj_set_style_text_font(score_title, SCORE_TITLE_FONT, 0);
    lv_obj_set_style_text_color(score_title, SCORE_TITLE_COLOR, 0);
    lv_label_set_text(score_title, "SCORE");
    lv_obj_align(score_title, LV_ALIGN_TOP_MID, 0, 5);

    _cur_score_label = lv_label_create(cur);
    lv_obj_set_style_text_font(_cur_score_label, SCORE_CONTEN_FONT, 0);
    lv_obj_set_style_text_color(_cur_score_label, SCORE_TITLE_COLOR, 0);
    lv_label_set_text(_cur_score_label, "0");
    lv_obj_align(_cur_score_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* Setup New Game button - same height as score displays with enhanced visibility */
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 80, SCORE_HEIGHT);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_radius(btn, CELL_RADIUS, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_white(), 0);
    lv_obj_set_style_pad_all(btn, 5, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x8f7a66), 0); // Darker brown for button
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    // Add press effect
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x9f8a76), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, new_game_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t *btn_title = lv_label_create(btn);
    lv_obj_set_style_text_font(btn_title, SCORE_TITLE_FONT, 0);
    lv_obj_set_style_text_color(btn_title, lv_color_white(), 0);
    lv_label_set_text(btn_title, "New\nGame");
    lv_obj_set_style_text_align(btn_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(btn_title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *best = lv_obj_create(lv_scr_act());
    lv_obj_set_size(best, SCORE_WIDTH, SCORE_HEIGHT);
    lv_obj_align(best, LV_ALIGN_TOP_RIGHT, -50, 35);
    lv_obj_set_style_radius(best, CELL_RADIUS, 0);
    lv_obj_set_style_border_width(best, 2, 0);
    lv_obj_set_style_border_color(best, lv_color_white(), 0);
    lv_obj_set_style_pad_all(best, 5, 0);
    lv_obj_set_style_bg_color(best, GRID_BG_COLOR, 0);
    lv_obj_set_style_shadow_width(best, 8, 0);
    lv_obj_set_style_shadow_color(best, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(best, LV_OPA_30, 0);

    score_title = lv_label_create(best);
    lv_obj_set_style_text_font(score_title, SCORE_TITLE_FONT, 0);
    lv_obj_set_style_text_color(score_title, SCORE_TITLE_COLOR, 0);
    lv_label_set_text(score_title, "BEST");
    lv_obj_align(score_title, LV_ALIGN_TOP_MID, 0, 5);

    _best_score_label = lv_label_create(best);
    lv_obj_set_style_text_font(_best_score_label, SCORE_CONTEN_FONT, 0);
    lv_obj_set_style_text_color(_best_score_label, SCORE_TITLE_COLOR, 0);
    lv_label_set_text_fmt(_best_score_label, "%d", _best_score);
    lv_obj_align(_best_score_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    /* Setup grid - positioned with more space now that top is cleaner */
    static lv_coord_t col_dsc[] = {CELL_SIZE, CELL_SIZE, CELL_SIZE, CELL_SIZE, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {CELL_SIZE, CELL_SIZE, CELL_SIZE, CELL_SIZE, LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(lv_scr_act());
    lv_obj_set_size(grid, _width, _width);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_style_radius(grid, 0, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_bg_color(grid, GRID_BG_COLOR, 0);
    lv_obj_set_style_text_font(grid, GRID_FONT, 0);
    lv_obj_set_style_grid_column_dsc_array(grid, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(grid, row_dsc, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_align(grid, LV_GRID_ALIGN_CENTER, LV_GRID_ALIGN_CENTER);

    lv_obj_t *cell;
    for (int i = 0; i < 16; i++) {
        cell = addBackgroundCell(grid);
        _background_cells[i / 4][i % 4] = cell;
        lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, i % 4, 1,
                             LV_GRID_ALIGN_STRETCH, i / 4, 1);
        lv_obj_update_layout(cell);
    }

    _foreground_grid = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_foreground_grid, _width, _width);
    lv_obj_align(_foreground_grid, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_style_radius(_foreground_grid, 0, 0);
    lv_obj_set_style_border_width(_foreground_grid, 0, 0);
    lv_obj_set_style_pad_all(_foreground_grid, 0, 0);
    lv_obj_set_style_bg_opa(_foreground_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(_foreground_grid, GRID_FONT, 0);
    lv_obj_add_flag(_foreground_grid, LV_OBJ_FLAG_CLICKABLE);

    /* Add motion detect module */
    auto gesture = getSystem()->getManager().getGesture();
    lv_obj_add_event_cb(gesture->getEventObj(), motion_event_cb, gesture->getReleaseEventCode(), this);

    newGame();

    return true;
}

/**
 * @brief The function will be called when the left button of navigate bar is clicked.
 */
bool Game2048::back()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

/**
 * @brief The function will be called when app should be closed.
 */
bool Game2048::close()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    _is_closing = true;

    // Since this function is usually called through gesture callback function,
    // we should avoid calling it during lvgl task traversal
    // So use lv_async_call for asynchronous call
    lv_async_call([](void *user_data) {
        auto app = static_cast<Game2048 *>(user_data);
        ESP_UTILS_CHECK_NULL_EXIT(app, "Invalid app");

        auto system = app->getSystem();
        ESP_UTILS_CHECK_NULL_EXIT(system, "Invalid system");

        auto gesture = system->getManager().getGesture();
        ESP_UTILS_CHECK_NULL_EXIT(gesture, "Invalid gesture");

        bool ret = lv_obj_remove_event_cb(gesture->getEventObj(), motion_event_cb);
        if (!ret) {
            ESP_UTILS_LOGE("Remove event callback failed");
        }

        app->_is_closing = false;
    }, this);

    return true;
}

void Game2048::debugCells()
{
#if ENABLE_CELL_DEBUG
    debugCells(_foreground_cells);
    debugCells(_background_cells);
    debugCells(_cells_weight);
#endif
}

void Game2048::debugCells(int cell[4][4])
{
#if ENABLE_CELL_DEBUG
    ESP_UTILS_LOGD("cell");
    for (int i = 0; i < 4; i++) {
        ESP_UTILS_LOGD("\t%d\t%d\t%d\t%d", cell[i][0], cell[i][1], cell[i][2], cell[i][3]);
    }
#endif
}

void Game2048::debugCells(lv_obj_t *cell[4][4])
{
#if ENABLE_CELL_DEBUG
    for (int i = 0; i < 4; i++) {
        ESP_UTILS_LOGD("\t%p\t%p\t%p\t%p", cell[i][0], cell[i][1], cell[i][2], cell[i][3]);
    }
#endif
}

void Game2048::debugCells(cell_weight_t cell[4][4])
{
#if ENABLE_CELL_DEBUG
    for (int i = 0; i < 4; i++) {
        ESP_UTILS_LOGD(
            "\t(%d,%d,%d)\t(%d,%d,%d)\t(%d,%d,%d)\t(%d,%d,%d)",
            cell[i][0].x, cell[i][0].y, cell[i][0].weight,
            cell[i][1].x, cell[i][1].y, cell[i][1].weight,
            cell[i][2].x, cell[i][2].y, cell[i][2].weight,
            cell[i][3].x, cell[i][3].y, cell[i][3].weight
        );
    }
#endif
}

void Game2048::debugCells(lv_obj_t *cell[4])
{
#if ENABLE_CELL_DEBUG
    for (int i = 0; i < 4; i++) {
        ESP_UTILS_LOGD("\t%p\t%p\t%p\t%p", cell[i], cell[i], cell[i], cell[i]);
    }
#endif
}

void Game2048::cleanForegroundCells()
{
    lv_obj_t *child = lv_obj_get_child(_foreground_grid, 0);

    while (child) {
        lv_obj_del(child);
        child = lv_obj_get_child(_foreground_grid, 0);
    }
    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].weight = 0;
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
        _foreground_cells[i / 4][i % 4] = NULL;
    }
}

void Game2048::newGame()
{
    _weight_max = 0;
    _current_score = 0;
    updateCurrentScore(_current_score);
    cleanForegroundCells();
    generateForegroundCell();
    generateForegroundCell();
    updateCellsStyle();
}

lv_obj_t *Game2048::addBackgroundCell(lv_obj_t *parent)
{
    lv_obj_t *cell = lv_obj_create(parent);
    // Shape
    lv_obj_set_style_radius(cell, CELL_RADIUS, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    // Background
    lv_obj_set_style_bg_color(cell, CELL_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(cell, CELL_OPA_1, 0);
    // Others
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    return cell;
}

void Game2048::generateForegroundCell()
{
    int zero_amount = 0, zero_index = 0;
    int target = 0;
    int target_i = 0;
    int target_j = 0;
    int target_weight = rand_1_2();

    _weight_max = (target_weight > _weight_max) ?
                  target_weight : _weight_max;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (_cells_weight[i][j].weight == 0) {
                zero_amount++;
            }
        }
    }
    if (zero_amount == 0) {
        return;
    }

    target = randint_between(0, zero_amount);

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if ((_cells_weight[i][j].weight == 0) &&
                    (zero_index++ == target)) {
                target_i = i;
                target_j = j;
                _cells_weight[i][j].weight = target_weight;
            }
        }
    }

    /* Add a new object of cell */
    lv_obj_t *cell = lv_obj_create(_foreground_grid);
    _foreground_cells[target_i][target_j] = cell;
    // Size
    lv_obj_set_size(cell, CELL_SIZE, CELL_SIZE);
    // Position
    lv_obj_set_pos(
        cell,
        lv_obj_get_x(_background_cells[target_i][target_j]),
        lv_obj_get_y(_background_cells[target_i][target_j])
    );
    // Shape
    lv_obj_set_style_radius(cell, CELL_RADIUS, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    // Background
    lv_obj_set_style_bg_color(cell, CELL_BG_COLOR, 0);
    lv_obj_set_style_opa(cell, CELL_OPA_2, 0);
    // Others
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(cell);
    lv_label_set_text_fmt(label, "%d", 1 << target_weight);
    lv_obj_center(label);

    debugCells();
}

void Game2048::addRemoveReadyCell(lv_obj_t *cell)
{
    if (cell == NULL) {
        return;
    }
    for (int i = 0; i < 16; i++) {
        if (_remove_ready_cells[i / 4][i % 4] == cell) {
            return;
        }
    }
    for (int i = 0; i < 16; i++) {
        if (_remove_ready_cells[i / 4][i % 4] == NULL) {
            _remove_ready_cells[i / 4][i % 4] = cell;
            return;
        }
    }
}

void Game2048::cleanRemoveReadyCell()
{
    for (int i = 0; i < 16; i++) {
        if (_remove_ready_cells[i / 4][i % 4] != NULL) {
            lv_obj_del(_remove_ready_cells[i / 4][i % 4]);
            _remove_ready_cells[i / 4][i % 4] = NULL;
        }
    }
}

void Game2048::startAnimationX(lv_obj_t *target, int x, int time)
{
    lv_anim_t a;

    _anim_running_flag = true;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_user_data(&a, this);
    lv_obj_update_layout(target);
    lv_anim_set_values(&a, lv_obj_get_x(target), x);
    lv_anim_set_time(&a, time);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_ready_cb(&a, anim_finish_cb);
    lv_anim_start(&a);
}

void Game2048::startAnimationY(lv_obj_t *target, int y, int time)
{
    lv_anim_t a;

    _anim_running_flag = true;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_user_data(&a, this);
    lv_obj_update_layout(target);
    lv_anim_set_values(&a, lv_obj_get_y(target), y);
    lv_anim_set_time(&a, time);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_ready_cb(&a, anim_finish_cb);
    lv_anim_start(&a);
}

void Game2048::updateCellValue()
{
    for (int i = 0; i < 16; i++) {
        if (_foreground_cells[i / 4][i % 4] != NULL) {
            lv_obj_t *label = lv_obj_get_child(_foreground_cells[i / 4][i % 4], 0);
            int value = 1 << _cells_weight[i / 4][i % 4].weight;
            lv_label_set_text_fmt(label, "%d", value);
        }
    }
}

void Game2048::updateCurrentScore(int score)
{
    lv_label_set_text_fmt(_cur_score_label, "%d", score);
}

void Game2048::updateBestScore(int score)
{
    lv_label_set_text_fmt(_best_score_label, "%d", score);
}

void Game2048::updateCellsStyle()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (_foreground_cells[i][j] != NULL && _cells_weight[i][j].weight > 0) {
                int color_index = _cells_weight[i][j].weight - 1;
                if (color_index >= 0 && color_index < 11) {
                    lv_obj_set_style_bg_color(
                        _foreground_cells[i][j],
                        _cell_colors[color_index],
                        0
                    );
                }
            }
        }
    }
}

int Game2048::maxWeight()
{
    return _weight_max;
}

int Game2048::moveLeft()
{
    int target_cells[4][4];
    int score = -1;

    for (int i = 0; i < 16; i++) {
        target_cells[i / 4][i % 4] = i % 4;
    }

    debugCells(_cells_weight);
    debugCells(target_cells);

    for (int i = 0; i < 4; i++) {
        for (int j = 1; j < 4; j++) {
            if (_cells_weight[i][j].weight != 0) {
                int merge_flag = false;
                int cur_j = j;
                while (cur_j != 0) {
                    if ((_cells_weight[i][cur_j - 1].weight == _cells_weight[i][cur_j].weight) &&
                            (!merge_flag)) {
                        merge_flag = true;
                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j].y & (1 << k);
                            if (target_flag) {
                                target_cells[i][k] = cur_j - 1;
                            }
                        }

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j - 1].y & (1 << k);
                            if (target_flag) {
                                addRemoveReadyCell(_foreground_cells[i][k]);
                            }
                        }

                        _cells_weight[i][cur_j - 1].y += _cells_weight[i][cur_j].y;
                        _cells_weight[i][cur_j - 1].weight++;
                        _cells_weight[i][cur_j].weight = 0;
                        _weight_max = (_cells_weight[i][cur_j - 1].weight > _weight_max) ?
                                      _cells_weight[i][cur_j - 1].weight : _weight_max;

                        score = (score < 0) ? 0 : score;
                        score += 1 << _cells_weight[i][cur_j - 1].weight;
                    } else if (_cells_weight[i][cur_j - 1].weight == 0) {

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j].y & (1 << k);
                            if (target_flag) {
                                target_cells[i][k] = cur_j - 1;
                            }
                        }

                        score = (score < 0) ? 0 : score;
                        _cells_weight[i][cur_j - 1] = _cells_weight[i][cur_j];
                        _cells_weight[i][cur_j].weight = 0;
                    } else {
                        break;
                    }
                    cur_j--;
                }
            }
        }
    }

    debugCells(_cells_weight);
    debugCells(target_cells);
    debugCells(_remove_ready_cells);
    debugCells(_foreground_cells);

    for (int i = 0; i < 4; i++) {
        for (int j = 1; j < 4; j++) {
            int target_j = target_cells[i][j];
            if ((_foreground_cells[i][j] != NULL) && (j != target_j)) {
                // printf(NULL, "i,j: %d,%d", i, j);
                int target_x = lv_obj_get_x(_background_cells[i][target_j]);
                startAnimationX(_foreground_cells[i][j], target_x, ANIM_PERIOD);
                _foreground_cells[i][target_j] = _foreground_cells[i][j];
                _foreground_cells[i][j] = NULL;
            }
        }
    }

    debugCells(_foreground_cells);

    // Reset x,y of weight
    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
    }

    return score;
}

int Game2048::moveRight()
{
    int target_cells[4][4];
    int score = -1;

    for (int i = 0; i < 16; i++) {
        target_cells[i / 4][i % 4] = i % 4;
    }

    debugCells(_cells_weight);
    debugCells(target_cells);

    for (int i = 0; i < 4; i++) {
        for (int j = 2; j >= 0; j--) {
            if (_cells_weight[i][j].weight != 0) {
                int merge_flag = false;
                int cur_j = j;
                while (cur_j != 3) {
                    if ((_cells_weight[i][cur_j + 1].weight == _cells_weight[i][cur_j].weight) &&
                            (!merge_flag)) {
                        merge_flag = true;
                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j].y & (1 << k);
                            if (target_flag) {
                                target_cells[i][k] = cur_j + 1;
                            }
                        }

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j + 1].y & (1 << k);
                            if (target_flag) {
                                addRemoveReadyCell(_foreground_cells[i][k]);
                            }
                        }

                        _cells_weight[i][cur_j + 1].y += _cells_weight[i][cur_j].y;
                        _cells_weight[i][cur_j + 1].weight++;
                        _cells_weight[i][cur_j].weight = 0;
                        _weight_max = (_cells_weight[i][cur_j + 1].weight > _weight_max) ?
                                      _cells_weight[i][cur_j + 1].weight : _weight_max;

                        score = (score < 0) ? 0 : score;
                        score += 1 << _cells_weight[i][cur_j + 1].weight;
                    } else if (_cells_weight[i][cur_j + 1].weight == 0) {

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[i][cur_j].y & (1 << k);
                            if (target_flag) {
                                target_cells[i][k] = cur_j + 1;
                            }
                        }

                        score = (score < 0) ? 0 : score;
                        _cells_weight[i][cur_j + 1] = _cells_weight[i][cur_j];
                        _cells_weight[i][cur_j].weight = 0;
                    } else {
                        break;
                    }
                    cur_j++;
                }
            }
        }
    }

    debugCells(_cells_weight);
    debugCells(target_cells);
    debugCells(_remove_ready_cells);
    debugCells(_foreground_cells);

    for (int i = 0; i < 4; i++) {
        for (int j = 2; j >= 0; j--) {
            int target_j = target_cells[i][j];
            if ((_foreground_cells[i][j] != NULL) && (j != target_j)) {
                // printf(NULL, "i,j: %d,%d", i, j);
                int target_x = lv_obj_get_x(_background_cells[i][target_j]);
                startAnimationX(_foreground_cells[i][j], target_x, ANIM_PERIOD);
                _foreground_cells[i][target_j] = _foreground_cells[i][j];
                _foreground_cells[i][j] = NULL;
            }
        }
    }

    debugCells(_foreground_cells);

    // Reset x,y of weight
    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
    }

    return score;
}

int Game2048::moveUp()
{
    int target_cells[4][4];
    int score = -1;

    for (int i = 0; i < 16; i++) {
        target_cells[i / 4][i % 4] = i / 4;
    }

    debugCells(_cells_weight);
    debugCells(target_cells);

    for (int j = 0; j < 4; j++) {
        for (int i = 1; i < 4; i++) {
            if (_cells_weight[i][j].weight != 0) {
                int merge_flag = false;
                int cur_i = i;
                while (cur_i != 0) {
                    if ((_cells_weight[cur_i - 1][j].weight == _cells_weight[cur_i][j].weight) &&
                            (!merge_flag)) {
                        merge_flag = true;
                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i][j].x & (1 << k);
                            if (target_flag) {
                                target_cells[k][j] = cur_i - 1;
                            }
                        }

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i - 1][j].x & (1 << k);
                            if (target_flag) {
                                addRemoveReadyCell(_foreground_cells[k][j]);
                            }
                        }

                        _cells_weight[cur_i - 1][j].x += _cells_weight[cur_i][j].x;
                        _cells_weight[cur_i - 1][j].weight++;
                        _cells_weight[cur_i][j].weight = 0;
                        _weight_max = (_cells_weight[cur_i - 1][j].weight > _weight_max) ?
                                      _cells_weight[cur_i - 1][j].weight : _weight_max;

                        score = (score < 0) ? 0 : score;
                        score += 1 << _cells_weight[cur_i - 1][j].weight;
                    } else if (_cells_weight[cur_i - 1][j].weight == 0) {

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i][j].x & (1 << k);
                            if (target_flag) {
                                target_cells[k][j] = cur_i - 1;
                            }
                        }

                        score = (score < 0) ? 0 : score;
                        _cells_weight[cur_i - 1][j] = _cells_weight[cur_i][j];
                        _cells_weight[cur_i][j].weight = 0;
                    } else {
                        break;
                    }
                    cur_i--;
                }
            }
        }
    }

    debugCells(_cells_weight);
    debugCells(target_cells);
    debugCells(_remove_ready_cells);
    debugCells(_foreground_cells);

    for (int j = 0; j < 4; j++) {
        for (int i = 1; i < 4; i++) {
            int target_i = target_cells[i][j];
            if ((_foreground_cells[i][j] != NULL) && (i != target_i)) {
                // printf(NULL, "i,j: %d,%d", i, j);
                int target_y = lv_obj_get_y(_background_cells[target_i][j]);
                startAnimationY(_foreground_cells[i][j], target_y, ANIM_PERIOD);
                _foreground_cells[target_i][j] = _foreground_cells[i][j];
                _foreground_cells[i][j] = NULL;
            }
        }
    }

    debugCells(_foreground_cells);

    // Reset x,y of weight
    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
    }

    return score;
}

int Game2048::moveDown()
{
    int target_cells[4][4];
    int score = -1;

    for (int i = 0; i < 16; i++) {
        target_cells[i / 4][i % 4] = i / 4;
    }

    debugCells(_cells_weight);
    debugCells(target_cells);

    for (int j = 0; j < 4; j++) {
        for (int i = 2; i >= 0; i--) {
            if (_cells_weight[i][j].weight != 0) {
                int merge_flag = false;
                int cur_i = i;
                while (cur_i != 3) {
                    if ((_cells_weight[cur_i + 1][j].weight == _cells_weight[cur_i][j].weight) &&
                            (!merge_flag)) {
                        merge_flag = true;
                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i][j].x & (1 << k);
                            if (target_flag) {
                                target_cells[k][j] = cur_i + 1;
                            }
                        }

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i + 1][j].x & (1 << k);
                            if (target_flag) {
                                addRemoveReadyCell(_foreground_cells[k][j]);
                            }
                        }

                        _cells_weight[cur_i + 1][j].x += _cells_weight[cur_i][j].x;
                        _cells_weight[cur_i + 1][j].weight++;
                        _cells_weight[cur_i][j].weight = 0;
                        _weight_max = (_cells_weight[cur_i + 1][j].weight > _weight_max) ?
                                      _cells_weight[cur_i + 1][j].weight : _weight_max;

                        score = (score < 0) ? 0 : score;
                        score += 1 << _cells_weight[cur_i + 1][j].weight;
                    } else if (_cells_weight[cur_i + 1][j].weight == 0) {

                        for (int k = 0; k < 4; k++) {
                            int target_flag = _cells_weight[cur_i][j].x & (1 << k);
                            if (target_flag) {
                                target_cells[k][j] = cur_i + 1;
                            }
                        }

                        score = (score < 0) ? 0 : score;
                        _cells_weight[cur_i + 1][j] = _cells_weight[cur_i][j];
                        _cells_weight[cur_i][j].weight = 0;
                    } else {
                        break;
                    }
                    cur_i++;
                }
            }
        }
    }

    debugCells(_cells_weight);
    debugCells(target_cells);
    debugCells(_remove_ready_cells);
    debugCells(_foreground_cells);

    for (int j = 0; j < 4; j++) {
        for (int i = 2; i >= 0; i--) {
            int target_i = target_cells[i][j];
            if ((_foreground_cells[i][j] != NULL) && (i != target_i)) {
                // printf(NULL, "i,j: %d,%d", i, j);
                int target_y = lv_obj_get_y(_background_cells[target_i][j]);
                startAnimationY(_foreground_cells[i][j], target_y, ANIM_PERIOD);
                _foreground_cells[target_i][j] = _foreground_cells[i][j];
                _foreground_cells[i][j] = NULL;
            }
        }
    }

    debugCells(_foreground_cells);

    // Reset x,y of weight
    for (int i = 0; i < 16; i++) {
        _cells_weight[i / 4][i % 4].x = 1 << (i / 4);
        _cells_weight[i / 4][i % 4].y = 1 << (i % 4);
    }

    return score;
}

bool Game2048::isGameOver()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 1; j < 4; j++) {
            if ((_cells_weight[i][j].weight == _cells_weight[i][j - 1].weight) ||
                    (_cells_weight[j][i].weight == _cells_weight[j - 1][i].weight) ||
                    (_cells_weight[i][j].weight == 0)) {
                return false;
            }
        }
    }

    return true;
}

void Game2048::new_game_event_cb(lv_event_t *e)
{
    Game2048 *app = (Game2048 *)lv_event_get_user_data(e);

    app->newGame();
}

void Game2048::motion_event_cb(lv_event_t *e)
{
    int score;
    systems::speaker::GestureInfo *type = (systems::speaker::GestureInfo *)lv_event_get_param(e);
    Game2048 *app = (Game2048 *)lv_event_get_user_data(e);

    // If the app is closing, return immediately
    if (app->_is_closing) {
        return;
    }

    if (!app->_anim_running_flag) {
        switch (type->direction) {
        case systems::speaker::GESTURE_DIR_UP:
            // printf(NULL, "up");
            score = app->moveUp();
            break;
        case systems::speaker::GESTURE_DIR_DOWN:
            // printf(NULL, "down");
            score = app->moveDown();
            break;
        case systems::speaker::GESTURE_DIR_LEFT:
            // printf(NULL, "left");
            score = app->moveLeft();
            break;
        case systems::speaker::GESTURE_DIR_RIGHT:
            // printf(NULL, "right");
            score = app->moveRight();
            break;
        default:
            return;
        }

        printf("score: %d\n", score);

        if (score >= 0) {
            app->_generate_cell_flag = true;
            app->_current_score += score;
            app->updateCurrentScore(app->_current_score);
            if (app->_current_score > app->_best_score) {
                app->_best_score = app->_current_score;
                app->updateBestScore(app->_best_score);
            }
        }
        if (app->maxWeight() == 11) {
            printf("Congratualation! You win!\n");
            app->newGame();
        }
        if (app->isGameOver()) {
            printf("Game Over\n");
        }
    }
}

void Game2048::anim_finish_cb(lv_anim_t *a)
{
    Game2048 *app = (Game2048 *)lv_anim_get_user_data(a);

    app->cleanRemoveReadyCell();
    app->updateCellValue();
    if (app->_generate_cell_flag) {
        app->_generate_cell_flag = false;
        app->generateForegroundCell();
        app->updateCellsStyle();
    }
    if (app->_anim_running_flag) {
        app->_anim_running_flag = false;
    }
}

ESP_UTILS_REGISTER_PLUGIN(systems::base::App, Game2048, APP_NAME)

} // namespace esp_brookesia::apps
