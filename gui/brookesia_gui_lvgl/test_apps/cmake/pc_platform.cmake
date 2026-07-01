set(
    BROOKESIA_TEST_BOOST_ROOT
    ""
    CACHE PATH "Optional Boost source root used when system Boost.Test is unavailable"
)

if(NOT TARGET brookesia::lib_utils)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../utils/brookesia_lib_utils
        ${CMAKE_BINARY_DIR}/brookesia_lib_utils
    )
endif()
if(NOT TARGET brookesia::gui_interface)
    add_subdirectory(
        ${TEST_APP_DIR}/../../brookesia_gui_interface
        ${CMAKE_BINARY_DIR}/brookesia_gui_interface
    )
endif()

message(STATUS "lvgl target is not provisioned by this test app; PC test will compile public headers and skip backend execution")

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_gui_lvgl ${TEST_APP_SRCS_CPP})

find_package(Boost COMPONENTS unit_test_framework QUIET)
if(Boost_unit_test_framework_FOUND)
    target_link_libraries(test_brookesia_gui_lvgl PRIVATE Boost::unit_test_framework)
    target_compile_definitions(test_brookesia_gui_lvgl PRIVATE BOOST_TEST_DYN_LINK)
else()
    if("${BROOKESIA_TEST_BOOST_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Boost.Test not found. Set BROOKESIA_TEST_BOOST_ROOT to an esp-boost/src path.")
    endif()
    target_include_directories(test_brookesia_gui_lvgl PRIVATE ${BROOKESIA_TEST_BOOST_ROOT})
    target_compile_definitions(test_brookesia_gui_lvgl PRIVATE
        BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST=1
    )
endif()

target_compile_features(test_brookesia_gui_lvgl PRIVATE cxx_std_23)
target_include_directories(
    test_brookesia_gui_lvgl
    PRIVATE
        ${TEST_APP_MAIN_DIR}
        ${TEST_APP_DIR}/../include
)
target_link_libraries(test_brookesia_gui_lvgl PRIVATE brookesia::gui_interface)
target_compile_definitions(test_brookesia_gui_lvgl PRIVATE BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND=0)

enable_testing()
add_test(NAME test_brookesia_gui_lvgl COMMAND test_brookesia_gui_lvgl)
