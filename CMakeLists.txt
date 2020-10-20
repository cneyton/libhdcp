cmake_minimum_required (VERSION 3.15)
project (libhdcp VERSION 0.2.2 LANGUAGES CXX)

################################################################################
# options
################################################################################
# Used for autocompletion in vim
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow static library to be included in another lib (used for python wrapping)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

################################################################################
# dependencies
################################################################################
if (TARGET common)
else()
    add_subdirectory(dependencies/libcommon)
endif()

################################################################################
# build
################################################################################
add_library(hdcp STATIC)
target_sources(hdcp PRIVATE
    src/usb_async.cpp
    src/tcp_server.cpp
    src/tcp_client.cpp
    src/request.cpp
    src/packet.cpp
    src/slave.cpp
    src/master.cpp
    src/error_code.cpp
    )
target_include_directories(hdcp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
target_link_libraries(hdcp
    PUBLIC  common
    PRIVATE usb-1.0)
target_compile_options(hdcp PUBLIC -Wall -Wextra)

# configure
configure_file(config.h.in config.h)
target_include_directories(hdcp PUBLIC ${PROJECT_BINARY_DIR})

################################################################################
# Tests
################################################################################
include(CTest)
if(HDCP_RUNTESTS)
    add_subdirectory(test)
endif()
