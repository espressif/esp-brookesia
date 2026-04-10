/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_gesture_nav.h"
#include "ui.h"
#include "ui_nav.h" /* reuse ui_nav_tab_t order */

static lv_obj_t *ui_gesture_get_screen_for_index(int idx)
{
    switch ((ui_nav_tab_t)idx) {
    case UI_NAV_TAB_HOME:
        return ui_Screen_Home;
    case UI_NAV_TAB_SCHEDULES:
        return ui_Screen_Schedules;
    case UI_NAV_TAB_SCENES:
        return ui_Screen_Scenes;
    case UI_NAV_TAB_USER:
        return ui_Screen_User;
    default:
        return NULL;
    }
}

static int ui_gesture_get_index_for_screen(lv_obj_t *scr)
{
    if (scr == ui_Screen_Home) {
        return UI_NAV_TAB_HOME;
    }
    if (scr == ui_Screen_Schedules) {
        return UI_NAV_TAB_SCHEDULES;
    }
    if (scr == ui_Screen_Scenes) {
        return UI_NAV_TAB_SCENES;
    }
    if (scr == ui_Screen_User) {
        return UI_NAV_TAB_USER;
    }
    return -1;
}

static void ui_gesture_nav_switch(lv_obj_t *scr, lv_dir_t dir)
{
    if (scr == NULL) {
        return;
    }
    int idx = ui_gesture_get_index_for_screen(scr);
    if (idx < 0) {
        return;
    }

    int next = idx;
    if (dir == LV_DIR_LEFT) {
        next = (idx + 1) % UI_NAV_TAB_MAX;
    } else if (dir == LV_DIR_RIGHT) {
        next = (idx - 1 + UI_NAV_TAB_MAX) % UI_NAV_TAB_MAX;
    } else {
        return;
    }

    lv_obj_t *target = ui_gesture_get_screen_for_index(next);
    if (!target) {
        return;
    }

    if (next == UI_NAV_TAB_HOME) {
        /* Scene activation may clear/rebuild the device list; Home cards still hold old
         * rm_device_item_handle_t pointers. Rebuild cards from current backend (same idea
         * as ui_nav Home tab, but without network/msgbox — list is already fresh). */
        ui_Screen_Home_reload_device_cards();
    } else if (next == UI_NAV_TAB_SCHEDULES) {
        ui_Screen_Schedules_show();
    } else if (next == UI_NAV_TAB_SCENES) {
        ui_Screen_Scenes_show();
    } else if (next == UI_NAV_TAB_USER) {
        ui_Screen_User_refresh_from_backend();
    }

    lv_disp_load_scr(target);
}

static void ui_gesture_nav_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) {
        return;
    }

    lv_obj_t *scr = lv_event_get_current_target(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    ui_gesture_nav_switch(scr, dir);
}

void ui_gesture_nav_enable(lv_obj_t *screen)
{
    if (!screen) {
        return;
    }
    lv_obj_add_event_cb(screen, ui_gesture_nav_event_cb, LV_EVENT_GESTURE, NULL);
}

void ui_gesture_nav_feed_dir(lv_dir_t dir)
{
    lv_obj_t *scr = lv_scr_act();
    ui_gesture_nav_switch(scr, dir);
}
