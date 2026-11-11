# HAL 主机回归测试

本目录为 HAL 生命周期、板级清理和依赖补丁回归提供统一入口。在 Linux 上编译实际实现并替换硬件接口，不执行实机烧录。

## 环境要求

- Python 3.9+、C 编译器，以及支持 C++23 的编译器（GCC 13 或更新版本）。
- 核心测试需要 Boost 头文件、Boost.Thread 和 Boost.System 开发库。
- 依赖测试还需要 Git、CMake 3.22+、PyYAML、已有配置工程的 `managed_components` 目录，以及 `PATH` 中支持 C++20 的 `c++`。这些额外要求不适用于核心测试。

核心测试优先使用 `CXX`，否则选择 `g++-13` 或 `c++`。依赖组的音频测试直接调用 `c++`，不使用 `CXX`。以下命令均在仓库根目录执行。

## 核心回归

```bash
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite core
```

这是供开发者本地手动运行的默认测试组，未接入 CI。覆盖扩展事件顺序、回调移除、摄像头与编码器并发、清理失败后的资源所有权，以及 NAND/BQ27220/USB 初始化回滚。

此外还使用任意设备名称验证真实 Board Manager 调用入口，覆盖无内存分配的错误与重试分发，以及从静态库保留 Mosaico 启动注册。注册失败测试覆盖全部三种错误处理策略，以及启用和禁用断言的配置。

## 依赖回归

先按 [System Super 构建流程](../../../../examples/system/super/README_CN.md#-如何使用)配置固件工程，在 `examples/system/super/build/dependencies/managed_components` 中准备解析出的依赖及必要补丁。以下命令从仓库根目录创建独立 Python 环境，并仅运行依赖测试：

```bash
python3 -m venv /tmp/brookesia-hal-host-venv
source /tmp/brookesia-hal-host-venv/bin/activate
python3 -m pip install PyYAML "cmake>=3.22"
python3 hal/brookesia_hal_adaptor/test_apps/host/run.py --suite dependencies \
    --managed-components examples/system/super/build/dependencies/managed_components
deactivate
```

同时具备核心测试前提时，可改用 `--suite all`。依赖测试覆盖真实 av_processor 头文件下的播放/录音格式、播放优先级、回调及打开失败回滚，以及补丁应用、组件别名、重复配置、释放错误、帧模式停止和 UYVY/YUYV 协商。依赖由参数显式传入；运行器不会下载依赖或修改工程中的组件副本。

共享 I2S 用例执行 codec 库的配对、格式检查、`set_fmt` 和 `esp_codec_dev_open` 函数，并使用驱动、OS 和模拟 codec 控制桩。用例覆盖 16/48 kHz、默认 16/16 kHz、声道和位深不一致，并检查驱动格式与 manager 元数据是否相符；不验证物理 I2S 时钟或音质。

使用 Board Manager 0.5.15、esp_video 2.4.1、esp_capture 1.0.2 和 av_processor 0.7.0 运行依赖用例。升级 Board Manager 时，使用候选工程解析出的依赖运行用例，检查补丁及 API 差异，再执行核心回归和实机测试。

依赖组还执行实际 HAL CMake 注册块及 ESP-DL 官方生成器，覆盖 local/managed 组件别名、P4/S3/S31/C5、额外模型需求并集及 SR 版本边界。这些检查不验证模型推理结果、音质或硬件时延。

## 验证范围与清理

测试在临时目录中构建，结束时删除生成的源码和可执行文件。编译或用例失败会返回非零退出码。核心测试无需 ESP-IDF 即可运行；依赖测试需显式提供组件源码。

测试模拟硬件故障并控制线程执行顺序，不验证传感器寄存器、显示 DMA 时序、USB 枚举或物理热插拔行为。实机验证使用已有 HAL 测试工程和 System Super；Video Service 测试工程保留预览格式回归。
