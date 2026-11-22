
cmake_minimum_required(VERSION 3.21)

project(DEADNET
    VERSION 0.1.0
    DESCRIPTION "Cross-platform network monitoring and mapping platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

include(cmake/ProjectOptions.cmake)
include(cmake/CompilerWarnings.cmake)

add_subdirectory(core)
