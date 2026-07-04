#
# PC Platform
#
set(COMPONENT_LIB brookesia_gui_interface_impl)
set(EXAMPLES_ASSETS_DIR_PC "${COMPONENT_EXAMPLES_DIR}/assets_bin")
set(component_pc_config_compile_definitions EXAMPLES_ASSETS_DIR="${EXAMPLES_ASSETS_DIR_PC}")

if(NOT EMSCRIPTEN)
    find_package(Boost REQUIRED COMPONENTS json)
else()
    list(APPEND COMPONENT_SRCS_CPP "${COMPONENT_DIR}/wasm_shims/gui_task_queue_wasm.cpp")
endif()

option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG on PC"
    OFF
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_DATA_STORE_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_PARSER_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_RUNTIME_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_VALIDATOR_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_WIDGET_ENABLE_DEBUG_LOG
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG on PC"
    ON
)
option(
    BROOKESIA_GUI_INTERFACE_PC_CONFIG_ENABLE_MEMORY_TRACE
    "Default value of CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE on PC (heap trace profiling)"
    OFF
)

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_DATA_STORE_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_PARSER_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_PARSER_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_RUNTIME_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_VALIDATOR_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_WIDGET_ENABLE_DEBUG_LOG)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_WIDGET_ENABLE_DEBUG_LOG=0)
endif()

if(BROOKESIA_GUI_INTERFACE_PC_CONFIG_ENABLE_MEMORY_TRACE)
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE=1)
else()
    list(APPEND component_pc_config_compile_definitions CONFIG_BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE=0)
endif()

add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB}
    PUBLIC
        $<$<BOOL:${EMSCRIPTEN}>:${COMPONENT_DIR}/wasm_shims>
        ${COMPONENT_INCLUDE_DIRS}
    PRIVATE
        ${COMPONENT_PRIVATE_INCLUDE_DIRS}
)
if(Boost_INCLUDE_DIRS)
    target_include_directories(${COMPONENT_LIB} PUBLIC ${Boost_INCLUDE_DIRS})
endif()
target_link_libraries(${COMPONENT_LIB}
    PUBLIC
        brookesia::lib_utils
)
if(NOT EMSCRIPTEN)
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            Boost::json
    )
endif()

target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        ${component_pc_config_compile_definitions}
)

if(DEFINED BROOKESIA_GUI_INTERFACE_PC_COMPILE_DEFINITIONS AND
    NOT "${BROOKESIA_GUI_INTERFACE_PC_COMPILE_DEFINITIONS}" STREQUAL "")
    target_compile_definitions(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_GUI_INTERFACE_PC_COMPILE_DEFINITIONS}
    )
endif()

if(DEFINED BROOKESIA_GUI_INTERFACE_PC_COMPILE_OPTIONS AND
    NOT "${BROOKESIA_GUI_INTERFACE_PC_COMPILE_OPTIONS}" STREQUAL "")
    target_compile_options(${COMPONENT_LIB}
        PRIVATE
            ${BROOKESIA_GUI_INTERFACE_PC_COMPILE_OPTIONS}
    )
endif()

add_library(brookesia::gui_interface ALIAS ${COMPONENT_LIB})

#
# Example suite (separate static library, gated per-example).
#
if(GUI_INTERFACE_EXAMPLES_CPP)
    set(EXAMPLES_LIB brookesia_gui_interface_examples)

    # Per-example master + individual switches. Each ON example defines
    # CONFIG_BROOKESIA_GUI_INTERFACE_EXAMPLE_<ID>=1 picked up by example_macros.h.
    set(BROOKESIA_GUI_INTERFACE_EXAMPLE_IDS
        DOCUMENT_ROOT DOCUMENT_VARIANTS DOCUMENT_ENVIRONMENT DOCUMENT_CONSTANTS
        ASSETS_CONSTANT ASSETS_STYLESET ASSETS_THEME ASSETS_TEMPLATE
        VIEW_SCREEN VIEW_CONTAINER VIEW_LABEL VIEW_BUTTON VIEW_IMAGE VIEW_TEXT_INPUT
        VIEW_SLIDER VIEW_SWITCH VIEW_CHECKBOX VIEW_DROPDOWN VIEW_PROGRESS_BAR VIEW_SPINNER
        VIEW_ARC VIEW_LINE VIEW_TABLE VIEW_KEYBOARD VIEW_CANVAS VIEW_FRAME_VIEW
        STYLING_FLEX STYLING_GRID STYLING_PLACEMENT STYLING_STYLE STYLING_STATE_STYLES STYLING_STYLE_REFS
        INTERACTION_BINDINGS INTERACTION_EVENTS INTERACTION_ANIMATIONS INTERACTION_SCREEN_FLOW
        RUNTIME_TEMPLATE RUNTIME_THEME_LANGUAGE RUNTIME_ANIMATION_API RUNTIME_MOUNT_STACK
    )

    option(BROOKESIA_GUI_INTERFACE_PC_ENABLE_EXAMPLES "Compile the JSON-UI example suite on PC" ON)

    set(examples_pc_compile_definitions
        EXAMPLES_ASSETS_DIR="${EXAMPLES_ASSETS_DIR_PC}"
        ${component_pc_config_compile_definitions}
    )
    foreach(example_id ${BROOKESIA_GUI_INTERFACE_EXAMPLE_IDS})
        option(BROOKESIA_GUI_INTERFACE_PC_EXAMPLE_${example_id}
            "Compile example ${example_id} on PC" ON)
        if(BROOKESIA_GUI_INTERFACE_PC_ENABLE_EXAMPLES AND BROOKESIA_GUI_INTERFACE_PC_EXAMPLE_${example_id})
            list(APPEND examples_pc_compile_definitions
                CONFIG_BROOKESIA_GUI_INTERFACE_EXAMPLE_${example_id}=1)
        else()
            list(APPEND examples_pc_compile_definitions
                CONFIG_BROOKESIA_GUI_INTERFACE_EXAMPLE_${example_id}=0)
        endif()
    endforeach()

    add_library(${EXAMPLES_LIB} STATIC ${GUI_INTERFACE_EXAMPLES_CPP})
    target_compile_features(${EXAMPLES_LIB} PUBLIC cxx_std_23)
    target_include_directories(${EXAMPLES_LIB}
        PUBLIC
            ${COMPONENT_INCLUDE_DIRS}
        PRIVATE
            ${COMPONENT_EXAMPLES_DIR}
    )
    if(Boost_INCLUDE_DIRS)
        target_include_directories(${EXAMPLES_LIB} PUBLIC ${Boost_INCLUDE_DIRS})
    endif()
    target_link_libraries(${EXAMPLES_LIB}
        PUBLIC
            brookesia::gui_interface
            brookesia::lib_utils
    )
    if(NOT EMSCRIPTEN)
        target_link_libraries(${EXAMPLES_LIB} PUBLIC Boost::json)
    endif()
    target_compile_definitions(${EXAMPLES_LIB}
        PRIVATE
            ${examples_pc_compile_definitions}
    )
    add_library(brookesia::gui_interface_examples ALIAS ${EXAMPLES_LIB})
endif()
