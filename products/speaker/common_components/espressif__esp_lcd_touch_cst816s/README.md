# ESP LCD Touch CST816S Controller

[![Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch_cst816s/badge.svg)](https://components.espressif.com/components/espressif/esp_lcd_touch_cst816s)

Implementation of the CST816S touch controller with esp_lcd_touch component.

| Touch controller | Communication interface |    Component name     |                             Link to datasheet                              |
| :--------------: | :---------------------: | :-------------------: | :------------------------------------------------------------------------: |
|     CST816S      |           I2C           | esp_lcd_touch_cst816s | [datasheet](https://www.buydisplay.com/download/ic/DS-CST816S_DS_V1.3.pdf) |

**Note, there are two things about the driver are noteworthy (from [document](https://doc.riot-os.org/group__drivers__cst816s.html)):**

* It only responds to I2C commands after an event, such as a touch detection. Do not expect it to respond on init. Instead after a touch event, it will assert the IRQ and respond to I2C reads for a short time.
* While it should be able to detect multiple finger events, this version of the chip always returns only a single finger event and a gesture.

Reading the display data multiple times during a single event will return the last sampled finger position.

## Add to project

Packages from this repository are uploaded to [Espressif's component service](https://components.espressif.com/).
You can add them to your project via `idf.py add-dependancy`, e.g.
```
    idf.py add-dependency esp_lcd_touch_cst816s==1.0.0
```

Alternatively, you can create `idf_component.yml`. More is in [Espressif's documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## Example use

Define a mutex for the touch and create it before initialize the touch:

```
static SemaphoreHandle_t touch_mux;

touch_mux = xSemaphoreCreateBinary();
```

Define a callback function used by ISR:

```
static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

Initialization of the touch component.

```
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_LCD_HRES,
        .y_max = CONFIG_LCD_VRES,
        .rst_gpio_num = CONFIG_LCD_TOUCH_RST,
        .int_gpio_num = CONFIG_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .interrupt_callback = touch_callback,
    };

    esp_lcd_touch_handle_t tp;
    esp_lcd_touch_new_i2c_cst816s(io_handle, &tp_cfg, &tp);
```

Read data from the touch controller and store it in RAM memory. It should be called regularly in poll.

```
    if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
        esp_lcd_touch_read_data(tp); // read only when ISR was triggled
    }
```

Get one X and Y coordinates with strength of touch.

```
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, touch_x, touch_y, touch_strength, &touch_cnt, 1);
```
