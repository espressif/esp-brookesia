# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Compile production Settings logic with fake services; no GUI, firmware or hardware test.

The harness extracts complete rendering/forget/query functions and the leading saved
selection, hardware query and SNTP timer branches. Unrelated UI/service paths are
excluded explicitly, so a C++ host compiler suffices without ESP-IDF or GSP.
"""

import json
import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def function(path, name):
    source = (ROOT / path).read_text()
    start = source.index('std::expected<void, std::string> SettingsApp::' + name)
    return source[start:source.index('\n}\n', start) + 3]


def run():
    wifi = 'src/service/wifi_page.ipp'
    render = function(wifi, 'render_wifi_page')
    forget = function(wifi, 'forget_wifi_page_network')
    select = function(wifi, 'select_wifi_page_network')
    select = select[:select.index('    if (wifi_ui_state_.general_state')] + '    return {};\n}\n'
    device = function('src/service/device.ipp', 'refresh_my_device_state')
    device = device[:device.index('    const auto &device_name = cached_device_name_;')]
    device += '    shown_groups = std::move(groups);\n    return {};\n}\n'
    timer = function('src/app/lifecycle.ipp', 'on_timer')
    timer = timer[:timer.index('    if (name == SOUND_UI_TIMER_NAME)')]
    timer += '    return {};\n}\n'
    query = function('src/service/services.ipp', 'refresh_time_zone_state')
    view = (ROOT / 'src/private/settings_surface.hpp').read_text()
    view = view[view.index('struct WifiPageView {'):view.index('struct WifiConnectView {')]
    cpp = PRELUDE + view + HARNESS + render + forget + select + device + timer + query + TESTS
    with tempfile.TemporaryDirectory(prefix='settings-daily-state-') as temp:
        path = Path(temp)
        (path / 'test.cpp').write_text(cpp)
        subprocess.run([os.environ.get('CXX', 'g++-13'), '-std=c++23', '-Wall', '-Wextra', '-Werror',
                        str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
    scene = json.loads((ROOT / 'gsp/settings_wifi_1024.json').read_text())
    binds = {obj.get('bind') for obj in scene['objects']}
    assert {'saved_networks', 'nearby_networks', 'connection_notice'} <= binds
    for locale in ('en', 'zh_CN'):
        strings = json.loads((ROOT / f'package/res/i18n/{locale}.json').read_text())['strings']
        assert strings['wifi.forget_target'] and strings['wifi.select_saved_to_forget']
    lifecycle = (ROOT / 'src/app/lifecycle.ipp').read_text()
    assert lifecycle.count('context.timer().stop(sntp_ui_timer_id_)') == 2
    surface = (ROOT / 'src/ui/gsp_surface.cpp').read_text()
    resume = surface[surface.index('SettingsApp::on_resume'):]
    assert 'return refresh_time_zone_state(context);' in resume
    print('PASS: scene bindings, translations, stop/uninstall cleanup and resume query')


PRELUDE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace system_core { struct Info {std::string name="Brookesia", version="0.9";};
struct Service { Info get_system_info() {return {};} };
struct AppContext {Service service; Service &system_service(){return service;}};
using TimerId = unsigned; constexpr TimerId INVALID_TIMER_ID=0; }
#define system system_stub
namespace system_stub { namespace core = system_core; }
#define BROOKESIA_LOGW(...) ((void)0)
#define BROOKESIA_LOG_TRACE_GUARD_WITH_THIS() ((void)0)
namespace service::helper {struct Timeout {explicit Timeout(unsigned){}};}
namespace boost::json {struct object {}; struct array {};}
constexpr size_t WIFI_PAGE_ROW_COUNT=5;
constexpr unsigned WIFI_SERVICE_TIMEOUT_MS=500, DEVICE_SERVICE_TIMEOUT_MS=500, SNTP_SERVICE_TIMEOUT_MS=500;
constexpr auto PAGE_HOME="home", PAGE_DEVICE="device", PAGE_WIFI="wifi", PAGE_TIME_ZONE="time_zone";
constexpr auto SNTP_UI_TIMER_NAME="settings_sntp_ui";
constexpr auto MY_DEVICE_FALLBACK_SYSTEM_NAME="Unknown", MY_DEVICE_FALLBACK_SYSTEM_VERSION="Unknown";
bool settings_uses_gsp_backend(){return true;}
std::string localized_text(std::string_view, std::string_view key){return std::string(key)+":";}
std::string make_localized_unavailable_text(std::string_view){return "Unavailable";}
std::string make_localized_unknown_text(std::string_view){return "Unknown";}
std::string classify_hardware_interface(std::string_view, std::string_view){return "Network";}
struct HardwareGroup {std::string key,title;std::vector<std::string> lines;};
HardwareGroup &find_or_create_hardware_group(std::vector<HardwareGroup> &groups,
    std::string_view key, std::string_view) {
    for(auto &g:groups) if(g.key==key) return g;
    groups.push_back({std::string(key),std::string(key),{}});return groups.back();
}
struct WifiHelper {
    enum class FunctionId {RemoveConnectedAp};
    static inline bool fail=false;
    static inline std::vector<std::string> removed;
    static std::expected<void,std::string> call_function_sync(FunctionId,const std::string &ssid,
        service::helper::Timeout){if(fail)return std::unexpected("remove failed");removed.push_back(ssid);return {};}
};
struct DeviceHelper {
    enum class FunctionId {GetBoardInfo, GetCapabilities, GetCameraDeviceInfos,
        GetNetworkConnectivityInfo, GetPowerBatteryInfo};
    struct BoardInfo {std::string name="P4",chip="ESP32-P4",manufacturer,version,description;
        bool is_valid(){return true;}};
    struct Interface {std::string type_name="Wifi",instance_name="wifi0";};
    struct Capability {std::string name="Wi-Fi";std::vector<Interface> interfaces{{}};};
    using Capabilities=std::vector<Capability>;
    struct Camera {unsigned id=0;std::string name="Camera",device_path;};
    using CameraDeviceInfos=std::vector<Camera>;
    struct Network {std::string instance_name="wifi0", state;};
    using NetworkConnectivityInfos=std::vector<Network>;
    struct PowerBatteryInfo {std::string name;};
    static inline std::array<unsigned,5> calls{};
    static inline std::string network_state="Disconnected";
    static inline bool network_fail=false;
    static bool is_running(){return true;}
    template<class T> static std::expected<T,std::string> call_function_sync(FunctionId id,
        service::helper::Timeout) {++calls[static_cast<size_t>(id)];
        if(id==FunctionId::GetNetworkConnectivityInfo && network_fail)return std::unexpected("network failed");
        return T{};}
};
template<class Payload,class T> bool from_json(const Payload&,T &value) {
    if constexpr(std::is_same_v<T,DeviceHelper::Capabilities>) value={{}};
    if constexpr(std::is_same_v<T,DeviceHelper::CameraDeviceInfos>) value={{}};
    if constexpr(std::is_same_v<T,DeviceHelper::NetworkConnectivityInfos>)
        value={{"wifi0",DeviceHelper::network_state}};
    return true;
}
#define BROOKESIA_DESCRIBE_FROM_JSON(payload,value) from_json(payload,value)
#define BROOKESIA_DESCRIBE_ENUM_TO_STR(value) (value)
struct SNTPHelper {
    enum class FunctionId {GetTimezone,GetState};
    static inline unsigned calls=0;
    static inline std::string state="WaitingNetwork";
    static inline bool fail=false;
    static bool is_running(){return true;}
    template<class T> static std::expected<T,std::string> call_function_sync(FunctionId id,
        service::helper::Timeout){++calls;if(fail)return std::unexpected("sntp failed");
        return id==FunctionId::GetTimezone?"CST-8":state;}
};
'''

HARNESS = r'''
struct Surface {
    WifiPageView wifi;
    bool paused=false;
    bool suspended(){return paused;}
    std::expected<void,std::string> set_wifi(system::core::AppContext&,const WifiPageView &view){wifi=view;return {};}
};
struct Binding {bool is_valid(){return true;}};
struct SettingsApp {
    struct WifiNetworkState {WifiNetworkState(std::string name):ssid(std::move(name)){}
        std::string ssid,band;bool enterprise=false,locked=true;int signal_level=0,rssi=0;};
    struct WifiUiState {bool enabled=true,service_ready=true,scanning=false,connecting=false;
        std::string general_state="Connected", connected_ssid="Current";} wifi_ui_state_;
    struct Page {std::optional<bool> toggle_target;bool selection=false,connect_pending=false;
        bool scan_received=true,saved_list=false;std::string forget_ssid,connect_ssid,error;} wifi_page_;
    struct TimeZoneUiState {bool service_ready=false;std::string timezone,state;} time_zone_ui_state_;
    std::vector<WifiNetworkState> available_wifi_networks_{{"Nearby"}},saved_wifi_networks_{{"Current"},{"Other"}};
    size_t available_wifi_page_=0,saved_wifi_page_=0;
    std::string current_page_=PAGE_WIFI,current_locale_="en",cached_device_name_;
    std::vector<HardwareGroup> cached_hardware_groups_,shown_groups;
    std::unique_ptr<Surface> surface_=std::make_unique<Surface>();
    Binding device_service_binding_,sntp_service_binding_;
    system::core::AppContext *context_=nullptr;
    system::core::TimerId sntp_ui_timer_id_=9;
    unsigned disconnects=0;
    std::string rendered_sntp;
    void bind_device_service(){}
    void clear_hardware_groups(system::core::AppContext&){}
    void bind_sntp_service(){}
    bool is_saved_wifi_ssid(std::string_view ssid)const{
        return std::any_of(saved_wifi_networks_.begin(),saved_wifi_networks_.end(),
            [ssid](const auto &n){return n.ssid==ssid;});}
    std::expected<void,std::string> disconnect_current_wifi(system::core::AppContext&){++disconnects;return {};}
    std::expected<void,std::string> refresh_time_zone_page_state(system::core::AppContext&){
        rendered_sntp=time_zone_ui_state_.state;return {};}
    std::expected<void,std::string> render_wifi_page(system::core::AppContext&);
    std::expected<void,std::string> forget_wifi_page_network(system::core::AppContext&);
    std::expected<void,std::string> select_wifi_page_network(system::core::AppContext&,size_t);
    std::expected<void,std::string> refresh_my_device_state(system::core::AppContext&);
    std::expected<void,std::string> on_timer(system::core::AppContext&,system::core::TimerId,std::string_view);
    std::expected<void,std::string> refresh_time_zone_state(system::core::AppContext&);
};
'''

TESTS = r'''
int main(){
    system::core::AppContext context;
    SettingsApp wifi;
    assert(wifi.render_wifi_page(context));assert(!wifi.surface_->wifi.forget_enabled);
    assert(wifi.forget_wifi_page_network(context));assert(WifiHelper::removed.empty());
    wifi.wifi_page_.saved_list=true;
    assert(wifi.render_wifi_page(context));assert(wifi.surface_->wifi.rows[1].ssid=="Other");
    assert(wifi.forget_wifi_page_network(context));assert(WifiHelper::removed.empty());
    assert(wifi.select_wifi_page_network(context,1));assert(wifi.wifi_page_.forget_ssid=="Other");
    assert(wifi.surface_->wifi.forget_enabled);assert(wifi.surface_->wifi.selection_notice.ends_with("Other"));
    assert(wifi.forget_wifi_page_network(context));assert(WifiHelper::removed.back()=="Other");
    assert(wifi.disconnects==0 && wifi.is_saved_wifi_ssid("Current"));
    assert(wifi.wifi_page_.forget_ssid.empty() && !wifi.surface_->wifi.forget_enabled);
    assert(wifi.select_wifi_page_network(context,0));
    WifiHelper::fail=true;assert(wifi.forget_wifi_page_network(context));
    assert(wifi.is_saved_wifi_ssid("Current") && wifi.wifi_page_.forget_ssid=="Current");
    WifiHelper::fail=false;assert(wifi.forget_wifi_page_network(context));assert(wifi.disconnects==1);
    SettingsApp pages;pages.wifi_ui_state_.general_state="Idle";
    pages.wifi_page_.saved_list=true;pages.saved_wifi_networks_.clear();
    for(unsigned i=0;i<7;++i)pages.saved_wifi_networks_.push_back({"SSID"+std::to_string(i)});
    pages.saved_wifi_page_=1;assert(pages.render_wifi_page(context));
    assert(pages.surface_->wifi.rows[0].ssid=="SSID5");
    assert(pages.select_wifi_page_network(context,1));assert(pages.wifi_page_.forget_ssid=="SSID6");
    assert(pages.forget_wifi_page_network(context));assert(WifiHelper::removed.back()=="SSID6");
    pages.wifi_page_.forget_ssid="Missing";assert(pages.render_wifi_page(context));
    const auto removed=WifiHelper::removed.size();assert(pages.forget_wifi_page_network(context));
    assert(WifiHelper::removed.size()==removed && !pages.surface_->wifi.forget_enabled);
    pages.wifi_page_.forget_ssid="SSID5";pages.wifi_ui_state_.connecting=true;
    assert(pages.forget_wifi_page_network(context));assert(WifiHelper::removed.size()==removed);
    std::cout<<"PASS: F4 explicit target, noncurrent/current removal, failure, stale target and pagination\n";

    SettingsApp device;device.current_page_=PAGE_HOME;
    assert(device.refresh_my_device_state(context));assert(DeviceHelper::calls[3]==0);
    device.current_page_=PAGE_DEVICE;
    assert(device.refresh_my_device_state(context));
    auto contains=[&device](std::string_view text){
        for(auto &g:device.shown_groups) {
            for(auto &line:g.lines) {if(line.find(text)!=std::string::npos)return true;}
        }
        return false;
    };
    assert(contains("Disconnected"));DeviceHelper::network_state="Connected";
    assert(device.refresh_my_device_state(context));assert(contains("Connected") && !contains("Disconnected"));
    assert(DeviceHelper::calls[0]==1 && DeviceHelper::calls[1]==1 && DeviceHelper::calls[2]==1);
    assert(DeviceHelper::calls[3]==2);
    DeviceHelper::network_fail=true;assert(device.refresh_my_device_state(context));
    assert(contains("Unavailable") && !contains("Connected"));
    DeviceHelper::network_fail=false;DeviceHelper::network_state="Disconnected";
    assert(device.refresh_my_device_state(context));assert(contains("Disconnected"));
    for(auto &g:device.cached_hardware_groups_)for(auto &line:g.lines)
        assert(line.find("Connected")==std::string::npos && line.find("Disconnected")==std::string::npos);
    std::cout<<"PASS: F5 dynamic network transitions/failure recovery; static camera/capability queries once\n";

    SettingsApp time;time.context_=&context;time.current_page_=PAGE_TIME_ZONE;
    assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(time.rendered_sntp=="WaitingNetwork");
    SNTPHelper::state="Syncing";assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));
    assert(time.rendered_sntp=="Syncing");SNTPHelper::state="Synced";
    assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(time.rendered_sntp=="Synced");
    assert(SNTPHelper::calls==6);
    time.surface_->paused=true;assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(SNTPHelper::calls==6);
    time.surface_->paused=false;time.current_page_=PAGE_HOME;
    assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(SNTPHelper::calls==6);
    time.current_page_=PAGE_TIME_ZONE;time.sntp_ui_timer_id_=10;
    assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(SNTPHelper::calls==6);
    time.sntp_ui_timer_id_=0;assert(time.on_timer(context,0,SNTP_UI_TIMER_NAME));assert(SNTPHelper::calls==6);
    time.sntp_ui_timer_id_=9;time.context_=nullptr;
    assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));assert(SNTPHelper::calls==6);
    time.context_=&context;SNTPHelper::fail=true;assert(time.on_timer(context,9,SNTP_UI_TIMER_NAME));
    assert(time.rendered_sntp.empty());
    std::cout<<"PASS: F6 WaitingNetwork/Syncing/Synced and no query for hidden/suspended/stopped/stale events\n";
}
'''


if __name__ == '__main__':
    run()
