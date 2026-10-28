set(BROOKESIA_SERVICE_BT_ENABLE_AUTO_REGISTER ON CACHE BOOL
    "Enable Bt service plugin registration in tests" FORCE)
set(BROOKESIA_SERVICE_BT_SPEAKER_ENABLE_AUTO_REGISTER ON CACHE BOOL
    "Enable BtSpeaker plugin registration in tests" FORCE)

if(NOT TARGET brookesia::hal_linux)
    add_subdirectory(
        ${TEST_APP_DIR}/../../../../hal/brookesia_hal_linux
        ${CMAKE_BINARY_DIR}/brookesia_hal_linux
    )
endif()

add_subdirectory(${TEST_APP_DIR}/.. ${CMAKE_BINARY_DIR}/brookesia_service_bt_speaker)

file(GLOB TEST_APP_SRCS_CPP ${TEST_APP_MAIN_DIR}/*.cpp)
add_executable(test_brookesia_service_bt_speaker ${TEST_APP_SRCS_CPP})

find_package(Boost REQUIRED COMPONENTS unit_test_framework)
target_link_libraries(
    test_brookesia_service_bt_speaker
    PRIVATE
        Boost::unit_test_framework
        brookesia::service_bt
        brookesia::service_bt_speaker
        brookesia::hal_linux
)
target_compile_definitions(test_brookesia_service_bt_speaker PRIVATE BOOST_TEST_DYN_LINK)
target_compile_features(test_brookesia_service_bt_speaker PRIVATE cxx_std_23)
enable_testing()
add_test(NAME test_brookesia_service_bt_speaker COMMAND test_brookesia_service_bt_speaker)
