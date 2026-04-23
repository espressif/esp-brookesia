/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui.h"
#include "ui_i18n.h"

lv_obj_t *ui_Startevents____initial_actions0;

void phone_app_rainmaker_ui_init(void)
{
}

void phone_app_rainmaker_ui_deinit(void)
{
}

void phone_app_rainmaker_ui_set_language(ui_lang_t lang)
{
    ui_i18n_set_lang(lang);
    ui_refresh_all_texts();
}

void phone_app_rainmaker_ui_soft_restart(void)
{
}
