/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"

/**
 * @brief Default log tag used by the video service component.
 */
#define BROOKESIA_SERVICE_VIDEO_LOG_TAG "SvcVideo"

#if !defined(BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG)
#   if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG)
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG
#   else
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG  (0)
#   endif
#endif

#if BROOKESIA_SERVICE_VIDEO_ENABLE_DEBUG_LOG
#   if !defined(BROOKESIA_SERVICE_VIDEO_ENCODER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_ENABLE_DEBUG_LOG)
#           define BROOKESIA_SERVICE_VIDEO_ENCODER_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_SERVICE_VIDEO_ENCODER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#   if !defined(BROOKESIA_SERVICE_VIDEO_DECODER_ENABLE_DEBUG_LOG)
#       if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_ENABLE_DEBUG_LOG)
#           define BROOKESIA_SERVICE_VIDEO_DECODER_ENABLE_DEBUG_LOG  CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_ENABLE_DEBUG_LOG
#       else
#           define BROOKESIA_SERVICE_VIDEO_DECODER_ENABLE_DEBUG_LOG  (0)
#       endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Encoder ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER)
#   if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER)
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER  CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER
#   else
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER  (0)
#   endif
#endif
#if BROOKESIA_SERVICE_VIDEO_ENABLE_ENCODER
#   if !defined(BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX)
#      if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX)
#          define BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX  CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX
#      else
#          define BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_DEVICE_PATH_PREFIX  "/dev/video"
#      endif
#   endif
#   if !defined(BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM)
#      if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM)
#          define BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM  CONFIG_BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM
#      else
#          define BROOKESIA_SERVICE_VIDEO_ENCODER_DEFAULT_NUM  (1)
#      endif
#   endif
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Decoder ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER)
#   if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER)
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER  CONFIG_BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER
#   else
#       define BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER  (0)
#   endif
#endif
#if BROOKESIA_SERVICE_VIDEO_ENABLE_DECODER
#   if !defined(BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM)
#      if defined(CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM)
#          define BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM  CONFIG_BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM
#      else
#          define BROOKESIA_SERVICE_VIDEO_DECODER_DEFAULT_NUM  (1)
#       endif
#   endif
#endif
