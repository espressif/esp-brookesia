# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Stress production app-record lookups, snapshots and removal with shared ownership."""

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
#include <atomic>
#include <cassert>
#include <expected>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#define BROOKESIA_LOGE(...) ((void)0)
using AppId=int;
constexpr int INVALID_APP_ID=-1;
constexpr auto SYSTEM_GUI_TASK_GROUP="gui";
namespace runtime {using AppId=int;}
struct System {struct Impl {
    struct AppRecord {
        struct Info {AppId app_id=0;struct Manifest {std::string id;} manifest;} info;
        bool running=false;
    };
    struct Scheduler {bool is_running() const{return false;}};
    std::shared_ptr<Scheduler> task_scheduler_;
    mutable std::mutex app_records_mutex_;
    std::map<AppId,std::shared_ptr<AppRecord>> apps_;
    std::map<std::string,AppId> manifest_id_to_app_;
    std::map<runtime::AppId,AppId> runtime_to_app_;
    AppId active_app_id_=INVALID_APP_ID;
    bool is_current_task_domain(const char *) const{return false;}
    bool gsp_session_running(const AppRecord &record) const{return record.running;}
    template<class R,class F> R run_task_sync(const char *,F &&fn,R){return fn();}
    std::expected<std::shared_ptr<AppRecord>,std::string> get_record(AppId);
    std::expected<std::shared_ptr<const AppRecord>,std::string> get_record(AppId) const;
    std::vector<std::shared_ptr<AppRecord>> snapshot_app_records() const;
    bool has_manifest(std::string_view) const;
    std::optional<AppId> runtime_owner(runtime::AppId) const;
    bool discard_app_record(AppId);
};};
'''

TESTS = r'''
int main(){
    System::Impl impl;
    std::atomic_bool done=false;
    std::vector<std::thread> readers;
    for(int reader=0;reader<4;++reader){
        readers.emplace_back([&](){
            while(!done.load()){
                for(const auto &record:impl.snapshot_app_records()){
                    assert(record->info.app_id>0);
                    const auto copy=record->info.manifest.id;
                    (void)impl.has_manifest(copy);
                    (void)impl.get_record(record->info.app_id);
                    (void)impl.runtime_owner(record->info.app_id);
                }
            }
        });
    }
    for(int id=1;id<=5000;++id){
        auto record=std::make_shared<System::Impl::AppRecord>();
        record->info.app_id=id;record->info.manifest.id=std::to_string(id);
        {
            std::lock_guard lock(impl.app_records_mutex_);
            impl.apps_.emplace(id,record);
            impl.manifest_id_to_app_.emplace(record->info.manifest.id,id);
            impl.runtime_to_app_.emplace(id,id);
        }
        auto held=impl.get_record(id);assert(held);
        assert(impl.discard_app_record(id));
        assert(!impl.get_record(id));
        assert((*held)->info.app_id==id && (*held)->info.manifest.id==std::to_string(id));
        std::lock_guard lock(impl.app_records_mutex_);impl.runtime_to_app_.erase(id);
    }
    done=true;for(auto &reader:readers)reader.join();
    std::cout << "PASS: concurrent record lookup/removal and held-record lifetime\n";
}
'''


def main():
    manager = (ROOT / 'src/app/manager.cpp').read_text()
    gui = (ROOT / 'src/system/gui.cpp').read_text()
    code = PREAMBLE
    for signature in (
        'std::expected<std::shared_ptr<System::Impl::AppRecord>, std::string> System::Impl::get_record(',
        'std::expected<std::shared_ptr<const System::Impl::AppRecord>, std::string> System::Impl::get_record(',
        'std::vector<std::shared_ptr<System::Impl::AppRecord>> System::Impl::snapshot_app_records()',
        'bool System::Impl::has_manifest(', 'std::optional<AppId> System::Impl::runtime_owner(',
    ):
        code += function(manager, signature)
    code += function(gui, 'bool System::Impl::discard_app_record(') + TESTS
    with tempfile.TemporaryDirectory(prefix='system-app-records-') as temp:
        test = Path(temp) / 'test.cpp'
        binary = Path(temp) / 'test'
        test.write_text(code)
        options = os.environ.get('APP_RECORDS_SANITIZER', '').split()
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        '-pthread', *options, str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == '__main__':
    main()
