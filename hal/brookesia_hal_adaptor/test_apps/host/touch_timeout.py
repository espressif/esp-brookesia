# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise the P4X GT911 release workaround with two fake controllers."""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[4]

PREAMBLE = r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_STATE=-1;
struct Touch;
using esp_lcd_touch_handle_t=Touch *;
struct Touch {
    struct Data {int lock=0;uint8_t points=0;} data;
    esp_err_t (*read_data)(Touch *)=nullptr;
    bool pending=false;
    uint8_t report=0;
};
int64_t now_us=0;
int64_t esp_timer_get_time(){return now_us;}
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
#define BROOKESIA_LOGW(...) ((void)0)
'''

TESTS = r'''
esp_err_t gt911(Touch *tp){
    if(tp->pending){tp->data.points=tp->report;tp->pending=false;}
    return ESP_OK;
}
esp_err_t other_driver(Touch *tp){tp->data.points=tp->report;return ESP_OK;}
int main(){
    Touch a,b,other;a.read_data=gt911;b.read_data=gt911;other.read_data=other_driver;
    install_release_timeout(&a);install_release_timeout(&b);
    assert(a.read_data!=gt911 && b.read_data!=gt911 && other.read_data==other_driver);
    a.pending=true;a.report=1;b.pending=true;b.report=1;
    assert(a.read_data(&a)==ESP_OK && b.read_data(&b)==ESP_OK);
    assert(a.data.points==1 && b.data.points==1);
    now_us=50000;a.pending=true;a.report=1;
    assert(a.read_data(&a)==ESP_OK && a.data.points==1);
    now_us=110000;b.pending=true;b.report=0;
    assert(b.read_data(&b)==ESP_OK && b.data.points==0);
    assert(a.read_data(&a)==ESP_OK && a.data.points==1);
    now_us=151000;assert(a.read_data(&a)==ESP_OK && a.data.points==0);
    other.report=1;assert(other.read_data(&other)==ESP_OK && other.data.points==1);
    uninstall_release_timeout(&a);uninstall_release_timeout(&b);
    assert(a.read_data==gt911 && b.read_data==gt911 && other.read_data==other_driver);
}
'''


def main():
    source = (ROOT / 'hal/brookesia_hal_adaptor/src/display/lcd_touch_impl.cpp').read_text()
    start = source.index('#if defined(CONFIG_ESP_BOARD_ESP32_P4X_FUNCTION_EV)')
    end = source.index('\n#endif', start) + len('\n#endif')
    code = PREAMBLE + source[start:end] + TESTS
    with tempfile.TemporaryDirectory(prefix='touch-release-timeout-') as temp:
        test = Path(temp) / 'test.cpp'
        binary = Path(temp) / 'test'
        test.write_text(code)
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                        '-DCONFIG_ESP_BOARD_ESP32_P4X_FUNCTION_EV=1', str(test), '-o', str(binary), '-pthread'],
                       check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('PASS: P4X GT911 press, hold, release, missing report and two independent controllers')


if __name__ == '__main__':
    main()
