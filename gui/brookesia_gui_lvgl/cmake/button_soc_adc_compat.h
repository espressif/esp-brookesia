/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "soc/soc_caps.h"
#include "hal/adc_ll.h"

#ifndef SOC_ADC_CHANNEL_NUM
#define SOC_ADC_CHANNEL_NUM(unit) ADC_LL_CHANNEL_NUM(unit)
#endif
