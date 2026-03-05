add_executable(pn_scanner
    deviceHandler.c
    fileHandler.c
    linkedList.c
    main.c
    packetCapture.c
    packetHandler.c
    platform_compat.c
    remoteScan.c
)

target_include_directories(pn_scanner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

target_include_directories(pn_scanner PRIVATE ${PCAP_INCLUDE_DIRS})
target_link_directories(pn_scanner PRIVATE ${PCAP_LIBRARY_DIRS})
target_link_libraries(pn_scanner PRIVATE ${PCAP_LIBRARIES} pthread)
target_compile_options(pn_scanner PRIVATE ${PCAP_CFLAGS_OTHER})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pn_scanner PRIVATE -Wall -Wextra -Wpedantic)
endif()
