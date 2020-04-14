cmake_minimum_required (VERSION 3.8)
project (libhdcp)

# Used for autocompletion in vim
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow static library to be included in another lib (used for python wrapping)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(hdcp STATIC)

add_subdirectory(src)
add_subdirectory(dependencies)

target_include_directories(hdcp
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src      # to remove
    )

target_link_libraries(hdcp PUBLIC common usb-1.0)

target_compile_options(hdcp INTERFACE -Werror -Wall -Wextra)

# Tests
include(CTest)
if(HDCP_RUNTESTS)
    add_subdirectory(test)
endif()
