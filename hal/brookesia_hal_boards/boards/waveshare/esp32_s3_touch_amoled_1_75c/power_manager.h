#pragma once

#include "driver/i2c_master.h"
#include "axp2101_power_manager.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum {
    POWER_MANAGER_FEATURE_MIC,
} power_manager_feature_t;

typedef struct {
    i2c_master_dev_handle_t pm_handle;  /*!< I2C device handle for AXP2101 power management unit */
} power_manager_handle_t;

int power_manager_init(void *config, int cfg_size, void **device_handle);

int power_manager_deinit(void *device_handle);

esp_err_t power_manager_enable(void *device_handle, power_manager_feature_t feature);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
