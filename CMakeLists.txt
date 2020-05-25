cmake_minimum_required (VERSION 3.8)
project (libhdcp)

# Used for autocompletion in vim
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Allow static library to be included in another lib (used for python wrapping)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.70 REQUIRED COMPONENTS system thread)
find_path(BOOST_INCLUDE_DIR boost)
message(STATUS "Boost version: ${Boost_VERSION}")

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

IF(APPLE)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    LINK_DIRECTORIES(/usr/local/lib)
ENDIF()

add_library(hdcp STATIC)

add_subdirectory(src)
add_subdirectory(dependencies)

target_include_directories(hdcp
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

target_link_libraries(hdcp PUBLIC common usb-1.0 ${Boost_LIBRARIES})

target_compile_options(hdcp INTERFACE -Werror -Wall -Wextra)

# Tests
include(CTest)
if(HDCP_RUNTESTS)
    add_subdirectory(test)
endif()
