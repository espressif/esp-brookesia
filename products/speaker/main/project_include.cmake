# Check if python patch package is available
execute_process(
    COMMAND python -c "import patch; print('patch package is available')"
    RESULT_VARIABLE PATCH_CHECK_RESULT
    OUTPUT_QUIET
    ERROR_QUIET
)
if(NOT PATCH_CHECK_RESULT EQUAL 0)
    message(WARNING "Python patch package not found. Installing via pip...")
    execute_process(
        COMMAND python -m pip install patch
        RESULT_VARIABLE PATCH_INSTALL_RESULT
    )
    if(NOT PATCH_INSTALL_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install python patch package")
    endif()
endif()

set(PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Project directory: ${PROJECT_DIR}")

set(PATCH_SCRIPT "${PROJECT_DIR}/tools/apply_patch.py")
if(EXISTS ${PATCH_SCRIPT})
    message(STATUS "Patch script: ${PATCH_SCRIPT}")
else()
    message(FATAL_ERROR "Patch script not found: ${PATCH_SCRIPT}")
endif()

set(PATCH_PATH "${PROJECT_DIR}/tools/patches")
if(EXISTS ${PATCH_PATH})
    message(STATUS "Patch path: ${PATCH_PATH}")
else()
    message(FATAL_ERROR "Patch path not found: ${PATCH_PATH}")
endif()

function(apply_patch_for_component COMPONENT_NAME PATCH_NAME)
    set(COMPONENT_DIR "${PROJECT_DIR}/managed_components/${COMPONENT_NAME}")
    message(STATUS "Target component directory: ${COMPONENT_DIR}")

    if(EXISTS ${COMPONENT_DIR})
        execute_process(
            COMMAND python "${PATCH_SCRIPT}" --patch "${PATCH_PATH}/${PATCH_NAME}" --target "${COMPONENT_DIR}"
            RESULT_VARIABLE RESULT
            ERROR_VARIABLE ERROR
        )

        if(NOT RESULT EQUAL 0)
            message(WARNING "Patch application failed: ${ERROR} ${RESULT}")
        endif()
    else()
        message(FATAL_ERROR "Component directory not found: ${COMPONENT_DIR}")
    endif()
endfunction()

apply_patch_for_component("espressif__esp_websocket_client" "esp_websocket_client.patch")
apply_patch_for_component("espressif__gmf_core" "gmf_core.patch")
apply_patch_for_component("espressif__esp_lvgl_port" "esp_lvgl_port.patch")
