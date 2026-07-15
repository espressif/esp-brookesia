set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_QRCODEGEN_DIR ${COMPONENT_DIR}/../../simulator/components/lvgl/src/libs/qrcode)
set(COMPONENT_REQUIRES brookesia_gui_lvgl)
if(ESP_PLATFORM)
    list(APPEND COMPONENT_REQUIRES
        brookesia_system_core
        esp_matter
        esp_matter_controller
        app_controller
        rmaker_matter_controller
        rmaker_app_network
        esp_rainmaker
        espressif__esp_secure_cert_mgr
        espressif__network_provisioning
        protocomm
        lvgl__lvgl
    )

    idf_component_get_property(COMPONENT_QRCODE_DIR espressif__qrcode COMPONENT_DIR)
    list(APPEND COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_QRCODE_DIR})
    idf_component_get_property(_app_controller_dir app_controller COMPONENT_DIR)
    if(_app_controller_dir)
        list(APPEND COMPONENT_PRIVATE_INCLUDE_DIRS "${_app_controller_dir}/priv_include")
    endif()
    
    idf_component_get_property(_esp_matter_dir esp_matter COMPONENT_DIR)
    if(_esp_matter_dir)
        list(APPEND COMPONENT_PRIVATE_INCLUDE_DIRS "${_esp_matter_dir}/data_model")
    endif()
endif()
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")

file(GLOB_RECURSE APP_MATTER_CONTROLLER_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_CPP ${APP_MATTER_CONTROLLER_SRCS_CPP})
if(EXISTS ${COMPONENT_QRCODEGEN_DIR}/qrcodegen.h)
    list(APPEND COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_QRCODEGEN_DIR})
endif()
if(EXISTS ${COMPONENT_QRCODEGEN_DIR}/qrcodegen.c)
    list(APPEND COMPONENT_SRCS_C ${COMPONENT_QRCODEGEN_DIR}/qrcodegen.c)
endif()

if(ESP_PLATFORM)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/esp_platform.cmake)
else()
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/pc_platform.cmake)
endif()

if(NOT CMAKE_SCRIPT_MODE_FILE)
    set_source_files_properties(${COMPONENT_SRCS_C}
        PROPERTIES
            COMPILE_FLAGS "${COMPONENT_SRCS_C_COMPILE_FLAGS}"
    )
    set_source_files_properties(${COMPONENT_SRCS_CPP}
        PROPERTIES
            COMPILE_FLAGS "${COMPONENT_SRCS_CPP_COMPILE_FLAGS}"
    )
endif()
