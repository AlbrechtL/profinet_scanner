cmake_minimum_required(VERSION 3.16)
project(profipp_virtual_device LANGUAGES CXX)

cmake_policy(SET CMP0115 NEW)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
find_package(profipp 1.0 REQUIRED)

add_executable(profipp_virtual_device
    profipp_virtual_device.cpp
)

target_compile_features(profipp_virtual_device PRIVATE cxx_std_17)

target_link_libraries(profipp_virtual_device PRIVATE
    Threads::Threads
    profipp
)

install(TARGETS profipp_virtual_device DESTINATION bin)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(profipp_virtual_device PRIVATE -Wall -Wextra -Wpedantic)
endif()