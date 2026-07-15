#
# ESP Platform
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_link_libraries(${COMPONENT_LIB} PUBLIC "-u app_store_provider_symbol")

if(NOT DEFINED CONFIG_BROOKESIA_APP_STORE_SERVER_ROOT_URL)
    set(CONFIG_BROOKESIA_APP_STORE_SERVER_ROOT_URL "https://brookesia-app-store.espressif.com/api/v1")
endif()
if(NOT DEFINED CONFIG_BROOKESIA_APP_STORE_INDEX_URL)
    set(CONFIG_BROOKESIA_APP_STORE_INDEX_URL "")
endif()
set(app_store_server_root_url "${CONFIG_BROOKESIA_APP_STORE_SERVER_ROOT_URL}")
set(app_store_index_url "${CONFIG_BROOKESIA_APP_STORE_INDEX_URL}")
get_property(
    app_store_server_root_url_cache_set
    CACHE CONFIG_BROOKESIA_APP_STORE_SERVER_ROOT_URL
    PROPERTY VALUE
    SET
)
if(app_store_server_root_url_cache_set)
    get_property(
        app_store_server_root_url_cache
        CACHE CONFIG_BROOKESIA_APP_STORE_SERVER_ROOT_URL
        PROPERTY VALUE
    )
    if(NOT app_store_server_root_url_cache STREQUAL "")
        set(app_store_server_root_url "${app_store_server_root_url_cache}")
    endif()
endif()
get_property(
    app_store_index_url_cache_set
    CACHE CONFIG_BROOKESIA_APP_STORE_INDEX_URL
    PROPERTY VALUE
    SET
)
if(app_store_index_url_cache_set)
    get_property(
        app_store_index_url_cache
        CACHE CONFIG_BROOKESIA_APP_STORE_INDEX_URL
        PROPERTY VALUE
    )
    if(NOT app_store_index_url_cache STREQUAL "")
        set(app_store_index_url "${app_store_index_url_cache}")
    endif()
endif()
string(REGEX REPLACE "^\"(.*)\"$" "\\1" app_store_server_root_url "${app_store_server_root_url}")
string(REGEX REPLACE "^\"(.*)\"$" "\\1" app_store_index_url "${app_store_index_url}")
target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
        BROOKESIA_APP_STORE_SERVER_ROOT_URL="${app_store_server_root_url}"
        BROOKESIA_APP_STORE_INDEX_URL="${app_store_index_url}"
)

set(app_store_package_id "brookesia.general.app_store")
if(DEFINED BROOKESIA_SYSTEM_CORE_COMPONENT_DIR AND NOT "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}" STREQUAL "")
    set(brookesia_system_core_dir "${BROOKESIA_SYSTEM_CORE_COMPONENT_DIR}")
else()
    idf_build_get_property(build_components BUILD_COMPONENTS)
    if("espressif__brookesia_system_core" IN_LIST build_components)
        idf_component_get_property(brookesia_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
    else()
        idf_component_get_property(brookesia_system_core_dir brookesia_system_core COMPONENT_DIR)
    endif()
endif()
include("${brookesia_system_core_dir}/cmake/runtime_app_stage.cmake")
brookesia_system_core_get_esp_runtime_app_stage_root(app_store_stage_root)
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_store_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_store_stage_root}"
    NO_INDEX
)

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
