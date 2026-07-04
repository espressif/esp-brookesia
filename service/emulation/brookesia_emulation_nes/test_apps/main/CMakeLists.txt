idf_component_register(
    SRCS
        test_app_main.cpp
        test_emulation_nes.cpp
        mock_storage_device.cpp
    INCLUDE_DIRS "."
    REQUIRES
        unity
        brookesia_hal_interface
        brookesia_service_display
        brookesia_service_helper
        brookesia_service_manager
        brookesia_emulation_nes
        brookesia_lib_utils
)
