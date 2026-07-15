# ESP-Brookesia Matter Controller

[English Version](./README.md)

本应用把 **ESP RainMaker Matter Fabric 上的 client-only Matter Controller** 做到 ESP-Brookesia System Super 框架里：设备列表、灯具控制、房间分组、配网二维码和 Thread 拓扑都可以在屏幕上操作。

运行当前项目需要额外给 **ESP RainMaker** 打一份补丁，ESP-IDF / ESP-Matter / ESP-Brookesia 使用下文给出的官方分支即可。

## 目录

- [ESP-Brookesia Matter Controller](#esp-brookesia-matter-controller)
  - [目录](#目录)
  - [项目说明](#项目说明)
  - [依赖版本](#依赖版本)
  - [需应用patch的仓库：ESP RainMaker](#需应用patch的仓库esp-rainmaker)
  - [环境准备](#环境准备)
    - [1. ESP-IDF 6.2（ESP32-S31）](#1-esp-idf-62esp32-s31)
    - [2. ESP-Matter `release/v1.6`](#2-esp-matter-releasev16)
    - [3. 指向本地仓库](#3-指向本地仓库)
    - [4. 硬件与证书](#4-硬件与证书)
  - [编译、烧录与运行](#编译烧录与运行)
  - [使用说明](#使用说明)
  - [故障排除](#故障排除)
  - [参考文档](#参考文档)

## 项目说明

Matter Controller 运行在 [examples/system/super](../../examples/system/super) 示例中，和 Settings 一起作为内置应用启动。

能力概览：

- **RainMaker 配网与入网**：开机后展示配网二维码 / 手工配对码，用 ESP RainMaker App 将控制器加入用户的 Matter Fabric
- **设备列表与控制**：从 RainMaker 拉取同 Fabric 设备，支持开关、亮度、色温、色相 / 饱和度
- **房间分组**：在本机 UI 中把设备划入房间
- **实时状态**：订阅 Matter attribute report，刷新在线状态和灯具参数
- **Thread 拓扑页**：查看 Thread 相关节点信息（取决于当前 Fabric 中的设备）

该工程关闭 Matter Server（`CONFIG_ESP_MATTER_ENABLE_MATTER_SERVER=n`），走 RainMaker [client-only controller](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html) 路径：Wi-Fi 由 RainMaker / `network_provisioning` 配网，NOC 与设备列表来自 RainMaker 云端。

## 依赖版本


| 仓库                                                          | 使用的commit版本                                  | 是否需要补丁    |
| ----------------------------------------------------------- | -------------------------------------------- | --------- |
| [ESP-IDF](https://github.com/espressif/esp-idf)             | **6.2**（commit id: e37a7ae）                  | 否         |
| [ESP-Matter](https://github.com/espressif/esp-matter)       | `release/v1.6`（commit id: 31b76ad）           | 否         |
| [ESP RainMaker](https://github.com/espressif/esp-rainmaker) | `master`，基线提交 `c91229d4`                     | **是**，见下文 |
| ESP-Brookesia                                               | 本仓库 `feat/add_matter_controller_app`（v0.8 线） | 否（已包含本应用） |


版本依据：

- ESP-Brookesia `master` (v0.8) 对 ESP32-S31 依赖 ESP-IDF **6.2**，见 [ESP-Brookesia 版本说明](https://github.com/espressif/esp-brookesia/blob/master/README_CN.md)
- ESP32-S31 目前在公开渠道以 ESP-IDF `master` 支持，见 [ESP-IDF 版本选择](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html#which-version-should-i-start-with)
- 本示例 `examples/system/super/main/idf_component.yml` 将 `espressif/esp_matter` 固定为 `~1.6.0`

> [!NOTE]
> RainMaker 官方 `examples/matter/matter_controller` 文档推荐的是 ESP-IDF v5.5.3，**不适用于** 本 Brookesia / ESP32-S31 工程。请使用上表中的 IDF 6.2。

## 需应用patch的仓库：ESP RainMaker

补丁文件：

```
[patches/0001-feat-compatible-with-brookesia-matter-controller.patch](./patches/0001-feat-compatible-with-brookesia-matter-controller.patch)
```

基线：ESP RainMaker `master` 提交 `c91229d4`（`v1.15.0-31`，`matter-examples-revamp` 合并之后）。

补丁内容：

1. 将 `esp_matter` 依赖从 `~1.5.1` 升到 `~1.6.0`
2. 适配 connectedhomeip / ESP-Matter 1.6 API（`ScopedMemoryBuffer`、订阅回调签名、`controller::get_fabric_index()`）
3. 为 client-only 控制器补上 `generate_controller_noc_chain_with_csr()`，用 RainMaker 签发的 CSR 申请 NOC

```bash
git clone --recursive https://github.com/espressif/esp-rainmaker.git
cd esp-rainmaker
git checkout c91229d4
git submodule update --init --recursive

# PATCH 换成本文件的绝对路径
git apply /path/to/esp-brookesia/app/brookesia_app_matter_controller/patches/0001-feat-compatible-with-brookesia-matter-controller.patch
```

检查是否打上：

```bash
git apply --check /path/to/0001-feat-compatible-with-brookesia-matter-controller.patch
git status
```

如果 `--check` 失败，说明当前 RainMaker 树已经偏离 `c91229d4`。请先回到该提交再打补丁，不要在不相关的旧 tag（例如 `v1.15.0`）上硬打。

## 环境准备

### 1. ESP-IDF 6.2（ESP32-S31）

按 [ESP-IDF 快速入门](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s31/get-started/index.html) 安装，并编译一次 hello_world 确认工具链可用。

```bash
cd /path/to/esp-idf
./install.sh
source ./export.sh
```

### 2. ESP-Matter `release/v1.6`

按 [ESP-Matter Developing — ESP Matter Setup](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html#esp-matter-setup) 克隆并安装。本工程需要 **1.6**，不要用默认 `main` 的浅克隆顶掉版本：

```bash
# 先 source ESP-IDF export.sh
git clone -b release/v1.6 --depth 1 https://github.com/espressif/esp-matter.git
cd esp-matter
git submodule update --init --depth 1
cd ./connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --platform esp32 linux --shallow   # macOS 把 linux 换成 darwin
cd ../..
./install.sh
```

每个新终端都要导出环境：

```bash
cd /path/to/esp-idf && source ./export.sh
cd /path/to/esp-matter && source ./export.sh
export IDF_CCACHE_ENABLE=1
```

### 3. 指向本地仓库

`examples/system/super/CMakeLists.txt` 通过环境变量查找 RainMaker 和 ESP-Matter：

```bash
export RMAKER_PATH=/path/to/esp-rainmaker
```

同时把 `examples/system/super/main/idf_component.yml` 里两处 **开发机相对路径** 改成你自己的仓库（可用绝对路径）：

```text
espressif/esp_matter:
  version: "~1.6.0"
  override_path: /path/to/esp-matter
  require: public

espressif/esp_rainmaker:
  version: ">=1.0"
  override_path: /path/to/esp-rainmaker/components/esp_rainmaker
```

### 4. 硬件与证书

- 开发板：`esp32_s31_korvo1`（Matter Controller 主验证板）。
- 分区表 `examples/system/super/partitions_16m.csv` 含 `esp_secure_cert` 和 `fctry`。工程默认：
  - `CONFIG_ESP_RMAKER_NO_CLAIM=y`
  - `CONFIG_ESP_RMAKER_USE_ESP_SECURE_CERT_MGR=y`
  - `CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER=y`

#### 认领设备证书

RainMaker + Matter 示例不能使用 Self Claiming 或 Assisted Claiming，因为 Matter 配网开始前证书就必须已经存在。因此请通过 [RainMaker CLI](https://github.com/espressif/esp-rainmaker-cli/blob/master/docs/README.md#installation) 使用 [主机侧认领（host driven claiming）](https://docs.rainmaker.espressif.com/docs/product_overview/concepts/claiming#host-driven-claiming-publicprivate-rainmaker)。

确保设备已连接到主机，登录 CLI 后执行：

```bash
esp-rainmaker-cli claim --matter {port}
```

这会获取设备证书并烧录到设备上。

#### 生成 factory NVS 二进制

factory NVS（`fctry` 分区）需要用 [esp-matter-mfg-tool](https://github.com/espressif/esp-matter-tools) 生成。

该工具已发布到 PyPI：[esp-matter-mfg-tool](https://pypi.org/project/esp-matter-mfg-tool)，可通过 `pip install esp-matter-mfg-tool` 安装。

```bash
esp-matter-mfg-tool --vendor-id 0x131B --product-id 0x2 \
                    --vendor-name "Espressif" --product-name "RainMaker-Matter-Light" \
                    --hw-ver-str "DevKitM1" \
                    -cd $RMAKER_PATH/examples/matter/mfg/cd_131B_0002.der \
                    --csv $RMAKER_PATH/examples/matter/mfg/keys.csv \
                    --mcsv $RMAKER_PATH/examples/matter/mfg/master.csv
```

这会生成 Matter 所需的 factory NVS 二进制，并通过 `master.csv` 把 RainMaker MQTT Host URL 写入其中。也可以选择把 MQTT host 编进固件：`idf.py menuconfig` → **ESP RainMaker Config** → `ESP_RMAKER_READ_MQTT_HOST_FROM_CONFIG`，然后调用 mfg-tool 时省略 `--csv` 和 `--mcsv`。

生成的 factory 二进制需要烧录到 `fctry` 分区。本工程分区表中该分区地址为 `0x1E000`（以 `examples/system/super/partitions_16m.csv` 为准）。

```bash
esptool.py write_flash 0x1E000 out/131b_2/{node-id}/{node-id}-partition.bin
```

## 编译、烧录与运行

开发环境总览也可参考：

- [ESP-Brookesia 编程指南 - 版本说明](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-versioning)
- [ESP-Brookesia 编程指南 - 如何使用示例工程](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html#getting-started-example-projects)

```bash
cd /path/to/esp-brookesia/examples/system/super

idf.py gen-bmgr-config -b esp32_s31_korvo1
idf.py build
idf.py -p <PORT> flash monitor
```

退出串口监视：`Ctrl-]`.

首次或改过分区表时建议擦除再烧录：

```bash
idf.py -p <PORT> erase-flash flash monitor
```

成功启动的串口标志：

```text
=== System Example Completed ===
```

然后会进入 System Super 桌面。打开 **Matter Controller** 图标即可。

可选功能（音频、视频、App Store 等）默认关闭。如需打开：`idf.py menuconfig` → **Super Example Optional Features**。

## 使用说明

1. 烧录后启动 Matter Controller。未入网时，**Commissioning** 页显示二维码和手工配对码。
2. 用 [ESP RainMaker App](https://rainmaker.espressif.com/docs/get-started.html)（[Android](https://play.google.com/store/apps/details?id=com.espressif.rainmaker) / [iOS](https://apps.apple.com/app/esp-rainmaker/id1497491540)）扫描二维码完成配网，并选择控制器所在的 Matter 组 / Fabric。
3. 控制器自动安装 RainMaker 签发的 Administer NOC，并拉取同 Fabric 设备列表。
4. 在 **Devices** 页开关灯具、调亮度 / 色温 / 颜色；在 **Rooms** 页分组。
5. 也可用设备控制台（前缀 `matter esp controller`），说明见 [Matter Controller 文档](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html)。

## 故障排除

**`ESP RainMaker repo not found` / `ESP Matter repo not found`**

未设置 `RMAKER_PATH` / `ESP_MATTER_PATH`，或路径不存在。导出变量后重新 `idf.py fullclean && idf.py build`。

**`git apply` 拒绝补丁**

RainMaker 不在 `c91229d4`。先 `git checkout c91229d4` 再 apply。

**Component Manager 仍解析到 `esp_matter` 1.5.x**

`idf_component.yml` 的 `override_path` 还指向旧树，或未改成你的 1.6 仓库。

**启动后白屏**

确认 `littlefs_data.bin` 已生成并烧录。System Super 依赖构建流程打包 `system/super`、`system/fonts` 和 `apps` 资源。

**App 能开但设备列表为空**

RainMaker 配网是否完成、是否选对 Matter 组、终端设备是否已在同一 Fabric。看日志里 `rainmaker_started`、`network_provisioned` 以及 device-list 回调。

**Wi-Fi 起不来 / 与 Brookesia Wi-Fi 服务冲突**

该项目默认关闭 `CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER`，由 RainMaker 接管 Wi-Fi 生命周期。不要在 menuconfig 里重新打开后又不配 RainMaker provisioning。

## 参考文档

- [ESP-Brookesia Getting Started](https://docs.espressif.com/projects/esp-brookesia/zh_CN/latest/getting_started.html)
- [ESP-Matter Controller](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/controller.html)
- [ESP-Matter Developing](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/developing.html)
- [ESP RainMaker Matter Controller 示例](https://github.com/espressif/esp-rainmaker/tree/master/examples/matter/matter_controller)
- [ESP RainMaker 入门](https://rainmaker.espressif.com/docs/get-started.html)
- [System Super 示例说明](../../examples/system/super/README_CN.md)
