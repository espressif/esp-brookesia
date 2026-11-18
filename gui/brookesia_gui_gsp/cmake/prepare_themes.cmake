# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

# Color is baked into text-bound GSP controls. Resolve both palettes at configure
# time; gsp_add_bundle compiles them at build time with identical object layouts.
function(brookesia_gsp_prepare_theme output theme)
    set(scenes ${ARGN})
    set(generator "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/prepare_themes.py")
    set(destination "${CMAKE_CURRENT_BINARY_DIR}/gsp_themes/${theme}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${scenes} "${generator}")
    execute_process(COMMAND "${PYTHON}" "${generator}"
        --theme "${theme}" --output "${destination}" ${scenes}
        COMMAND_ERROR_IS_FATAL ANY)
    set(prepared "")
    foreach(scene IN LISTS scenes)
        get_filename_component(filename "${scene}" NAME)
        list(APPEND prepared "${destination}/${filename}")
    endforeach()
    set(${output} "${prepared}" PARENT_SCOPE)
endfunction()
