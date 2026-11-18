# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise the production SNTP callback with a non-draining fake subscription."""

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]


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
#include <cassert>
#include <expected>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#define BROOKESIA_LOGW(...) ((void)0)
struct AppContext {};
namespace service {
using EventItem=std::variant<std::string>;
using EventItemMap=std::map<std::string,EventItem>;
struct EventRegistry {using SignalSlot=std::function<void(const std::string &,const EventItemMap &)>;};
}
std::promise<void> *entered=nullptr;
std::shared_future<void> finish;
const service::EventItem *find_event_item(const service::EventItemMap &items,const std::string &key){
    if(entered){entered->set_value();finish.wait();}
    auto it=items.find(key);return it==items.end()?nullptr:&it->second;
}
struct Connection {bool connected() const{return true;}};
struct SNTPHelper {
    enum class EventId {StateChanged};
    static inline service::EventRegistry::SignalSlot callback;
    static Connection subscribe_event(EventId,service::EventRegistry::SignalSlot slot){
        callback=std::move(slot);return {};
    }
};
struct SettingsApp;
struct WifiUiMailbox {
    std::mutex mutex;
    bool open=true;
    std::vector<std::function<void(SettingsApp &)>> pending;
    void enqueue(std::function<void(SettingsApp &)> work){
        std::lock_guard lock(mutex);if(open)pending.push_back(std::move(work));
    }
    void close(){std::lock_guard lock(mutex);open=false;pending.clear();}
};
struct SettingsApp {
    struct Impl {
        std::shared_ptr<WifiUiMailbox> wifi_ui_mailbox=std::make_shared<WifiUiMailbox>();
        struct State {bool service_ready=true;std::string state;} time_zone_ui_state;
        AppContext *context=nullptr;
        std::vector<Connection> sntp_event_connections;
    };
    std::unique_ptr<Impl> impl_=std::make_unique<Impl>();
    int refresh_count=0;
    ~SettingsApp(){impl_->wifi_ui_mailbox->close();disconnect_sntp_events();}
    std::expected<void,std::string> refresh_time_zone_state(AppContext &){++refresh_count;return {};}
    void subscribe_sntp_events();
    void disconnect_sntp_events();
};
#define time_zone_ui_state_ impl_->time_zone_ui_state
#define context_ impl_->context
#define wifi_ui_mailbox_ impl_->wifi_ui_mailbox
#define sntp_event_connections_ impl_->sntp_event_connections
'''

TESTS = r'''
int main(){
    AppContext context;
    auto app=std::make_unique<SettingsApp>();
    app->impl_->context=&context;
    app->subscribe_sntp_events();
    auto callback=SNTPHelper::callback;
    service::EventItemMap event{{"State",std::string("Synced")}};
    callback("StateChanged",event);
    assert(app->impl_->wifi_ui_mailbox->pending.size()==1);
    app->impl_->wifi_ui_mailbox->pending.front()(*app);
    assert(app->impl_->time_zone_ui_state.state=="Synced" && app->refresh_count==1);
    std::weak_ptr<WifiUiMailbox> weak=app->impl_->wifi_ui_mailbox;
    std::promise<void> started,proceed;
    entered=&started;finish=proceed.get_future().share();
    std::thread emitter([&](){callback("StateChanged",event);});
    started.get_future().wait();
    app.reset();
    assert(weak.expired());
    proceed.set_value();emitter.join();
    entered=nullptr;
    callback("StateChanged",event);
}
'''


def main():
    source = (ROOT / 'src/service/services.ipp').read_text()
    code = PREAMBLE + function(source, 'void SettingsApp::subscribe_sntp_events()')
    code += function(source, 'void SettingsApp::disconnect_sntp_events()') + TESTS
    with tempfile.TemporaryDirectory(prefix='settings-sntp-lifetime-') as temp:
        test = Path(temp) / 'test.cpp'
        binary = Path(temp) / 'test'
        test.write_text(code)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        '-pthread', str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=10)
    print('PASS: SNTP updates use the app mailbox; in-flight and stale callbacks survive app destruction')


if __name__ == '__main__':
    main()
