cmake_minimum_required(VERSION 3.16)
project(pnet_virtual_device LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED PNET_SDK_DIR)
    set(PNET_SDK_DIR "$ENV{PNET_SDK_DIR}")
endif()

if(NOT PNET_SDK_DIR)
    message(FATAL_ERROR "Provide PNET_SDK_DIR pointing at an extracted P-Net Linux SDK package")
endif()

set(PNET_CMAKE_DIR "${PNET_SDK_DIR}/cmake")

if(NOT EXISTS "${PNET_CMAKE_DIR}/PNetConfig.cmake")
    message(FATAL_ERROR "${PNET_SDK_DIR} does not contain cmake/PNetConfig.cmake")
endif()

find_package(Threads REQUIRED)
find_package(Osal REQUIRED CONFIG PATHS ${PNET_CMAKE_DIR} NO_DEFAULT_PATH)
find_package(PNet REQUIRED CONFIG PATHS ${PNET_CMAKE_DIR} NO_DEFAULT_PATH)

foreach(imported_target IN ITEMS osal pnet)
    if(TARGET ${imported_target})
        set_property(TARGET ${imported_target} PROPERTY MAP_IMPORTED_CONFIG_DEBUG RelWithDebInfo)
        set_property(TARGET ${imported_target} PROPERTY MAP_IMPORTED_CONFIG_RELEASE RelWithDebInfo)
        set_property(TARGET ${imported_target} PROPERTY MAP_IMPORTED_CONFIG_MINSIZEREL RelWithDebInfo)
        set_property(TARGET ${imported_target} PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo)
    endif()
endforeach()

add_executable(pnet_virtual_device
    pnet_virtual_device.c
)

target_link_libraries(pnet_virtual_device PRIVATE
    pnet
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pnet_virtual_device PRIVATE -Wall -Wextra -Wpedantic)
endif()