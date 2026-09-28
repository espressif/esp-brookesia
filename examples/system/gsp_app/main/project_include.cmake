# Forget cached paths to components that left the build when the GUI backend changed.
get_cmake_property(brookesia_cached_variables CACHE_VARIABLES)
foreach(brookesia_cached_variable IN LISTS brookesia_cached_variables)
    get_property(brookesia_cached_value CACHE ${brookesia_cached_variable} PROPERTY VALUE)
    if(brookesia_cached_value MATCHES "^${PROJECT_DIR}/managed_components/"
            AND NOT EXISTS "${brookesia_cached_value}")
        unset(${brookesia_cached_variable} CACHE)
    elseif(brookesia_cached_value MATCHES "^idf::([A-Za-z0-9_.-]+__[A-Za-z0-9_.-]+)$"
            AND NOT EXISTS "${PROJECT_DIR}/managed_components/${CMAKE_MATCH_1}")
        unset(${brookesia_cached_variable} CACHE)
    endif()
endforeach()
unset(brookesia_cached_variables)
unset(brookesia_cached_variable)
unset(brookesia_cached_value)
