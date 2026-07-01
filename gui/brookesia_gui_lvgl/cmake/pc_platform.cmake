#
# PC Platform
#
set(COMPONENT_LIB brookesia_gui_lvgl_impl)
set(component_pc_config_compile_definitions "")

option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_BACKEND_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_EVENT_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_EVENT_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_FACTORY_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_FACTORY_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_LAYOUT_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_LAYOUT_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_PROPS_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_PROPS_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_LVGL_PC_CONFIG_STYLE_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_LVGL_STYLE_ENABLE_DEBUG_LOG on PC"
    ON
)

if(BROOKESIA_GUI_LVGL_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_BACKEND_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_BACKEND_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_BACKEND_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_EVENT_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_EVENT_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_EVENT_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_FACTORY_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_FACTORY_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_FACTORY_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_LAYOUT_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_LAYOUT_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_LAYOUT_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_PROPS_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_PROPS_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_PROPS_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_LVGL_PC_CONFIG_STYLE_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_STYLE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_LVGL_STYLE_ENABLE_DEBUG_LOG=0)
endif()

if(NOT TARGET brookesia::gui_interface)
    message(FATAL_ERROR "brookesia_gui_lvgl requires brookesia_gui_interface to be enabled")
endif()

if(NOT TARGET lvgl)
    message(FATAL_ERROR "brookesia_gui_lvgl requires an existing lvgl target")
endif()

if(NOT TARGET brookesia::service_display)
    add_subdirectory(
        ${COMPONENT_DIR}/../../service/media/brookesia_service_display
        ${CMAKE_BINARY_DIR}/brookesia_service_display
    )
endif()

if(EMSCRIPTEN)
    if(NOT TARGET brookesia::hal_wasm)
        add_subdirectory(
            ${COMPONENT_DIR}/../../hal/brookesia_hal_wasm
            ${CMAKE_BINARY_DIR}/brookesia_hal_wasm
        )
    endif()
elseif(NOT TARGET brookesia::hal_linux)
    add_subdirectory(
        ${COMPONENT_DIR}/../../hal/brookesia_hal_linux
        ${CMAKE_BINARY_DIR}/brookesia_hal_linux
    )
endif()

if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::gui_interface
        brookesia::lib_utils
        brookesia::service_display
        lvgl
)
if(EMSCRIPTEN)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::hal_wasm
    )
else()
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia::hal_linux
            Threads::Threads
    )
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_GUI_LVGL_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_GUI_LVGL_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_GUI_LVGL_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_GUI_LVGL_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_GUI_LVGL_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_GUI_LVGL_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::gui_lvgl ALIAS ${COMPONENT_LIB})
