# ESP32-P4-Function-EV-Board 运行 ESP-Brookesia Phone 示例

[English Version](./README.md)

这个示例演示了如何在 [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4-function-ev-board/index.html) 开发板上运行 ESP-Brookesia Phone，并使用 `1024 x 600` 分辨率的 UI 样式表。

## 入门指南

### 硬件要求

* 一个配有 `1024 x 600` 分辨率 LCD 屏幕的 ESP32-P4-Function-EV-Board 开发板。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.3 及更高的分支。默认情况下，它在 IDF release/v5.3 上运行。
- 请按照 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html) 设置开发环境。**我们强烈建议**您 [构建第一个项目](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#build-your-first-project)，以熟悉 ESP-IDF 并确保环境设置正确。

### 获取 esp-brookesia 仓库

要从 esp-brookesia 示例开始，请在终端运行以下命令，将仓库克隆到本地电脑：

```
git clone --recursive https://github.com/espressif/esp-brookesia.git
```

### 配置

  运行 ``idf.py menuconfig`` 并修改 esp-brookesia 配置。

## 如何使用示例

### 构建和烧录示例

构建项目并将其烧录到开发板，然后运行监视工具查看串行输出（将 `PORT` 替换为您的开发板串口名称）：

```c
idf.py -p PORT flash monitor
```

要退出串行监视器，请输入 ``Ctrl-]``。

完整的配置和使用 ESP-IDF 构建项目的步骤，请参见 [ESP-IDF 入门指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/get-started/index.html)。

### 示例输出

- 完整日志如下：

    ```bash
    I (25) boot: ESP-IDF v5.3-281-gdf5bf8c365 2nd stage bootloader
    I (26) boot: compile time Aug  9 2024 13:58:28
    I (26) boot: Multicore bootloader
    I (31) boot: chip revision: v0.1
    I (34) qio_mode: Enabling default flash chip QIO
    I (39) boot.esp32p4: SPI Speed      : 80MHz
    I (44) boot.esp32p4: SPI Mode       : QIO
    I (48) boot.esp32p4: SPI Flash Size : 16MB
    I (53) boot: Enabling RNG early entropy source...
    I (59) boot: Partition Table:
    I (62) boot: ## Label            Usage          Type ST Offset   Length
    I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (77) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (84) boot:  2 factory          factory app      00 00 00010000 00f00000
    I (93) boot: End of partition table
    I (96) esp_image: segment 0: paddr=00010020 vaddr=48090020 size=29ffc0h (2752448) map
    I (585) esp_image: segment 1: paddr=002affe8 vaddr=30100000 size=00020h (    32) load
    I (587) esp_image: segment 2: paddr=002b0010 vaddr=30100020 size=0003ch (    60) load
    I (593) esp_image: segment 3: paddr=002b0054 vaddr=4ff00000 size=0ffc4h ( 65476) load
    I (614) esp_image: segment 4: paddr=002c0020 vaddr=48000020 size=843dch (541660) map
    I (710) esp_image: segment 5: paddr=00344404 vaddr=4ff0ffc4 size=07c28h ( 31784) load
    I (719) esp_image: segment 6: paddr=0034c034 vaddr=4ff17c00 size=02c84h ( 11396) load
    I (727) boot: Loaded app from partition at offset 0x10000
    I (728) boot: Disabling RNG early entropy source...
    I (739) hex_psram: vendor id    : 0x0d (AP)
    I (740) hex_psram: Latency      : 0x01 (Fixed)
    I (740) hex_psram: DriveStr.    : 0x00 (25 Ohm)
    I (743) hex_psram: dev id       : 0x03 (generation 4)
    I (749) hex_psram: density      : 0x07 (256 Mbit)
    I (754) hex_psram: good-die     : 0x06 (Pass)
    I (760) hex_psram: SRF          : 0x02 (Slow Refresh)
    I (765) hex_psram: BurstType    : 0x00 ( Wrap)
    I (770) hex_psram: BurstLen     : 0x03 (2048 Byte)
    I (776) hex_psram: BitMode      : 0x01 (X16 Mode)
    I (781) hex_psram: Readlatency  : 0x04 (14 cycles@Fixed)
    I (787) hex_psram: DriveStrength: 0x00 (1/1)
    I (792) MSPI DQS: tuning success, best phase id is 2
    I (980) MSPI DQS: tuning success, best delayline id is 12
    I esp_psram: Found 32MB PSRAM device
    I esp_psram: Speed: 200MHz
    I (981) mmu_psram: flash_drom_paddr_start: 0x10000
    I (1194) mmu_psram: flash_irom_paddr_start: 0x2c0000
    I (1239) hex_psram: psram CS IO is dedicated
    I (1239) cpu_start: Multicore app
    I (1615) esp_psram: SPI SRAM memory test OK
    W (1625) clk: esp_perip_clk_init() has not been implemented yet
    I (1631) cpu_start: Pro cpu start user code
    I (1632) cpu_start: cpu freq: 360000000 Hz
    I (1632) app_init: Application information:
    I (1635) app_init: Project name:     esp_brookesia_phone_p4_function_ev_boa
    I (1642) app_init: App version:      7d4f9ae-dirty
    I (1647) app_init: Compile time:     Aug  9 2024 13:58:26
    I (1653) app_init: ELF file SHA256:  954eeeffe...
    I (1659) app_init: ESP-IDF:          v5.3-281-gdf5bf8c365
    I (1665) efuse_init: Min chip rev:     v0.1
    I (1670) efuse_init: Max chip rev:     v0.99
    I (1675) efuse_init: Chip rev:         v0.1
    I (1679) heap_init: Initializing. RAM available for dynamic allocation:
    I (1687) heap_init: At 4FF1CA50 len 0001E570 (121 KiB): RAM
    I (1693) heap_init: At 4FF3AFC0 len 00004BF0 (18 KiB): RAM
    I (1699) heap_init: At 4FF40000 len 00040000 (256 KiB): RAM
    I (1706) heap_init: At 50108080 len 00007F80 (31 KiB): RTCRAM
    I (1712) heap_init: At 3010005C len 00001FA4 (7 KiB): TCM
    I (1718) esp_psram: Adding pool of 26816K of PSRAM memory to heap allocator
    I (1726) spi_flash: detected chip: generic
    I (1730) spi_flash: flash io: qio
    W (1734) i2c: This driver is an old driver, please migrate your application code to adapt `driver/i2c_master.h`
    I (1746) main_task: Started on CPU0
    I (1769) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (1769) main_task: Calling app_main()
    I (1771) LVGL: Starting LVGL task
    I (1775) ESP32_P4_EV: MIPI DSI PHY Powered on
    I (1781) ESP32_P4_EV: Install MIPI DSI LCD control panel
    I (1786) ESP32_P4_EV: Install EK79007 LCD control panel
    I (1792) ek79007: version: 0.1.0
    I (1796) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (1962) ESP32_P4_EV: Display initialized
    E (1963) lcd_panel: esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel
    W (1964) GT911: Unable to initialize the I2C address
    I (1970) GT911: TouchPad_ID:0x39,0x31,0x31
    I (1974) GT911: TouchPad_Config_Version:89
    I (1979) ESP32_P4_EV: Setting LCD backlight: 100%
    I (1984) app_main: Display ESP BROOKESIA phone demo
    I (1990) esp-brookesia: [esp_brookesia_core.cpp:150](beginCore)Library version: 0.1.0
    I (2027) MEM:    Biggest /     Free /    Total
              SRAM : [  253952 /   378367 /   479363]
            PSRAM : [24641536 / 24997772 / 27459584]
    ```

- 以下动画展示了示例在开发板上运行的效果。

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_p4_function_ev_board_1024_600_2.gif" alt ="esp_brookesia_phone_p4_function_ev_board">
</p>

<p align="center">
（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_demo_1024_600_compress.mp4">点击查看视频</a>）
</p>

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
