set(PRIV_REQS unity esp_psram brookesia_hal_adaptor esp_board_manager lwip)

# `gen_bmgr_codes` is generated only for board-specific configs.
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../components/gen_bmgr_codes/CMakeLists.txt")
    list(APPEND PRIV_REQS gen_bmgr_codes)
endif()

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../src"
    PRIV_REQUIRES ${PRIV_REQS}
    WHOLE_ARCHIVE
)
