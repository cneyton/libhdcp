cmake_minimum_required (VERSION 3.8)
project (libhdcp)

# Used for autocompletion in vim
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow static library to be included in another lib (used for python wrapping)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(hdcp INTERFACE)

add_subdirectory(src)
add_subdirectory(dependencies)

target_include_directories(hdcp
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

target_link_libraries(hdcp INTERFACE common)

target_compile_options(hdcp INTERFACE -Werror -Wall -Wextra)

# Tests
include(CTest)
if(HDCP_RUNTESTS)
    add_subdirectory(test)
endif()
