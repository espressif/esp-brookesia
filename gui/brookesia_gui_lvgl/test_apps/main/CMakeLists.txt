set(priv_requires unity brookesia_lib_utils brookesia_gui_lvgl)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../components/gen_bmgr_codes/CMakeLists.txt")
    list(APPEND priv_requires gen_bmgr_codes)
endif()

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES ${priv_requires}
    WHOLE_ARCHIVE TRUE
)
