add_library(deadnet_core STATIC
    src/common/version.cpp
    src/network/network_quality.cpp
    src/network/speed_test.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(deadnet_core PRIVATE
        src/network/linux_network_probe.cpp
    )
endif()

find_package(CURL REQUIRED)

target_link_libraries(deadnet_core
    PRIVATE
        CURL::libcurl
)

target_include_directories(deadnet_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

deadnet_configure_project(deadnet_core)
deadnet_enable_warnings(deadnet_core)

if(BUILD_TESTING)
    add_executable(deadnet_version_test
        tests/common/version_test.cpp
    )

    target_link_libraries(deadnet_version_test
        PRIVATE deadnet_core
    )

    deadnet_configure_project(deadnet_version_test)
    deadnet_enable_warnings(deadnet_version_test)

    add_test(
        NAME deadnet.version
        COMMAND deadnet_version_test
    )
endif()