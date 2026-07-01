#
# ESP Platform
#
idf_component_register(
    SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
    PRIV_REQUIRES freertos heap pthread
)

# Wrap explicit pthread mutex init/destroy so Brookesia can allocate mutex semaphores with caps that avoid
# unnecessary Internal SRAM pressure on ESP platforms.
target_link_options(${COMPONENT_LIB}
    INTERFACE
        "-Wl,--wrap=pthread_mutex_init"
        "-Wl,--wrap=pthread_mutex_destroy"
)

include(package_manager)
cu_pkg_define_version(${COMPONENT_DIR})
