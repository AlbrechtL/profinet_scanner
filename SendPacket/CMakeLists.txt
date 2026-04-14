add_executable(pn_scanner
    common.h
    deviceHandler.c
    linkedList.c
    main.c
    npcap_loader.c
    packetCapture.c
    packetHandler.c
    platform_compat.c
    remoteScan.c
)

target_include_directories(pn_scanner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(pn_scanner PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(pn_scanner PRIVATE iphlpapi ws2_32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCAP REQUIRED IMPORTED_TARGET libpcap)
    target_link_libraries(pn_scanner PRIVATE PkgConfig::PCAP)
    target_compile_options(pn_scanner PRIVATE ${PCAP_CFLAGS_OTHER})
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pn_scanner PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS pn_scanner RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
