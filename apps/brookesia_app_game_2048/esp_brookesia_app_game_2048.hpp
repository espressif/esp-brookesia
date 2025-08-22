/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"

namespace esp_brookesia::apps {

typedef struct {
    int x; // Index of row
    int y; // Index of column
    int weight;
} cell_weight_t;

class Game2048: public systems::speaker::App {
public:
    Game2048();
    ~Game2048();

    // Core app interface methods
    bool init() override;
    bool run() override;
    bool back() override;
    bool close() override;

    // Game logic methods
    void debugCells();
    void debugCells(int cell[4][4]);
    void debugCells(lv_obj_t *cell[4][4]);
    void debugCells(cell_weight_t cell[4][4]);
    void debugCells(lv_obj_t *cell[4]);
    void cleanForegroundCells();
    void generateForegroundCell();
    void addRemoveReadyCell(lv_obj_t *cell);
    void cleanRemoveReadyCell();
    void newGame();
    void updateCellValue();
    void updateCurrentScore(int score);
    void updateBestScore(int score);
    void updateCellsStyle();
    int maxWeight();
    int moveLeft();
    int moveRight();
    int moveUp();
    int moveDown();
    bool isGameOver();

private:
    lv_obj_t *addBackgroundCell(lv_obj_t *parent);
    void startAnimationX(lv_obj_t *target, int x, int time);
    void startAnimationY(lv_obj_t *target, int y, int time);

    static void new_game_event_cb(lv_event_t *e);
    static void motion_event_cb(lv_event_t *e);
    static void anim_finish_cb(lv_anim_t *a);

    uint16_t _width = 0;
    uint16_t _height = 0;
    uint16_t _current_score = 0;
    uint16_t _best_score = 0;
    uint16_t _weight_max = 0;
    bool _is_closing = false;
    bool _anim_running_flag = false;
    bool _generate_cell_flag = false;

    cell_weight_t _cells_weight[4][4] = {};
    lv_obj_t *_cur_score_label = nullptr;
    lv_obj_t *_best_score_label = nullptr;
    lv_obj_t *_background_cells[4][4] = {};
    lv_obj_t *_foreground_cells[4][4] = {};
    lv_obj_t *_remove_ready_cells[4][4] = {};
    lv_obj_t *_foreground_grid = nullptr;
    lv_obj_t *_game_grid = nullptr;
    lv_color_t _cell_colors[11] = {};
};

} // namespace esp_brookesia::apps
