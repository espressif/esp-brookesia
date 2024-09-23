# M5Stack CoreS3 运行 ESP-Brookesia Phone 示例

[English Version](./README.md)

这个示例演示了如何在 [M5Stack CoreS3](https://docs.m5stack.com/en/core/CoreS3) 开发板上运行 ESP-Brookesia Phone，并使用 `320 x 240` 分辨率的 UI 样式表。

## 入门指南

### 硬件要求

* 一个 M5Stack CoreS3 开发板。

### ESP-IDF 要求

- 此示例支持 IDF release/v5.1 及更高的分支。默认情况下，它在 IDF release/v5.1 上运行。
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
    I (26) boot: ESP-IDF v5.1.4-605-g5c57dfe949 2nd stage bootloader
    I (27) boot: compile time Aug 22 2024 17:56:35
    I (27) boot: Multicore bootloader
    I (27) boot: chip revision: v0.1
    I (28) qio_mode: Enabling default flash chip QIO
    I (28) boot.esp32s3: Boot SPI Speed : 80MHz
    I (28) boot.esp32s3: SPI Mode       : QIO
    I (28) boot.esp32s3: SPI Flash Size : 16MB
    I (29) boot: Enabling RNG early entropy source...
    I (29) boot: Partition Table:
    I (29) boot: ## Label            Usage          Type ST Offset   Length
    I (30) boot:  0 nvs              WiFi data        01 02 00009000 00006000
    I (30) boot:  1 phy_init         RF data          01 01 0000f000 00001000
    I (31) boot:  2 factory          factory app      00 00 00010000 00400000
    I (31) boot: End of partition table
    I (31) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=19c884h (1689732) map
    I (288) esp_image: segment 1: paddr=001ac8ac vaddr=3fc99d00 size=02a64h ( 10852) load
    I (290) esp_image: segment 2: paddr=001af318 vaddr=40374000 size=00d00h (  3328) load
    I (291) esp_image: segment 3: paddr=001b0020 vaddr=42000020 size=77510h (488720) map
    I (366) esp_image: segment 4: paddr=00227538 vaddr=40374d00 size=14f34h ( 85812) load
    I (392) boot: Loaded app from partition at offset 0x10000
    I (392) boot: Disabling RNG early entropy source...
    I (392) cpu_start: Multicore app
    I (393) esp_psram: Found 4MB PSRAM device
    I (393) esp_psram: Speed: 80MHz
    I (453) mmu_psram: Instructions copied and mapped to SPIRAM
    I (647) mmu_psram: Read only data copied and mapped to SPIRAM
    I (647) cpu_start: Pro cpu up.
    I (647) cpu_start: Starting app cpu, entry point is 0x4037570c
    0x4037570c: call_start_cpu1 at /home/lzw/esp/work/esp-idf-github/components/esp_system/port/cpu_start.c:159

    I (0) cpu_start: App cpu up.
    I (863) esp_psram: SPI SRAM memory test OK
    I (872) cpu_start: Pro cpu start user code
    I (872) cpu_start: cpu freq: 240000000 Hz
    I (872) cpu_start: Application information:
    I (872) cpu_start: Project name:     esp_brookesia_phone_m5stack_core_s3
    I (872) cpu_start: App version:      v0.1.1-8-gc62348c-dirty
    I (873) cpu_start: Compile time:     Aug 22 2024 20:03:20
    I (873) cpu_start: ELF file SHA256:  8a2a86890dacbd8c...
    I (874) cpu_start: ESP-IDF:          v5.1.4-605-g5c57dfe949
    I (874) cpu_start: Min chip rev:     v0.0
    I (874) cpu_start: Max chip rev:     v0.99
    I (874) cpu_start: Chip rev:         v0.1
    I (875) heap_init: Initializing. RAM available for dynamic allocation:
    I (875) heap_init: At 3FC9D840 len 0004BED0 (303 KiB): DRAM
    I (875) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
    I (876) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
    I (876) heap_init: At 600FE010 len 00001FD8 (7 KiB): RTCRAM
    I (877) esp_psram: Adding pool of 1920K of PSRAM memory to heap allocator
    I (877) spi_flash: detected chip: generic
    I (878) spi_flash: flash io: qio
    I (878) sleep: Configure to isolate all GPIO pins in sleep state
    I (878) sleep: Enable automatic switching of GPIO sleep configuration
    I (879) app_start: Starting scheduler on CPU0
    I (879) app_start: Starting scheduler on CPU1
    I (879) main_task: Started on CPU0
    I (880) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
    I (880) main_task: Calling app_main()
    I (881) LVGL: Starting LVGL task
    I (883) gpio: GPIO[35]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
    I (883) ili9341: LCD panel create success, version: 1.2.0
    I (1007) M5Stack: Setting LCD backlight: 100%
    I (1007) app_main: Display ESP BROOKESIA phone demo
    I (1007) app_main: Using external stylesheet
    I (1008) esp-brookesia: [esp_brookesia_core.cpp:150](beginCore)Library version: 0.1.0
    I (1087) app_main:    Biggest /     Free /    Total
              SRAM : [  155648 /   252259 /   406987]
            PSRAM : [ 1933312 /  1963116 /  1966080]
    ```

- 以下动画展示了示例在开发板上运行的效果。

<p align="center">
<img src="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_m5stace_core_s3.gif" alt ="esp_brookesia_phone_m5stace_core_s3" width="400">
</p>

<p align="center">
（<a href="https://dl.espressif.com/AE/esp-dev-kits/esp_ui_phone_m5stace_core_s3.mp4">点击查看视频</a>）
</p>

## 技术支持和反馈

请使用以下反馈渠道：

- 有技术问题，请访问 [esp32.com](https://esp32.com/viewforum.php?f=35) 论坛。
- 如需提交功能请求或错误报告，请创建 [GitHub issue](https://github.com/espressif/esp-brookesia/issues)。

我们会尽快回复您。
