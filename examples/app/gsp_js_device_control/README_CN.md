# 设备控制

[English](./README.md)

设备控制（Device Control）是一个面向 ESP-Brookesia GSP 界面的示例应用，用于显示屏幕亮度并通过滑块调节。页面是 GSP 场景，逻辑是 JavaScript，修改后通过 USB 安装新版本即可，无需重新编译或烧录固件。

## 功能

- 状态页，以及带滑块的亮度页
- 通过 Display 服务读取和设置背光
- 开发板运行时通过 USB 安装、更新和卸载
- 也可编入固件镜像，作为预装应用

## 环境要求

- 运行 [System GSP App](../../system/gsp_app/README_CN.md) 并使用 GSP 界面（默认）的 ESP32-P4 Function EV Board
- Python 3.10 或更新版本，并已激活其环境
- 以下命令使用 Bash 或 Zsh

在本应用目录下执行。安装工具管理器和 USB CLI，再获取匹配的 GSPC 编译器并导出路径：

```bash
python3 -m pip install -U esp-gsp-tools
python3 -m pip install brookesia-usb
python3 -m gsp.manager install 0.5.0 gspc
export GSPC_EXECUTABLE="$(python3 -m gsp.manager path 0.5.0 gspc)"
"$GSPC_EXECUTABLE" --version
```

管理器会下载适用于当前主机的发行版。构建、更新和预览时沿用此环境；新开终端后，激活相同的 Python 环境并重新执行 `export` 命令。编译器应输出 `gspc 0.5.0`。

## 快速开始

在本目录下执行以下命令。

1. 构建安装包：

   ```bash
   python3 tools/build_package.py --version 0.1.0
   ```

   安装包生成在 `build/brookesia.gsp.device_control.debug.0.1.0.bpk`。

2. 连接开发板的 USB Serial/JTAG 接口，并关闭占用该端口的串口监视器。

3. 安装：

   ```bash
   brookesia-usb install build/brookesia.gsp.device_control.debug.0.1.0.bpk
   ```

启动页的 **Settings** 旁会出现 **Device Control**。点击进入，再点击 **Brightness** 拖动滑块。**Back** 返回状态页，**Exit** 返回启动页。

## 更新

用更高的版本号构建，并以同样方式安装：

```bash
python3 tools/build_package.py --version 0.2.0
brookesia-usb install build/brookesia.gsp.device_control.debug.0.2.0.bpk
```

新版本会替换旧版本，并保留应用数据。若安装包损坏或与固件不匹配，安装会被拒绝并给出原因，已安装的版本继续可用。

## 卸载

先查到应用的 `app_id`，再卸载：

```bash
brookesia-usb call SystemCore ListApps '{}'
brookesia-usb call SystemCore UninstallApp '{"AppId": <app_id>}'
```

卸载会同时删除应用数据。每次安装后 `app_id` 都会变化，请先列出应用。

## 预装到固件

通常通过 USB 安装本应用。如需出厂即带有该应用的固件镜像，先激活 ESP-IDF 6.0 或更新版本，并按 [System GSP App 构建说明](../../system/gsp_app/README_CN.md#构建与烧录)准备 GSP 宿主。从本应用目录进入宿主目录，配置已有的 GSP 输出：

```bash
cd ../../system/gsp_app
idf.py -B build-gsp menuconfig
```

开启 **Brookesia system GSP example → Preinstall the Device Control app**。若宿主使用其他输出目录，将所有命令中的 `build-gsp` 替换为该目录。构建时会编译安装包并放入 LittleFS；完整烧录**会替换所有通过 USB 安装的应用及其数据**：

```bash
idf.py -B build-gsp build
idf.py -B build-gsp -p <PORT> -b 921600 flash monitor
```

将 `<PORT>` 替换为开发板串口。使用 USB CLI 前，通过 `Ctrl+]` 退出监视器。`idf.py -B build-gsp app-flash` 不会改动 LittleFS，因此既不会加入也不会移除预装应用。预装应用仍可通过 USB 更新或卸载。

## 自定义

| 路径 | 内容 |
| --- | --- |
| `package/app/app.js` | 应用逻辑 |
| `scenes/status.json`、`scenes/brightness.json` | 两个页面 |
| `package/manifest.json` | 应用 ID、名称、版本和所需服务 |
| `tools/build_package.py` | 编译页面并生成 `.bpk` |

修改后用新的 `--version` 构建并安装。没有开发板时，在本应用目录中沿用上述工具环境，先获取匹配的模拟器并导出路径，再预览页面布局：

```bash
python3 -m gsp.manager install 1.4.0 sim
export GSP_SIM_EXECUTABLE="$(python3 -m gsp.manager path 1.4.0 sim)"
python3 tools/build_package.py --preview
```

新开终端后需重新导出两个工具路径。预览只显示布局，不会运行 `app.js`。在终端按 `Ctrl+C` 结束预览。

### JavaScript 接口

`app.js` 为 `globalThis.brookesia_app` 提供以下回调：

| 回调 | 调用时机 |
| --- | --- |
| `on_start()` | 应用打开。返回 `false` 或抛出异常会取消启动 |
| `on_stop()` | 应用关闭 |
| `on_action(action, path, payloadJson)` | 控件被操作（`gsp.call`），或请求的页面已显示（`gsp.scene`） |
| `on_event(serviceName, eventName, itemsJson)` | 收到已订阅的服务事件 |

`gsp.call` 的载荷包含 `arg` 和 `key`，`key` 是控件 `callback` 名称的 FNV-1a 哈希，计算方法见 `app.js`。新页面的控件在收到 `gsp.scene` 后才存在，请在此时更新它们。

| 函数 | 作用 |
| --- | --- |
| `brookesia.gsp_set_text(name, text)` | 设置场景中带 `bind` 的标签文本 |
| `brookesia.gsp_set_value(name, value)` | 设置滑块数值 |
| `brookesia.gsp_set_property(name, property, value)` | 设置场景声明过的属性,如 `visible`、`enabled`、`color`(`"#RRGGBB"`)或时钟指针角度 |
| `brookesia.gsp_show_page(index)` | 按构建顺序显示页面 |
| `brookesia.call_service_function(service, function, argsJson, timeoutMs)` | 调用 `manifest.json` 中声明的服务 |

场景更新会缓存以便从模态界面恢复，每个场景的缓存有容量限制；无效或超限的更新会返回错误。
具体限制和切页行为见 [System Core GSP 说明](../../../system/brookesia_system_core/README_CN.md)。

## 常见问题

| 现象 | 处理方法 |
| --- | --- |
| `brookesia-usb` 超时或端口被占用 | 关闭占用开发板串口的程序 |
| 重启后应用列表为空 | 等启动页出现后重试 |
| 安装失败并提示 GUI backend | 固件使用 LVGL 界面，请构建并烧录 GSP 界面 |
| 构建时找不到 `gspc` | 设置 `GSPC_EXECUTABLE`，或使用 `--gspc <path>` |
| 点击应用后又回到启动页 | `on_start` 失败，原因见串口日志 |

## 限制

- 仅能在使用 GSP 界面的固件上运行。
- Figma 导出生成的是 LVGL JSON UI 包，不能生成 GSP 页面。
