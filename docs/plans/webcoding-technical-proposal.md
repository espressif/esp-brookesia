# Webcoding 技术方案（评审稿）

日期：2026-11-10

状态：**待用户审阅；尚未开始本方案的正式实现。** 用户已确认首期采用 LVGL，不做 GSP。本文其余接口、组件划分和实施顺序为待审阅方案。

本文是[产品蓝图](esp-mosaico-workshop-blueprint.md)的技术展开。仓库证据以本机 ESP-Brookesia `913da33aaaa5ede608e40cd272a25b0a6cd61e46` 检出及其工作区为基线；现有代码、历史样机记录和拟新增能力分别标明。

## 1. 目标与首期范围

用户在网页中与 AI 对话，得到一份 JavaScript 作品；在同一作品页试玩，满意后将**当前试玩的同一份代码**安装到 ESP-Mosaico V1.2。设备保存多个作品，用户可从桌面打开、退出和切换；断开电脑和重启后仍可运行。

首期交付包括：本地网页与服务、用户自带的模型 API、作品和对话的本地保存、同页预览、网页引导首次刷固件、日常通过 USB 安装或更新单个 JS 作品，以及 Mosaico 固件与应用桌面。一次只要求一个作品处于前台运行。

已有网页是用于审阅体验的初版交互原型，AI、账号和设备操作仍是演示。正式网页围绕该设计重新实现，按能力复用已有代码和验证结果，不整体迁移旧工坊。

账号、云端项目同步、个人主页的真实发布和他人通过链接安装作品属于后续在线阶段；首期保留清楚的产品入口和项目数据边界，不把演示状态呈现为已上线。GSP、后台多应用并行、通用网页 DOM/CSS 和外部图片/音频资源导入不属于首期。后续可扩展资源形式，但首期作品以一个 JS 文件为交付对象。

## 2. 已有基础与缺口

| 部分 | 仓库中已有 | 仍需完成 |
| --- | --- | --- |
| JS 运行 | `brookesia_runtime_js` 集成 QuickJS-NG，可从文件读取 JS、注册宿主函数并执行生命周期回调。 | 作品描述约定、受限的描述提取环境、执行中断和产品接口。 |
| 界面 | JSON UI 可内嵌资源对象；LVGL 后端渲染 JSON UI；`SystemGui` 支持已有界面的状态更新、事件和基于模板创建视图。 | 从 JS 内的描述生成并验证界面；首期不承诺任意动态创建整棵界面。 |
| 安装与桌面 | Core 可安装 runtime manifest，也能在启动时扫描未打包的应用目录；Super 安装应用后刷新桌面。 | 接收单 JS 后自动准备目录、在线注册和安全替换；对接 Webcoding 的作品身份。 |
| 浏览器预览 | Toolkit 有 JSON UI/LVGL 的 WASM 浏览器模拟器；JS runtime 的 WASM 构建包含 QuickJS-NG。 | 在当前作品页加载单 JS、接入同一适配层、同步事件及错误。 |
| Mosaico | Super 示例选择 LVGL，已有 V1.2 板型配置及部分桌面、触摸的历史实板记录。 | 精简组合产品固件；验证本产品的性能、存储、单 JS 运行和 USB 下发。 |
| USB | Brookesia USB service 有分帧、校验、上传及 `.bpk` 安装流程。 | Mosaico 板载 TinyUSB CDC 默认未接入该 service；需要新传输适配和单 JS 安装命令。 |

主要证据：[JS 加载与执行](../../runtime/brookesia_runtime_js/src/backend.cpp#L657)、[runtime 应用安装与启动顺序](../../docs/zh_CN/system/core/app_model.rst#L111)、[未打包应用目录](../../docs/zh_CN/system/core/app_package.rst#L12)、[界面操作服务](../../docs/zh_CN/system/core/services.rst#L72)、[Super 示例及 V1.2 记录](../../examples/system/super/README_CN.md#L33)、[Mosaico USB 限制](../../examples/system/super/README_CN.md#L104)、[WASM 中的 QuickJS 构建](../../runtime/brookesia_runtime_js/cmake/pc_platform.cmake#L184)。这些证据不代表单 JS 链路已实现或通过实板验收。

## 3. 总体架构

```text
                         Webcoding 网页（同一作品页）
    AI 对话 ──► 项目 JS / 版本 ──► 同页预览 ──► 连接设备 / 安装
                   │                  │                │
             本地项目服务        WASM 运行环境       Web Serial
             记录项目和对话      作品适配层           │
                   │             QuickJS + LVGL        │
               外部模型 API                            ▼
                                              Mosaico 产品固件
                                              TinyUSB / 安装服务
                                              作品适配层
                                              Core / Super / QuickJS / LVGL
                                                        │
                                                显示、触摸、存储
```

作品适配层位于**应用框架与 JS 作品之间**，并非新的解释器或绘图库。它负责作品描述、内部应用资料生成、生命周期和界面事件对接；屏幕绘制和硬件驱动继续使用 Brookesia。适配层中与平台无关的代码应同时编译到设备与 WASM；文件、时钟和连接通过各自的宿主实现，不能假定设备代码可原样在浏览器运行。

固件建议新建 `examples/system/webcoding/`，其中组装 Mosaico 板级能力、必要的 Brookesia 服务、QuickJS、JSON UI/LVGL、Super 和 Webcoding 产品组件。优先复用组件及扩展点；确有跨产品价值或现有公共接口不够时，再小范围调整 Core/runtime。网页和本地服务建议在上层 Toolkit 仓库新建 `packages/webcoding/`，按前端、服务和共享接口组织，使用现有 pnpm workspace；不把用户作品放进固件工程。

## 4. 单 JS 作品约定

首期作品为 UTF-8 编码的单个 `app.js`。它同时包含界面描述和交互函数：界面数据是 JS 对象，遵循约定的 JSON UI 子集；交互逻辑使用 Brookesia 已有的 `brookesia_app` 回调及产品封装的受控接口。作品不依赖浏览器 `document`、HTML/CSS、Node.js 或外部 JS 模块。

下面展示拟议的**文件结构**，只画出一个文本，事件与样式省略；`webcoding_app.describe()` 是本方案新增约定，尚未存在，示例不能作为当前产品直接运行：

```js
globalThis.webcoding_app = {
  describe() {
    return JSON.stringify({
      apiVersion: 1,
      name: "专注计时器",
      startupFlow: "main",
      ui: {
        version: "0.1.1",
        assets: [
          { type: "viewScreen", id: "main", children: [
            { type: "label", id: "time", labelProps: { text: "25:00" } }
          ] },
          { type: "screenFlow", id: "main", screens: ["main"], initial: "main" }
        ]
      }
    });
  }
};

globalThis.brookesia_app = {
  on_start() { /* 初始化业务状态 */ return true; },
  on_action(action, path, payload) { /* 处理点击 */ },
  on_timer(timerId, name) { /* 更新计时 */ }
};
```

`describe()` 返回 JSON **字符串**，因为现有 native/runtime 桥的值类型包含字符串，但不直接传递任意 JS 对象。`ui` 最终是带内嵌 `viewScreen`、`screenFlow` 等资源的 JSON UI 根文档；`startupFlow` 用于生成启动描述。实际字段及允许的控件以首个可运行验证确定并版本化，不把上面的草图当成已实现 API。

首期覆盖文本、按钮、基础布局与样式、触摸动作、数值更新和定时；视图位置、颜色等变化通过已声明的 binding，动态实例通过已声明的模板。能力随实测扩展，AI 提示词和校验器使用同一份版本化接口说明。设备内置字体和基础图标；作品引用外部资源的规则另行设计。

作品的稳定 ID 由 Webcoding 项目持有，名称可修改；每次生成或编辑形成不可变修订，保存 JS 字节及 SHA-256。试玩对应一个修订；安装固定使用该次试玩的相同字节和摘要，编辑过程中不悄悄切换安装对象。协议另传 ID、版本和摘要等小量元信息，作品内容仍只传一个 JS 文件。

## 5. 设备端作品适配与安装

### 5.1 从 JS 到 Brookesia 应用

1. 接收 JS 到设备临时目录并校验大小与摘要，不修改已安装作品。
2. 在受限的临时 QuickJS 环境中执行作品声明，调用 `webcoding_app.describe()`，取得 JSON 字符串。脚本本身不能调用设备服务或访问文件和网络，并限制执行时间、内存和微任务；允许的模块加载范围须显式实现。宿主仍需读取已暂存的 JS 文件。
3. 校验作品 API 版本、名称、启动 flow 和 JSON UI，生成内部 `manifest.json`、`res/profile.json`、`res/root.json`，连同原始 `app.js` 组成未打包的暂存应用目录。目录和文件是设备内部管理细节，不需要 `.bpk`。
4. 校验通过后调用 Core 应用安装能力并刷新桌面；启动时由 Core 先加载生成的 GUI，再启动同一份 JS，并调用 `brookesia_app.on_start()`。触摸动作和 timer 沿现有 SystemGui/SystemTimer 路径回调 JS。

同一文件在描述提取和正式打开时会分别执行。作品约定要求顶层只声明数据与函数，描述不得依赖时间、随机数或设备状态；业务副作用在生命周期回调中执行。网页和设备对相同 JS 得到的描述必须一致：对 `describe()` 返回字符串的 UTF-8 字节计算 SHA-256，预览保留摘要，设备重新提取后比对；不一致时拒绝安装并提示重新生成，而不是安装与预览不同的界面。

现有 `JS_SetMemoryLimit` 和栈限制不是完整的执行保护；现有 runtime 还会配置模块加载器，且微任务会循环排空。实现准备环境时需限制导入及宿主能力，并对 JS 执行和微任务总量设置可中断的预算；正式运行也要能处理死循环、异常和资源耗尽。QuickJS 的中断回调用于 JS 执行超时，原生服务调用仍需自身超时。数值上限以 Mosaico 实测决定。相关依据：[JS 运行与模块加载](../../runtime/brookesia_runtime_js/src/backend.cpp#L657)、[微任务循环](../../runtime/brookesia_runtime_js/src/backend.cpp#L110)、[QuickJS 中断接口](https://github.com/quickjs-ng/quickjs/blob/master/docs/docs/developer-guide/intro.md#execution-timeout-and-interrupts)。

### 5.2 登记、替换与数据

每个项目的稳定 ID 对应设备内一个应用，更新替换同一应用，其他作品不受影响。设备重新启动后从内部应用目录扫描恢复。卸载是单独动作，并明确会删除该作品的设备数据。

安装提交前要在 GUI 调度上下文中使用实际 JSON UI 解析/验证能力检查生成的文档和启动 flow，检查后释放，不显示暂存页面。现有 Core 的安装预检查只确认根文件是 JSON 对象等基本条件，不能把这一步当成完整界面验证：[实现](../../system/brookesia_system_core/src/app/package.cpp#L962)。

更新采用暂存、校验、停止旧应用、保留最终数据、替换、注册和提交标记；中途失败或掉电按提交状态恢复旧版本。优先将现有 `.bpk` 更新的事务能力抽出供未打包目录复用，而不是对上传目录直接覆盖。更新时不应把旧应用当成用户主动卸载而重复执行会删除数据的卸载流程。

**需要先复现的源码问题：** 当前检出中的 [BPK 更新实现](../../system/brookesia_system_core/src/app/app.cpp#L1250)先复制旧应用数据，之后才[停止旧应用](../../system/brookesia_system_core/src/app/app.cpp#L1319)。如果 `on_stop` 写入数据，最后写入可能未进入新版本；这是源码推断，尚未在本次复现。复用前应以回调写文件的用例验证并修正顺序，同时验证替换时的 `on_uninstall` 行为。

### 5.3 固件与存储

产品固件包含 Super 桌面、QuickJS、JSON UI/LVGL、显示触摸、存储、USB 和作品适配组件；其余服务按作品能力需要启用。首次烧录的交付物应包含与分区表对应的固件和必要资源镜像；日常作品安装不重新烧录固件。

先以 Super 的 NOR/LittleFS 布局为工作基线，测量固件、字体、桌面资源和应用可用空间后确定产品分区。当前示例的分区值只是现有配置，不作为产品容量承诺；V1.2 的 NAND 虽已接入 HAL/Storage，当前示例尚不能将其选为应用安装位置。[现有分区](../../examples/system/super/partitions_16m.csv)、[V1.2 限制](../../examples/system/super/README_CN.md#L38)。

## 6. 网页、本地服务与预览

建议新建 TypeScript Webcoding 应用：React 前端负责创作页、项目、同页预览、设备和个人主页入口；Node.js 本地服务负责项目/对话/版本持久化及模型请求代理，初期以 SQLite 保存索引与会话，JS 修订按项目保存。页面继续支持桌面和手机尺寸；设备连接首期以桌面 Chrome 为验证目标。采用现有工具链可复用的依赖和版本，但不迁移旧工坊整套产品。

AI 接入先实现可配置 Base URL、模型名和 API Key 的 OpenAI 兼容接口；其他提供方通过明确的适配器扩展，不承诺所有 API 格式自动兼容。Agent 只获得当前项目的读取、修改、校验及预览等受限工具：每次修改先生成候选修订，校验/预览失败保留上一个可用版本，并把错误反馈给对话。模型未配置或调用失败时给出实际状态，不生成假的成功回复。

API Key 只供本机服务访问外部模型，优先存入系统凭据存储，避免写入项目、对话、网页代码或日志；若系统凭据存储不可用，提供明确告知用户的受限本机配置文件方案，使用户下次打开无需重新填写。本地服务默认只监听本机并限制来源。模型回复支持安全的 Markdown、代码块和列表渲染。公开部署时另行设计账号、租户隔离及凭据管理。

预览以现有 WASM 模拟器为底座，将相同的作品适配逻辑、QuickJS 和 JSON UI/LVGL 引入作品页；由页面向模拟器传入当前 JS 修订，使用同一份作品描述生成界面，而不是重写一套近似 DOM 画面。现有模拟器以独立页面及应用包为主要入口，同页嵌入和单 JS 输入需要新增。预览与创作页之间要隔离执行并支持中断，避免作品死循环使整个工作台失去响应。模拟器使用虚拟设备服务，不能代替 Mosaico 的触摸、字体、性能和内存验证。

## 7. 连接、刷机与作品下发

首期面向桌面 Chrome，通过 Web Serial 连接；本地工作台由 `localhost` 安全上下文提供，线上版本使用 HTTPS，页面在用户操作后申请串口权限。首次安装固件和后续安装作品呈现为一个连续引导，但内部是两种操作：

- **首次固件安装：** 使用预编译镜像及其分区地址，优先评估 Espressif 的 `esptool-js`。Mosaico V1.2 当前样板需手动 BOOT 进入下载模式；刷入后设备重启、USB 重新枚举，页面重新识别并在需要时请求用户选择端口。必须先验证所选 `esptool-js`、S31 和板载端口的组合。首次刷固件可能覆盖设备原有固件及相关分区，网页上须在执行前说明；本机项目和对话单独保存。
- **作品安装：** 固件通过握手声明自身身份、协议/API 版本、板型和可用空间，网页发送当前预览修订的 JS、摘要和项目 ID。设备完成校验、生成、注册并确认后才显示安装成功。未能握手不自动认定可以覆盖设备固件，应让用户选择后续操作。

Mosaico 产品固件优先验证同一条 Type-C 线上的双 CDC：一个用于 console，另一个用于作品协议；ESP USB 配置支持一至两个 CDC 通道，但板级默认只有一个。当前板级 console 自己初始化 TinyUSB 驱动，两个通道必须共用一次驱动初始化并避免多个读取者争抢同一通道。[板级实现](../../hal/brookesia_hal_boards/components/mosaico/esp_mosaico_usb_console/src/esp_mosaico_usb_console.c#L131)、[ESP32-S31 USB 配置](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s31/usb_device.html)。双 CDC 的描述符、端点、操作系统枚举和浏览器选择体验均需实板验证；若不可用，再验证单通道由一个接收者管理，并对日志和控制协议进行分帧或独占，避免互相干扰。

作品传输优先复用现有 USB service 的分帧、校验、ACK 和临时文件机制，新增 TinyUSB 传输后端及“安装 JS”桥接；不假设现有 `install_bpk` 已支持它。传输使用请求 ID、长度和 SHA-256，结果区分接收、准备、校验、安装和提交。断线或结果不明时先查询设备上的作品 ID 与摘要，再决定是否重试，不能仅凭文件传完报告安装成功。Web Serial 需要浏览器用户操作选择端口：[Chrome 接口说明](https://developer.chrome.com/docs/capabilities/serial)。

## 8. 实施顺序与验收

用户审阅通过后按可运行结果推进，不以目录或代码量计算完成：

1. **验证作品约定。** 实现受限描述提取与内部目录生成；用一个包含文字、按钮、点击和定时的 JS，在现有 JSON UI/LVGL WASM 环境运行。验证语法错误、非法描述、执行超时及旧版本保留。
2. **验证设备运行。** 组装 Mosaico Webcoding 固件，用与预览相同的 JS 验证界面、触摸和定时；确认返回桌面、多个作品与重启恢复。记录实际固件体积、内存及不同生命周期下的资源释放。
3. **打通网页下发。** 完成 TinyUSB 接入、浏览器握手和 JS 安装/更新；验证当前预览摘要与设备安装摘要一致、传输中断和更新失败后的恢复，以及拔线后的运行。
4. **接入真实创作。** 用本地模型 API 驱动项目修订、校验和同页预览；保留作品与对话，并在真实工作台中走通“描述、调整、试玩、安装”。

验收分别记录：Host 单元/集成测试、浏览器实际渲染与交互、ESP-IDF 构建、真实 Mosaico 烧录及触摸验证。模拟设备服务不能冒充真实硬件；不能将界面预览或构建成功当成端到端完成。

## 9. 审阅时的关键判断

1. 是否接受首期单 JS 作品采用“**界面描述 + 回调逻辑**”约定，并由设备生成内部 JSON 文件？这保留单文件下发，但不支持直接运行任意网页。
2. 是否接受首个可用范围集中在触屏小工具和轻量游戏，先使用内置字体与图标，外部媒体资源后续设计？
3. 浏览器和设备共用适配层及 QuickJS/LVGL 作为一致性基础；Mosaico USB 双 CDC 和首次刷机仍是实板验证项，不能在当前方案阶段保证已打通。

这些是本方案的审阅点，不代表已有功能或实板结果。在线账号与个人主页的正式架构在本地作品链路完成后继续设计。
