cmake_minimum_required(VERSION 3.25)
project(libads VERSION 0.3.0 LANGUAGES CXX)

include(GNUInstallDirs)
find_package(Threads REQUIRED)

# Replacement for the TcAdsDll: ADS through the local TwinCAT router (TCP 127.0.0.1:48898) with the C functions of
# the TcAdsDll. Further ways to reach ADS devices plug in below the C-API (AdsBackend.h); libads-client adds the
# route way without TwinCAT and the C++ classes.
# Off when the library is part of another project (FetchContent, add_subdirectory).
option(LIBADS_INSTALL "Install rules and CMake package" ${PROJECT_IS_TOP_LEVEL})
option(LIBADS_BUILD_TESTS "Build the test programs" ${PROJECT_IS_TOP_LEVEL})

add_library(ads
    src/core/AdsDef.cpp
    src/core/AmsNetId.cpp
    src/core/Frame.cpp
    src/core/Log.cpp
    src/connection/AmsConnection.cpp
    src/connection/AmsTransport.cpp
    src/connection/NotificationDispatcher.cpp
    src/connection/Sockets.cpp
    src/connection/Subscriptions.cpp
    src/local/LocalRouter.cpp
    src/local/StandaloneAdsLib.cpp
)
add_library(libads::ads ALIAS ads)
# Static only (no export macros); Debug and Release can be installed side by side. The version for libads-client,
# which needs exactly this one.
set_target_properties(ads PROPERTIES DEBUG_POSTFIX d
    LIBADS_VERSION ${PROJECT_VERSION} EXPORT_PROPERTIES LIBADS_VERSION)
# Settings as in Beckhoff ADS.
target_compile_features(ads PUBLIC cxx_std_14)
target_compile_definitions(ads PRIVATE CONFIG_DEFAULT_LOGLEVEL=1
    LIBADS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    LIBADS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    LIBADS_VERSION_PATCH=${PROJECT_VERSION_PATCH})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_definitions(ads PRIVATE _GNU_SOURCE)
endif()
# Installed flat, so that #include "TcAdsAPI.h" works as with the TcAdsDll.
target_include_directories(ads PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/local>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/core>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libads>"
)
target_include_directories(ads PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/connection)
# TcAdsAPI.h, StandaloneAdsLib.h and what they include.
set(LIBADS_PUBLIC_HEADERS
    src/local/TcAdsAPI.h
    src/local/TcAdsDef.h
    src/local/StandaloneAdsLib.h
    src/core/AdsDef.h
    src/core/StandaloneAdsDef.h
    src/core/wrap_endian.h
)
target_link_libraries(ads PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(ads PUBLIC ws2_32)
    # The public headers pull in winsock2.h, so NOMINMAX has to reach the users as well.
    target_compile_definitions(ads PUBLIC NOMINMAX=1 PRIVATE _CRT_NONSTDC_NO_WARNINGS=1 _CRT_SECURE_NO_WARNINGS=1)
endif()

# The inner headers (connection layer, protocol, local router) for libads-client and the tests. Not a stable API:
# they change with every version, so libads-client asks for one exact version of libads.
add_library(ads-internal INTERFACE)
add_library(libads::ads-internal ALIAS ads-internal)
target_include_directories(ads-internal INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/connection>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libads/detail>"
)
target_link_libraries(ads-internal INTERFACE ads)
file(GLOB LIBADS_INTERNAL_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    src/core/*.h src/connection/*.h src/local/*.h)
list(REMOVE_ITEM LIBADS_INTERNAL_HEADERS ${LIBADS_PUBLIC_HEADERS})

if(LIBADS_INSTALL)
    include(CMakePackageConfigHelpers)
    install(TARGETS ads ads-internal EXPORT libadsTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${LIBADS_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libads)
    install(FILES ${LIBADS_INTERNAL_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libads/detail)
    install(FILES LICENSE LICENSE.Beckhoff DESTINATION ${CMAKE_INSTALL_DOCDIR})

    set(LIBADS_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/libads)
    install(EXPORT libadsTargets NAMESPACE libads:: DESTINATION ${LIBADS_CMAKE_DIR})
    configure_package_config_file(cmake/libadsConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/libadsConfig.cmake
        INSTALL_DESTINATION ${LIBADS_CMAKE_DIR})
    # Before 1.0 a new minor version may change the API.
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/libadsConfigVersion.cmake
        COMPATIBILITY SameMinorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/libadsConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/libadsConfigVersion.cmake
        DESTINATION ${LIBADS_CMAKE_DIR})
endif()

if(LIBADS_BUILD_TESTS)
    # CTest runs only what needs no TwinCAT: the tests against the fake router. The others need TwinCAT and are
    # started by hand (below). Each fake-router test has its own ports, so they can run in parallel (ctest -j). Under
    # WSL Linux and Windows share ports, so Linux uses others by default.
    enable_testing()
    if(WIN32)
        set(LIBADS_TEST_PORT_DEFAULT 48910)
    else()
        set(LIBADS_TEST_PORT_DEFAULT 49110)
    endif()
    set(LIBADS_TEST_PORT ${LIBADS_TEST_PORT_DEFAULT} CACHE STRING
        "First port of the fake-router tests in CTest (uses this one up to +11)")
    add_subdirectory(test/fake-router)
    add_subdirectory(test/reconnect-fake)
    add_subdirectory(test/subscriptions-fake)

    # Programs against a real TwinCAT system, started by hand. Not part of every source copy, hence only if present.
    function(libads_manual_test dir)
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/${dir}/CMakeLists.txt)
            add_subdirectory(test/${dir})
        endif()
    endfunction()
    libads_manual_test(tc-control)
    libads_manual_test(reconnect)
    # These use windows.h or Winsock directly.
    if(WIN32)
        libads_manual_test(tcadsdll-compat)
        libads_manual_test(router-probe)
        libads_manual_test(router-watch)
    endif()
endif()
