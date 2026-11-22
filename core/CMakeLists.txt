add_library(deadnet_core STATIC
    src/common/vercion.cpp
)

target_include_directories(deadnet_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

deadnet_configure_project(deadnet_core)
dednet_enable_warnings(deadnet_core)
