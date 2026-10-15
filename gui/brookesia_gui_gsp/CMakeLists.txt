if(NOT ESP_PLATFORM)
    if(NOT EXISTS "${ESP_GSP_ROOT}/tools/sim_bridge/CMakeLists.txt")
        message(FATAL_ERROR "Linux GSP requires ESP_GSP_ROOT pointing to the official esp-gsp 1.4 component")
    endif()
    if(NOT TARGET esp_gsp::sim_bridge)
        add_subdirectory("${ESP_GSP_ROOT}/tools/sim_bridge" "${CMAKE_CURRENT_BINARY_DIR}/sim_bridge")
    endif()
    find_package(Boost REQUIRED COMPONENTS json)
    find_package(ZLIB REQUIRED)
    find_package(Threads REQUIRED)
    add_library(brookesia_gui_gsp STATIC
        src/app_host.cpp src/bundle_desktop.cpp src/session_desktop.cpp
        src/desktop_display.cpp src/desktop_api.cpp)
    add_library(brookesia::gui_gsp ALIAS brookesia_gui_gsp)
    target_compile_features(brookesia_gui_gsp PUBLIC cxx_std_23)
    target_compile_definitions(brookesia_gui_gsp PUBLIC BROOKESIA_GUI_GSP_DESKTOP=1)
    target_include_directories(brookesia_gui_gsp PUBLIC include PRIVATE src)
    target_link_libraries(brookesia_gui_gsp PUBLIC
        esp_gsp::sim_bridge brookesia::lib_utils brookesia::hal_interface Boost::json ZLIB::ZLIB Threads::Threads)
    foreach(api IN ITEMS component_set_text component_set_value component_set_visible component_set_enabled
            component_set_property component_set_property_color_rgb888 goto_scene set_swipe_enabled flush on_event)
        target_link_options(brookesia_gui_gsp INTERFACE "LINKER:--wrap=esp_gsp_${api}")
    endforeach()
    return()
endif()

set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})

set(COMPONENT_SRCS_CPP
    ${COMPONENT_SRC_DIR}/session.cpp
    ${COMPONENT_SRC_DIR}/bundle.cpp
    ${COMPONENT_SRC_DIR}/app_host.cpp
)

idf_component_register(
    SRCS ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    PRIV_REQUIRES espressif__zlib
)
