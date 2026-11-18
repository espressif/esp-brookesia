# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise production GSP update and cache logic with fault-injectable renderer metadata."""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


PREAMBLE = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <expected>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#define CONFIG_BROOKESIA_GUI_BACKEND_GSP 1
#define BROOKESIA_LOGI(...) ((void)0)
using esp_gsp_err_t=int;
using esp_gsp_handle_t=void *;
constexpr int ESP_GSP_OK=0, ESP_GSP_FAIL=-1, ESP_GSP_ERR_TIMEOUT=107, ESP_GSP_NO_TRANSITION=0;
enum gsp_component_prop_kind_t {GSP_COMPONENT_PROP_VALUE, GSP_COMPONENT_PROP_TEXT};
enum gsp_value_type_t {GSP_VALUE_BOOL, GSP_VALUE_I32, GSP_VALUE_U32, GSP_VALUE_COLOR};
struct gsp_property_info_t {gsp_value_type_t value_type; int64_t min_value, max_value;};
namespace runtime {using AppId=int;}
struct SystemHostBridge {
    struct GspUpdate {
        enum class Kind {SetText,SetValue,ShowPage,SetProperty};
        enum class ValueKind {Integer,Boolean,Text};
        Kind kind=Kind::SetText;
        std::string name,text,property;
        int32_t value=0;
        ValueKind value_kind=ValueKind::Integer;
    };
};
'''

FAKES = r'''
namespace gui::gsp {
struct BundleContract {
    bool has_scene(uint16_t scene) const {return scene < 3;}
    bool has_component_property(uint16_t, uint32_t component, gsp_component_prop_kind_t property) const {
        return component==gsp_name_key("label") && property==GSP_COMPONENT_PROP_TEXT;
    }
    std::expected<gsp_property_info_t,std::string> property_info(uint16_t, uint32_t component, uint32_t property) const {
        if(component!=gsp_name_key("label") || property!=gsp_name_key("angle"))return std::unexpected("not found");
        return gsp_property_info_t{GSP_VALUE_I32,0,359};
    }
};
}
struct Host {
    std::shared_ptr<const gui::gsp::BundleContract> metadata=std::make_shared<gui::gsp::BundleContract>();
    bool running() const{return true;}
    void *handle(){return this;}
    std::shared_ptr<const gui::gsp::BundleContract> bundle() const{return metadata;}
};
struct Record {
    bool gsp_suspended=false,gsp_scene_known=true;
    uint16_t gsp_current_scene=1;
    std::vector<SystemHostBridge::GspUpdate> gsp_scene_updates;
    std::shared_ptr<const gui::gsp::BundleContract> gsp_suspended_contract=std::make_shared<gui::gsp::BundleContract>();
    std::unique_ptr<Host> gsp_host=std::make_unique<Host>();
};
std::atomic<int64_t> gsp_page_requested_us{0};
int64_t steady_us(){return 1;}
bool reject_scene=false;
int esp_gsp_goto_scene(void *,uint16_t,int){return reject_scene?ESP_GSP_FAIL:ESP_GSP_OK;}
bool reject_update=false;
std::expected<void,std::string> submit_gsp_component_update(void *,const SystemHostBridge::GspUpdate &){
    return reject_update?std::expected<void,std::string>(std::unexpected("injected update failure")):
           std::expected<void,std::string>{};
}
constexpr auto SYSTEM_GUI_TASK_GROUP="gui";
struct System {struct Impl {
    struct Scheduler {bool is_running() const{return false;}};
    std::shared_ptr<Scheduler> task_scheduler_;
    std::map<int,int> runtime_to_app_{{7,42}};
    Record record;
    std::optional<int> runtime_owner(int id) const {
        auto it=runtime_to_app_.find(id);return it==runtime_to_app_.end()?std::nullopt:std::optional<int>(it->second);
    }
    bool is_current_task_domain(const char *) const {return true;}
    template<class R,class F> R run_task_sync(const char *,F &&fn,R){return fn();}
    std::expected<Record *,std::string> get_record(int id){
        return id==42?std::expected<Record *,std::string>(&record):std::unexpected("not found");
    }
    std::expected<void,std::string> apply_runtime_gsp_update(int,const SystemHostBridge::GspUpdate &);
};};
'''

TESTS = r'''
int main(){
    using Update=SystemHostBridge::GspUpdate;
    int attempts=0;
    auto started=std::chrono::steady_clock::now();
    assert(submit_gsp_update([&](){++attempts;return ESP_GSP_ERR_TIMEOUT;})==ESP_GSP_ERR_TIMEOUT);
    assert(attempts==2 && std::chrono::steady_clock::now()-started<std::chrono::milliseconds(50));
    System::Impl system;
    Update text; text.name="label"; text.text="before";
    assert(system.apply_runtime_gsp_update(7,text));
    assert(system.record.gsp_scene_updates.size()==1);
    reject_update=true;text.text="rejected";assert(!system.apply_runtime_gsp_update(7,text));
    assert(system.record.gsp_scene_updates[0].text=="before");reject_update=false;text.text="before";
    Update page;page.kind=Update::Kind::ShowPage;page.value=2;
    reject_scene=true;
    assert(!system.apply_runtime_gsp_update(7,page));
    assert(system.record.gsp_current_scene==1 && system.record.gsp_scene_updates[0].text=="before");
    reject_scene=false;
    page.value=99;assert(!system.apply_runtime_gsp_update(7,page));
    assert(system.record.gsp_current_scene==1 && system.record.gsp_scene_updates[0].text=="before");
    system.record.gsp_suspended=true;
    for(int i=0;i<20000;++i){
        Update invalid;invalid.name="missing"+std::to_string(i);
        assert(!system.apply_runtime_gsp_update(7,invalid));
    }
    assert(system.record.gsp_scene_updates.size()==1);
    text.text=std::string(65536,'x');assert(!system.apply_runtime_gsp_update(7,text));
    assert(system.record.gsp_scene_updates[0].text=="before");
    for(int i=0;i<10000;++i){text.text=std::to_string(i);assert(system.apply_runtime_gsp_update(7,text));}
    assert(system.record.gsp_scene_updates.size()==1 && system.record.gsp_scene_updates[0].text=="9999");
    Update prop;prop.kind=Update::Kind::SetProperty;prop.name="label";prop.property="angle";prop.value=400;
    assert(!system.apply_runtime_gsp_update(7,prop));
    prop.value=180;assert(system.apply_runtime_gsp_update(7,prop));
    assert(!system.apply_runtime_gsp_update(7,page));
    assert(system.record.gsp_current_scene==1 && system.record.gsp_scene_updates.size()==2);
    page.value=2;assert(system.apply_runtime_gsp_update(7,page));
    assert(system.record.gsp_current_scene==2 && system.record.gsp_scene_updates.empty());
    std::vector<Update> many;
    for(int i=0;i<256;++i){Update item;item.name=std::to_string(i);
        assert(check_gsp_scene_update_budget(many,item));assert(record_gsp_scene_update(many,item));}
    Update overflow;overflow.name="overflow";assert(!check_gsp_scene_update_budget(many,overflow));
    system.record.gsp_suspended=false;page.value=1;assert(system.apply_runtime_gsp_update(7,page));
    assert(system.record.gsp_current_scene==1);
    std::cout << "PASS: failed and suspended scene changes, metadata validation and bounded updates\n";
}
'''


def main():
    source = (ROOT / 'src/system/gui.cpp').read_text()
    code = PREAMBLE + function(source, 'uint32_t gsp_name_key(') + FAKES
    code += 'template <typename Submit>\n' + function(source, 'esp_gsp_err_t submit_gsp_update(')
    for signature in (
        'std::expected<void, std::string> check_gsp_scene_update_budget(',
        'std::expected<void, std::string> record_gsp_scene_update(',
        'std::expected<void, std::string> validate_suspended_gsp_update(',
        'std::expected<void, std::string> System::Impl::apply_runtime_gsp_update(',
    ):
        code += function(source, signature)
    with tempfile.TemporaryDirectory(prefix='gsp-runtime-updates-') as temp:
        test = Path(temp) / 'test.cpp'
        binary = Path(temp) / 'test'
        test.write_text(code + TESTS)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == '__main__':
    main()
