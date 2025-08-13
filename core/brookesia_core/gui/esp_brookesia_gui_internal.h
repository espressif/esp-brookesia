/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_brookesia_internal.h"

#if !ESP_BROOKESIA_ENABLE_GUI
#   error "GUI is not enabled, please enable it in the menuconfig"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Animation Player ///////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER)
#   if defined(CONFIG_ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER)
#       define ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER  CONFIG_ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER
#   else
#       define ESP_BROOKESIA_GUI_ENABLE_ANIMATION_PLAYER  (0)
#   endif
#endif

#if ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER
#   if !defined(ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_ANIM_PLAYER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// LVGL //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG)
#       define ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG
#   else
#       define ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

#if ESP_BROOKESIA_LVGL_ENABLE_DEBUG_LOG
#   if !defined(ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_ANIMATION_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_CANVAS_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_CONTAINER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_CONTAINER_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_CONTAINER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_CONTAINER_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_CONTAINER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_DISPLAY_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_HELPER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_HELPER_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_HELPER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_HELPER_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_HELPER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_OBJECT_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_SCREEN_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_SCREEN_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_SCREEN_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_SCREEN_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_SCREEN_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(ESP_BROOKESIA_LVGL_TIMER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_ESP_BROOKESIA_LVGL_TIMER_ENABLE_DEBUG_LOG)
#           define ESP_BROOKESIA_LVGL_TIMER_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_LVGL_TIMER_ENABLE_DEBUG_LOG
#       else
#           define ESP_BROOKESIA_LVGL_TIMER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Squareline ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_GUI_ENABLE_SQUARELINE)
#   if defined(CONFIG_ESP_BROOKESIA_GUI_ENABLE_SQUARELINE)
#       define ESP_BROOKESIA_GUI_ENABLE_SQUARELINE  CONFIG_ESP_BROOKESIA_GUI_ENABLE_SQUARELINE
#   else
#       define ESP_BROOKESIA_GUI_ENABLE_SQUARELINE  (0)
#   endif
#endif

#if ESP_BROOKESIA_GUI_ENABLE_SQUARELINE
#   if !defined(ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP)
#       if defined(CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP)
#           define ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP  CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP
#       else
#           define ESP_BROOKESIA_EXPRESSION_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif

#   if !defined(ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS)
#       if defined(CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS)
#           define ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS  CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS
#       else
#           define ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS  (0)
#       endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Style //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(ESP_BROOKESIA_STYLE_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_ESP_BROOKESIA_STYLE_ENABLE_DEBUG_LOG)
#       define ESP_BROOKESIA_STYLE_ENABLE_DEBUG_LOG  CONFIG_ESP_BROOKESIA_STYLE_ENABLE_DEBUG_LOG
#   else
#       define ESP_BROOKESIA_STYLE_ENABLE_DEBUG_LOG  (0)
#   endif
#endif
