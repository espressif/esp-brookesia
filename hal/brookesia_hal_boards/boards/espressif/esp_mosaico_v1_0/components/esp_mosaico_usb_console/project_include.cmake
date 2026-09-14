if(CONFIG_IDF_TARGET_ESP32S31 AND CONFIG_BSP_USB_AUTO_DOWNLOAD)
    set(mosaico_esptool_wrapper
        "${CMAKE_CURRENT_LIST_DIR}/tools/esp_mosaico_esptool_wrapper.py"
    )

    if(DEFINED ENV{ESPTOOL_WRAPPER} AND NOT "$ENV{ESPTOOL_WRAPPER}" STREQUAL "")
        get_filename_component(configured_esptool_wrapper "$ENV{ESPTOOL_WRAPPER}" REALPATH)
        get_filename_component(mosaico_esptool_wrapper_real "${mosaico_esptool_wrapper}" REALPATH)
        if(NOT configured_esptool_wrapper STREQUAL mosaico_esptool_wrapper_real)
            message(WARNING
                "ESPTOOL_WRAPPER is already set; the ESP-Mosaico ESP32-S31 "
                "post-flash reset workaround will not be installed"
            )
        endif()
    else()
        set(ENV{ESPTOOL_WRAPPER} "${mosaico_esptool_wrapper}")

        # The IDF esptool component can initialize its command before board
        # components are processed. Update the property as well so the flash
        # targets generated later in this configure pass use the wrapper.
        idf_build_get_property(mosaico_python PYTHON)
        idf_build_get_property(mosaico_target IDF_TARGET)
        set(mosaico_esptool_command
            "${mosaico_python}"
            "${mosaico_esptool_wrapper}"
            "esptool"
            "--chip"
            "${mosaico_target}"
        )
        idf_component_set_property(
            esptool_py
            ESPTOOLPY_CMD
            "${mosaico_esptool_command}"
        )
    endif()
endif()
