# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
"""Run the real expansion scheduler, CameraSession and encoder with fake hardware."""
from ctypes.util import find_library
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run():
    here = Path(__file__).resolve().parent
    root = here.parents[4]
    compiler = os.environ.get("CXX") or shutil.which("g++-13") or "c++"
    with tempfile.TemporaryDirectory(prefix="brookesia-lifecycle-") as directory:
        build = Path(directory)
        stubs = build / "include"
        aliases = ["sdkconfig.h", "esp_err.h", "esp_log.h", "esp_heap_caps.h", "esp_system.h", "esp_timer.h",
                   "esp_board_device.h", "esp_board_manager.h", "esp_board_manager_defs.h",
                   "esp_board_periph.h", "esp_video_device.h", "dev_camera.h", "driver/gpio.h",
                   "video_processor.h", "impl/esp_capture_video_v4l2_src.h"]
        for name in aliases:
            target = stubs / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#include "sdk_stubs.hpp"\n')
        helpers = {
            "brookesia/lib_utils/describe_helpers.hpp":
                "#pragma once\n#define BROOKESIA_DESCRIBE_ENUM(...)\n#define BROOKESIA_DESCRIBE_STRUCT(...)\n",
            "brookesia/lib_utils/plugin.hpp":
                "#pragma once\n#define BROOKESIA_PLUGIN_REGISTER_PRE_MAIN_FUNCTION(...)\n",
            "brookesia/hal_interface/interface.hpp":
                "#pragma once\nnamespace esp_brookesia::hal { class Interface { public: "
                "explicit Interface(const char *) {} virtual ~Interface() = default; }; }\n",
            "brookesia/lib_utils/log.hpp": "#pragma once\n" + "\n".join(
                f"#define {name}(...) ((void)0)" for name in (
                    "BROOKESIA_LOGE", "BROOKESIA_LOGW", "BROOKESIA_LOGI", "BROOKESIA_LOGD",
                    "BROOKESIA_LOG_TRACE_GUARD", "BROOKESIA_LOG_TRACE_GUARD_WITH_THIS")) + "\n",
        }
        for name, contents in helpers.items():
            target = stubs / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(contents)
        adaptor = root / "hal/brookesia_hal_adaptor"
        utils = root / "utils/brookesia_lib_utils"
        mosaico = root / "hal/brookesia_hal_boards/components/mosaico/brookesia_hal_custom"
        sources = [here / "test_main.cpp", here / "test_mosaico_provider.cpp", adaptor / "src/expansion/runtime.cpp",
                   adaptor / "src/video/camera_impl.cpp", adaptor / "src/video/encoder_impl.cpp",
                   utils / "src/task_scheduler.cpp", utils / "src/task_scheduler_facade.cpp",
                   utils / "src/thread_config.cpp"]
        includes = [stubs, here, adaptor / "include", adaptor / "src",
                    root / "hal/brookesia_hal_interface/include", utils / "include",
                    mosaico / "include", mosaico / "src/expansion"]
        defines = ["CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT=1",
                   "CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_VIDEO_DEVICE=1",
                   "CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_CAMERA_IMPL=1",
                   "CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_ENABLE_ENCODER_IMPL=1",
                   "CONFIG_BROOKESIA_HAL_ADAPTOR_VIDEO_CAMERA_REQUIRES_EXPANSION_RUNTIME=1",
                   "CONFIG_BROOKESIA_HAL_ADAPTOR_ENABLE_EXPANSION_MODULES=1",
                   "BROOKESIA_HAL_ADAPTOR_EXPANSION_SCAN_INTERVAL_MS=10",
                   "BROOKESIA_UTILS_CHECK_HANDLE_METHOD=0", "BOOST_CHRONO_HEADER_ONLY=1"]
        command = [compiler, "-std=c++23", "-pthread", "-O0", "-g"]
        command += [f"-I{path}" for path in includes] + [f"-D{value}" for value in defines]
        command += [str(path) for path in sources]
        thread_library = find_library('boost_thread')
        command += ["-Wl,--wrap=open", "-Wl,--wrap=close", "-Wl,--wrap=ioctl",
                    f"-l:{thread_library}" if thread_library else "-lboost_thread",
                    "-lboost_system", "-o", str(build / "test")]
        subprocess.run(command, check=True)
        subprocess.run([str(build / "test")], check=True, timeout=30)
