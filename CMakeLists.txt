cmake_minimum_required(VERSION 3.1...3.28)
project(c-fs LANGUAGES C)

# Suppress warnings from unused variables passed by legacy c-ci scripts
set(CDD_CHARSET "" CACHE STRING "")
set(CDD_DEPS "" CACHE STRING "")
set(CDD_MSVC_RTC "" CACHE STRING "")
set(CDD_THREADING "" CACHE STRING "")

# Phase 1: Build System Initialization
option(CFS_BUILD_SHARED "Build c-fs as a shared library" OFF)
option(CFS_BUILD_STATIC "Build c-fs as a static library" ON)
option(CFS_HEADER_ONLY "Use c-fs as a header-only library" OFF)
option(CFS_UNICODE "Use Unicode (Wide char) APIs natively on Windows" ON)
option(CFS_MULTITHREADED "Enable thread-safe error handling and primitives" ON)
option(CFS_ENABLE_LTO "Enable Link-Time Optimization" OFF)

# Phase 2: MSVC & Toolchain Configuration
if(MSVC)
    option(CFS_MSVC_CRT_STATIC "Link MSVC statically (/MT)" OFF)
    option(CFS_MSVC_CRT_DEBUG "Link MSVC statically with debug (/MTd) or shared debug (/MDd)" OFF)
    option(CFS_MSVC_RTC1 "Enable MSVC runtime checks (/RTC1)" OFF)
    option(CFS_MSVC_RTCS "Enable MSVC stack frame runtime checks (/RTCs)" OFF)
    option(CFS_MSVC_RTCU "Enable MSVC uninitialized variable checks (/RTCu)" OFF)
endif()

# Compiler flags setup
set(CFS_C_FLAGS "")

if(MSVC)
    # Detect MSVC 2005 to 2022/2026
    if(MSVC_VERSION LESS 1400) # Pre-2005
        message(WARNING "MSVC version earlier than 2005 is not officially supported.")
    endif()
    
    # CRT Linkage (Note: CMake 3.15+ supports CMAKE_MSVC_RUNTIME_LIBRARY but we target 3.1)
    if(CFS_MSVC_CRT_STATIC)
        if(CFS_MSVC_CRT_DEBUG OR CMAKE_BUILD_TYPE STREQUAL "Debug")
            string(REPLACE "/MDd" "/MTd" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
            string(REPLACE "/MD" "/MTd" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        else()
            string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
            string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif()
    else()
        if(CFS_MSVC_CRT_DEBUG OR CMAKE_BUILD_TYPE STREQUAL "Debug")
            string(REPLACE "/MTd" "/MDd" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
            string(REPLACE "/MT" "/MDd" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        else()
            string(REPLACE "/MT" "/MD" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
            string(REPLACE "/MT" "/MD" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
        endif()
    endif()

    # Disable warning C4996 for standard C functions that MSVC thinks are deprecated
    list(APPEND CFS_C_FLAGS "/wd4996")

    # RTC checks
    if(CFS_MSVC_RTC1)
        list(APPEND CFS_C_FLAGS "/RTC1")
    endif()
    if(CFS_MSVC_RTCS)
        list(APPEND CFS_C_FLAGS "/RTCs")
    endif()
    if(CFS_MSVC_RTCU)
        list(APPEND CFS_C_FLAGS "/RTCu")
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    list(APPEND CFS_C_FLAGS "-std=c89" "-pedantic" "-Wall" "-Wextra")
    
    if(MINGW)
        # MinGW specific overrides
        list(APPEND CFS_C_FLAGS "-D__USE_MINGW_ANSI_STDIO=1")
    elseif(CYGWIN)
        # Cygwin specific overrides
        list(APPEND CFS_C_FLAGS "-D_DEFAULT_SOURCE")
    endif()
endif()

if(CFS_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO is not supported: ${error}")
    endif()
endif()

# Source and include directories
set(CFS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CFS_SOURCES "")

if(NOT CFS_HEADER_ONLY)
    set(CFS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cfs.c")
endif()

# Target definition
if(CFS_HEADER_ONLY)
    add_library(cfs INTERFACE)
    target_include_directories(cfs INTERFACE 
        $<BUILD_INTERFACE:${CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    if(CFS_UNICODE)
        target_compile_definitions(cfs INTERFACE CFS_UNICODE)
    endif()
    if(CFS_MULTITHREADED)
        target_compile_definitions(cfs INTERFACE CFS_MULTITHREADED)
    endif()
else()
    if(CFS_BUILD_SHARED)
        add_library(cfs SHARED ${CFS_SOURCES})
        target_compile_definitions(cfs PRIVATE CFS_EXPORTS)
    elseif(CFS_BUILD_STATIC)
        add_library(cfs STATIC ${CFS_SOURCES})
    endif()

    target_include_directories(cfs PUBLIC 
        $<BUILD_INTERFACE:${CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_options(cfs PRIVATE ${CFS_C_FLAGS})
    
    if(CFS_UNICODE)
        target_compile_definitions(cfs PUBLIC CFS_UNICODE)
    endif()
    if(CFS_MULTITHREADED)
        target_compile_definitions(cfs PUBLIC CFS_MULTITHREADED)
    endif()
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS cfs
    EXPORT cfs-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CFS_INCLUDE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT cfs-targets
    FILE cfs-targets.cmake
    NAMESPACE cfs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cfs
)

# Testing
enable_testing()
add_subdirectory(tests)
