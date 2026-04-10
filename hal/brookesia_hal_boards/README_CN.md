# ESP-Brookesia HAL Boards

* [English Version](./README.md)

## 概述

`brookesia_hal_boards` 是 ESP-Brookesia 的开发板配置集合，基于 `esp_board_manager` 组件以 YAML 描述各开发板的外设拓扑与设备参数，供 `brookesia_hal_adaptor` 在运行时无需硬编码即可完成硬件初始化。

## 目录

- [ESP-Brookesia HAL Boards](#esp-brookesia-hal-boards)
  - [概述](#概述)
  - [目录](#目录)
  - [支持的开发板](#支持的开发板)
  - [目录结构](#目录结构)
    - [board\_info.yaml](#board_infoyaml)
    - [board\_devices.yaml](#board_devicesyaml)
    - [board\_peripherals.yaml](#board_peripheralsyaml)
    - [sdkconfig.defaults.board](#sdkconfigdefaultsboard)
    - [setup\_device.c](#setup_devicec)
  - [使用方法](#使用方法)
    - [选择开发板](#选择开发板)
    - [添加自定义开发板](#添加自定义开发板)
  - [开发环境要求](#开发环境要求)
  - [添加到工程](#添加到工程)

## 支持的开发板

| 板级名称 | 芯片 | 描述 |
|----------|------|------|
| `esp_vocat_board_v1_0` | ESP32-S3 | ESP VoCat Board V1.0 — AI 宠物伴侣开发板 |
| `esp_vocat_board_v1_2` | ESP32-S3 | ESP VoCat Board V1.2 — AI 宠物伴侣开发板 |
| `esp_box_3` | ESP32-S3 | ESP-BOX-3 开发板 |
| `esp32_s3_korvo2_v3` | ESP32-S3 | ESP32-S3-Korvo-2 V3 开发板 |
| `esp32_p4_function_ev` | ESP32-P4 | ESP32-P4-Function-EV-Board |
| `esp_sensair_shuttle` | ESP32-C5 | ESP Sensair Shuttle 模块 |

## 目录结构

每块开发板在 `boards/<板级名称>/` 目录下包含以下文件：

```
boards/
└── <board>/
    ├── board_info.yaml          # 开发板元信息（名称、芯片、版本、制造商等）
    ├── board_devices.yaml       # 逻辑设备配置（音频编解码、LCD 显示、触摸、存储等）
    ├── board_peripherals.yaml   # 底层外设配置（I2C/I2S/SPI 总线、GPIO、LEDC 等）
    ├── sdkconfig.defaults.board # 开发板专用 Kconfig 默认值（Flash、PSRAM 等）
    └── setup_device.c           # 板级设备工厂回调（用于需要自定义驱动初始化的场景）
```

### board_info.yaml

记录开发板的基本元信息，供 `esp_board_manager` 识别和匹配：

```yaml
board: esp_vocat_board_v1_2
chip: esp32s3
version: 1.2.0
description: "ESP VoCat Board V1.2 - AI Pet Companion Development Board"
manufacturer: "ESPRESSIF"
```

### board_devices.yaml

以设备为单位描述板上的逻辑功能模块，常见设备类型包括：

| 设备类型 | 说明 |
|----------|------|
| `audio_codec` | 音频编解码芯片（DAC 播放 / ADC 录音），支持 ES8311、ES7210、内置 ADC 等 |
| `display_lcd` | LCD 显示屏，支持 SPI（ST77916、ILI9341）、DSI（EK79007）等接口 |
| `lcd_touch` | 触摸面板，支持 CST816S、GT911 等 I2C 触控芯片 |
| `ledc_ctrl` | 基于 LEDC 的 PWM 背光控制 |
| `fs_fat` / `fs_spiffs` | 文件系统存储，支持 SD 卡（SDMMC/SPI）和 SPIFFS |
| `camera` | 摄像头（CSI 接口） |
| `power_ctrl` | GPIO 供电控制（音频电源、LCD/SD 卡电源等） |
| `gpio_ctrl` | 通用 GPIO 控制（LED、按键等） |

### board_peripherals.yaml

描述底层硬件资源的引脚分配与参数，包括：

- **I2C**：SDA/SCL 引脚、端口号
- **I2S**：MCLK/BCLK/WS/DOUT/DIN 引脚、采样率、位深
- **SPI**：MOSI/MISO/CLK/CS 引脚、SPI 主机编号、传输大小
- **LEDC**：背光 GPIO、PWM 频率与分辨率
- **GPIO**：供电控制、功放使能、LED 等独立引脚配置

### sdkconfig.defaults.board

包含与开发板硬件强相关的 Kconfig 默认值，例如 Flash 大小、PSRAM 模式与频率、CPU 主频、数据缓存行大小以及 `brookesia_hal_adaptor` 的录音格式参数等：

```kconfig
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_BROOKESIA_HAL_ADAPTOR_AUDIO_CODEC_RECORDER_SAMPLE_RATE=16000
...
```

### setup_device.c

为需要自定义驱动初始化流程的设备提供工厂回调，例如向 LCD 驱动传入厂商特定的初始化命令序列：

```c
esp_err_t lcd_panel_factory_entry_t(
    esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *panel_dev_config,
    esp_lcd_panel_handle_t *ret_panel)
{
    // 注入 ST77916 的 vendor_config（自定义寄存器初始化序列）
    ...
}
```

## 使用方法

### 选择开发板

在工程根目录执行以下命令，指定目标开发板名称：

```bash
idf.py gen-bmgr-config -b <board>
```

可选的 `<board>` 值见[支持的开发板](#支持的开发板)。命令执行后，`esp_board_manager` 会读取对应的 YAML 配置并生成板级代码，随后正常编译即可：

```bash
idf.py -p PORT build flash monitor
```

> [!NOTE]
> 如果 `brookesia_hal_boards` 作为本地路径依赖引入，需通过 `-c <boards_dir>` 参数指定 `boards/` 目录的路径：
>
> ```bash
> idf.py gen-bmgr-config -b <board> -c path/to/brookesia_hal_boards/boards
> ```
>
> 在使用了 `idf_ext.py` 的示例工程中，该参数会在构建时自动注入，无需手动添加。

### 添加自定义开发板

在 `boards/` 目录（或任意自定义目录）下创建新的开发板子目录，按照现有板子的文件结构添加以下文件：

1. **`board_info.yaml`**：填写开发板名称、芯片型号、版本号和描述
2. **`board_peripherals.yaml`**：按实际引脚和总线配置填写外设参数
3. **`board_devices.yaml`**：按实际板载外设填写设备类型和配置
4. **`sdkconfig.defaults.board`**：添加与该板相关的 Kconfig 默认值
5. **`setup_device.c`**（可选）：如驱动需要额外初始化步骤则实现对应工厂函数

完成后执行 `idf.py gen-bmgr-config -b <new_board>` 即可使用。

> [!NOTE]
> 关于 `esp_board_manager` 配置格式的完整说明，请参考 [esp_board_manager 组件文档](https://github.com/espressif/esp-gmf/blob/main/packages/esp_board_manager/README_CN.md)。

## 开发环境要求

- [ESP-IDF](https://github.com/espressif/esp-idf)：`>=5.5,<6`

> [!NOTE]
> SDK 的安装方法请参阅 [ESP-IDF 编程指南 - 安装](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html#get-started-how-to-get-esp-idf)。

## 添加到工程

`brookesia_hal_boards` 已发布到 [Espressif 组件库](https://components.espressif.com/)，可通过以下方式添加：

1. **命令行**

   ```bash
   idf.py add-dependency "espressif/brookesia_hal_boards"
   ```

2. **idf_component.yml**

   ```yaml
   dependencies:
     espressif/brookesia_hal_boards: "*"
   ```

详细说明请参阅 [Espressif 文档 - IDF 组件管理器](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/tools/idf-component-manager.html)。
