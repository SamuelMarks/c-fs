# File: CMakeLists.txt
# Brief: Target-scoped CMake build configuration for c-fs.
cmake_minimum_required(VERSION 3.18...3.31)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(c-fs VERSION 0.1.0 LANGUAGES C)

if(DEFINED PROJECT_IS_TOP_LEVEL)
    set(CFS_IS_TOP_LEVEL "${PROJECT_IS_TOP_LEVEL}")
else()
    string(COMPARE EQUAL "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}" CFS_IS_TOP_LEVEL)
endif()

# Build options
option(CFS_HEADER_ONLY "Use c-fs as a header-only INTERFACE library" OFF)

set(_CFS_DEFAULT_SHARED OFF)
if(DEFINED BUILD_SHARED_LIBS AND BUILD_SHARED_LIBS)
    set(_CFS_DEFAULT_SHARED ON)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "DOS" OR DOS)
    set(_CFS_DEFAULT_SHARED OFF)
endif()

option(CFS_BUILD_SHARED "Build c-fs as a shared library" ${_CFS_DEFAULT_SHARED})
if(CFS_BUILD_SHARED)
    set(_CFS_DEFAULT_STATIC OFF)
else()
    set(_CFS_DEFAULT_STATIC ON)
endif()
option(CFS_BUILD_STATIC "Build c-fs as a static library" ${_CFS_DEFAULT_STATIC})

if(CMAKE_SYSTEM_NAME STREQUAL "DOS" OR DOS)
    set(CFS_BUILD_SHARED OFF)
    set(CFS_BUILD_STATIC ON)
endif()

if(NOT CFS_HEADER_ONLY AND NOT CFS_BUILD_SHARED AND NOT CFS_BUILD_STATIC)
    message(FATAL_ERROR "At least one of CFS_BUILD_STATIC, CFS_BUILD_SHARED, or CFS_HEADER_ONLY must be ON.")
endif()

set(CFS_CHARSET "Unicode" CACHE STRING "Charset to use: Unicode, ANSI, or MBCS")
set_property(CACHE CFS_CHARSET PROPERTY STRINGS "Unicode" "ANSI" "MBCS")

set(CFS_THREADING "Multi-threaded" CACHE STRING "Threading: Multi-threaded or Single-threaded")
set_property(CACHE CFS_THREADING PROPERTY STRINGS "Multi-threaded" "Single-threaded")

option(CFS_LTO "Enable Link-Time Optimization (IPO/LTO)" OFF)

set(CFS_MSVC_RTC "" CACHE STRING "MSVC Runtime Checks: RTC1, RTCs, RTCu, or OFF")
set_property(CACHE CFS_MSVC_RTC PROPERTY STRINGS "" "RTC1" "RTCs" "RTCu" "OFF")

set(CFS_CRT_LINKAGE "Shared" CACHE STRING "MSVC CRT Linkage: Shared or Static")
set_property(CACHE CFS_CRT_LINKAGE PROPERTY STRINGS "Shared" "Static")

option(CFS_INSTALL "Generate install and CMake package export rules for c-fs" ${CFS_IS_TOP_LEVEL})

if(CFS_IS_TOP_LEVEL)
    option(CFS_COVERAGE "Enable code coverage instrumentation" OFF)
    option(CFS_BUILD_TESTING "Build the c-fs test suite" OFF)
endif()

# Acknowledge optional CI / superproject pass-through variables without polluting cache
set(_cfs_cli_passthrough
    "${CDD_CHARSET}${CDD_THREADING}${CDD_MSVC_RTC}${CDD_DEPS}"
    "${CFS_UNICODE}${CFS_BUILD_TESTS}${C_FS_BUILD_TESTS}"
    "${C_FS_BUILD_TESTING}${C_FS_TESTING}${c_fs_TESTING}${CFS_TESTING}"
)
unset(_cfs_cli_passthrough)

# Resolve effective configuration (honoring c-ci matrix overrides when provided)
set(_CFS_EFFECTIVE_CHARSET "${CFS_CHARSET}")
if(DEFINED CDD_CHARSET AND NOT CDD_CHARSET STREQUAL "")
    set(_CFS_EFFECTIVE_CHARSET "${CDD_CHARSET}")
elseif(DEFINED CFS_UNICODE AND NOT CFS_UNICODE STREQUAL "")
    if(CFS_UNICODE)
        set(_CFS_EFFECTIVE_CHARSET "Unicode")
    else()
        set(_CFS_EFFECTIVE_CHARSET "ANSI")
    endif()
endif()

set(_CFS_EFFECTIVE_THREADING "${CFS_THREADING}")
if(DEFINED CDD_THREADING AND NOT CDD_THREADING STREQUAL "")
    set(_CFS_EFFECTIVE_THREADING "${CDD_THREADING}")
endif()

set(_CFS_EFFECTIVE_RTC "${CFS_MSVC_RTC}")
if(DEFINED CDD_MSVC_RTC AND NOT CDD_MSVC_RTC STREQUAL "")
    set(_CFS_EFFECTIVE_RTC "${CDD_MSVC_RTC}")
endif()

string(TOUPPER "${_CFS_EFFECTIVE_CHARSET}" _CFS_CHARSET_UPPER)
string(TOUPPER "${_CFS_EFFECTIVE_THREADING}" _CFS_THREADING_UPPER)
string(TOUPPER "${CFS_CRT_LINKAGE}" _CFS_CRT_UPPER)

# Identify compiler frontend (distinguishes clang GNU driver vs clang-cl / cl.exe)
set(_CFS_IS_MSVC_FRONTEND FALSE)
set(_CFS_IS_GNU_FRONTEND FALSE)
if(MSVC AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    set(_CFS_IS_MSVC_FRONTEND TRUE)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    set(_CFS_IS_GNU_FRONTEND TRUE)
endif()

# Per-target MSVC CRT linkage & debug information properties
set(_CFS_MSVC_RUNTIME_LIBRARY "")
set(_CFS_MSVC_DEBUG_FORMAT "")
if(_CFS_IS_MSVC_FRONTEND)
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
        set(_CFS_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    elseif(CFS_IS_TOP_LEVEL OR NOT "${CFS_CRT_LINKAGE}" STREQUAL "Shared")
        if(_CFS_CRT_UPPER STREQUAL "STATIC" OR _CFS_CRT_UPPER STREQUAL "MT")
            set(_CFS_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        else()
            set(_CFS_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        endif()
    endif()

    if(POLICY CMP0141)
        if(DEFINED CMAKE_MSVC_DEBUG_INFORMATION_FORMAT AND NOT CMAKE_MSVC_DEBUG_INFORMATION_FORMAT STREQUAL "")
            set(_CFS_MSVC_DEBUG_FORMAT "${CMAKE_MSVC_DEBUG_INFORMATION_FORMAT}")
        elseif(CFS_IS_TOP_LEVEL)
            set(_CFS_MSVC_DEBUG_FORMAT "Embedded")
        endif()
    endif()
endif()

# Normalize per-target MSVC runtime checks (/RTC1, /RTCs, /RTCu)
set(_CFS_RTC_FLAG "")
if(_CFS_IS_MSVC_FRONTEND AND NOT _CFS_EFFECTIVE_RTC STREQUAL "" AND NOT _CFS_EFFECTIVE_RTC STREQUAL "OFF")
    string(REGEX REPLACE "^/" "" _CFS_RTC_CLEAN "${_CFS_EFFECTIVE_RTC}")
    if(_CFS_RTC_CLEAN STREQUAL "1" OR _CFS_RTC_CLEAN STREQUAL "s" OR _CFS_RTC_CLEAN STREQUAL "u")
        set(_CFS_RTC_FLAG "/RTC${_CFS_RTC_CLEAN}")
    else()
        set(_CFS_RTC_FLAG "/${_CFS_RTC_CLEAN}")
    endif()
endif()

# Internal build-only INTERFACE target for strict compiler warnings & private OS macros
add_library(cfs_build_flags INTERFACE)

if(_CFS_IS_MSVC_FRONTEND)
    if(MSVC_VERSION LESS 1400)
        message(WARNING "MSVC version earlier than 2005 is not officially supported.")
    endif()
    target_compile_options(cfs_build_flags INTERFACE "/W4" "/WX")
    if(NOT _CFS_RTC_FLAG STREQUAL "")
        target_compile_options(cfs_build_flags INTERFACE "/Od" "/Ob0" "${_CFS_RTC_FLAG}")
    endif()
elseif(_CFS_IS_GNU_FRONTEND)
    if(NOT "c_std_90" IN_LIST CMAKE_C_COMPILE_FEATURES)
        target_compile_options(cfs_build_flags INTERFACE "$<$<COMPILE_LANGUAGE:C>:-ansi>")
    endif()
    target_compile_options(cfs_build_flags INTERFACE
        "-pedantic"
        "-Wall"
        "-Wextra"
        "-Werror"
        "-Wconversion"
        "-Wsign-conversion"
        "-Wcast-align"
        "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>"
        "$<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>"
        "$<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>"
        "$<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>"
        "-Wpointer-arith"
        "-Wcast-qual"
        "-Wshadow"
    )
    if(APPLE)
        target_compile_definitions(cfs_build_flags INTERFACE _DARWIN_C_SOURCE)
    elseif(MINGW)
        target_compile_definitions(cfs_build_flags INTERFACE __USE_MINGW_ANSI_STDIO=1)
    elseif(UNIX OR CYGWIN OR EMSCRIPTEN)
        target_compile_definitions(cfs_build_flags INTERFACE _DEFAULT_SOURCE _GNU_SOURCE)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Watcom")
    target_compile_options(cfs_build_flags INTERFACE "-wx" "-we")
endif()

# Top-level testing & coverage flags
set(_CFS_ENABLE_TESTING FALSE)
if(CFS_IS_TOP_LEVEL AND (BUILD_TESTING OR CFS_BUILD_TESTING OR C_FS_BUILD_TESTING OR CFS_TESTING OR C_FS_TESTING OR c_fs_TESTING))
    set(_CFS_ENABLE_TESTING TRUE)
endif()

if(CFS_IS_TOP_LEVEL AND _CFS_IS_GNU_FRONTEND AND (CFS_COVERAGE OR "${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_DEBUG}" MATCHES "(--coverage|-fprofile-arcs|-ftest-coverage)"))
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-fprofile-update=atomic" _CFS_HAS_PROFILE_ATOMIC)
    add_library(cfs_coverage_flags INTERFACE)
    target_compile_options(cfs_coverage_flags INTERFACE
        "--coverage"
        "$<$<BOOL:${_CFS_HAS_PROFILE_ATOMIC}>:-fprofile-update=atomic>"
    )
    target_link_options(cfs_coverage_flags INTERFACE "--coverage")
endif()

# Per-target Link-Time Optimization (IPO) detection
set(_CFS_IPO_SUPPORTED FALSE)
if((CFS_LTO OR CMAKE_INTERPROCEDURAL_OPTIMIZATION)
   AND _CFS_RTC_FLAG STREQUAL ""
   AND NOT (APPLE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
   AND NOT CMAKE_C_COMPILER_ID MATCHES "Watcom"
   AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS"
   AND NOT DOS)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _CFS_IPO_SUPPORTED OUTPUT _CFS_IPO_ERROR)
    if(NOT _CFS_IPO_SUPPORTED)
        message(WARNING "IPO/LTO is not supported: ${_CFS_IPO_ERROR}")
    endif()
endif()

set(_CFS_USE_UNICODE FALSE)
if(_CFS_CHARSET_UPPER MATCHES "^(UNICODE|ON|TRUE|1)$")
    set(_CFS_USE_UNICODE TRUE)
endif()

set(_CFS_USE_MULTITHREAD FALSE)
if(_CFS_THREADING_UPPER MATCHES "^(MULTI-THREADED|MULTITHREADED|MULTI|ON|TRUE|1)$")
    set(_CFS_USE_MULTITHREAD TRUE)
endif()

function(_cfs_find_threads)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endfunction()

if(_CFS_USE_MULTITHREAD AND NOT WIN32 AND NOT EMSCRIPTEN AND NOT DOS AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    _cfs_find_threads()
endif()

if(CFS_INSTALL AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT DOS)
    include(GNUInstallDirs)
    set(_CFS_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    set(_CFS_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    set(_CFS_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
else()
    set(_CFS_INSTALL_INCLUDEDIR "include")
    set(_CFS_INSTALL_LIBDIR "lib")
    set(_CFS_INSTALL_BINDIR "bin")
endif()

set(_CFS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(_CFS_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/cfs.c")

# Target-scoped helper functions
function(cfs_apply_target_defaults target_name)
    if(CFS_IS_TOP_LEVEL)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        )
    endif()

    if("c_std_90" IN_LIST CMAKE_C_COMPILE_FEATURES)
        set_target_properties(${target_name} PROPERTIES
            C_STANDARD 90
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
        )
    endif()

    if(NOT _CFS_MSVC_RUNTIME_LIBRARY STREQUAL "")
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY "${_CFS_MSVC_RUNTIME_LIBRARY}")
    endif()

    if(NOT _CFS_MSVC_DEBUG_FORMAT STREQUAL "")
        set_property(TARGET ${target_name} PROPERTY MSVC_DEBUG_INFORMATION_FORMAT "${_CFS_MSVC_DEBUG_FORMAT}")
    endif()

    set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION ${_CFS_IPO_SUPPORTED})
    if(_CFS_IPO_SUPPORTED)
        target_compile_definitions(${target_name} PRIVATE CFS_ENABLE_LTO)
    endif()

    target_link_libraries(${target_name} PRIVATE $<BUILD_INTERFACE:cfs_build_flags>)
endfunction()

function(cfs_configure_compiled_library target_name)
    cfs_apply_target_defaults(${target_name})

    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${_CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${_CFS_INSTALL_INCLUDEDIR}>
    )

    if(CFS_BUILD_SHARED)
        target_compile_definitions(${target_name}
            PRIVATE CFS_EXPORTS
            PUBLIC CFS_BUILD_SHARED
        )
        set_target_properties(${target_name} PROPERTIES
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
        )
    endif()

    if(_CFS_USE_UNICODE)
        target_compile_definitions(${target_name} PUBLIC CFS_UNICODE)
    elseif(WIN32 AND (_CFS_CHARSET_UPPER STREQUAL "MBCS" OR _CFS_CHARSET_UPPER STREQUAL "_MBCS"))
        target_compile_definitions(${target_name} PRIVATE _MBCS)
    endif()

    if(_CFS_USE_MULTITHREAD)
        target_compile_definitions(${target_name} PUBLIC CFS_MULTITHREADED)
        if(EMSCRIPTEN)
            target_compile_options(${target_name} PUBLIC "-pthread")
            target_link_options(${target_name} PUBLIC "-pthread")
        endif()
    endif()

    if(TARGET Threads::Threads)
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
    endif()
endfunction()

# Primary library target
if(CFS_HEADER_ONLY)
    add_library(cfs INTERFACE)
    target_include_directories(cfs INTERFACE
        $<BUILD_INTERFACE:${_CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${_CFS_INSTALL_INCLUDEDIR}>
    )
    if(MINGW)
        target_compile_definitions(cfs INTERFACE __USE_MINGW_ANSI_STDIO=1)
    endif()
    if(_CFS_USE_UNICODE)
        target_compile_definitions(cfs INTERFACE CFS_UNICODE)
    endif()
    if(_CFS_USE_MULTITHREAD)
        target_compile_definitions(cfs INTERFACE CFS_MULTITHREADED)
        if(EMSCRIPTEN)
            target_compile_options(cfs INTERFACE "-pthread")
            target_link_options(cfs INTERFACE "-pthread")
        endif()
    endif()
    if(TARGET Threads::Threads)
        target_link_libraries(cfs INTERFACE Threads::Threads)
    endif()
else()
    if(CFS_BUILD_SHARED)
        add_library(cfs SHARED "${_CFS_SOURCE_FILE}")
    else()
        add_library(cfs STATIC "${_CFS_SOURCE_FILE}")
    endif()

    cfs_configure_compiled_library(cfs)

    if(TARGET cfs_coverage_flags)
        if(_CFS_ENABLE_TESTING)
            if(UNIX AND CMAKE_C_COMPILER_ID STREQUAL "GNU"
               AND "${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_DEBUG}" MATCHES "(--coverage|-fprofile-arcs|-ftest-coverage)")
                target_compile_options(cfs PRIVATE "-fprofile-note=/dev/null" "-fprofile-dir=/tmp")
            endif()
        else()
            target_link_libraries(cfs PUBLIC $<BUILD_INTERFACE:cfs_coverage_flags>)
        endif()
    endif()
endif()

if(NOT TARGET cfs::cfs)
    add_library(cfs::cfs ALIAS cfs)
endif()

# Install & CMake package config export rules
if(CFS_INSTALL)
    include(CMakePackageConfigHelpers)

    set(_CFS_CMAKE_CONFIG_DESTINATION "${_CFS_INSTALL_LIBDIR}/cmake/cfs")
    set(_CFS_NEEDS_THREADS_DEPENDENCY FALSE)
    if(TARGET Threads::Threads AND (CFS_HEADER_ONLY OR NOT CFS_BUILD_SHARED))
        set(_CFS_NEEDS_THREADS_DEPENDENCY TRUE)
    endif()

    install(TARGETS cfs
        EXPORT cfs-targets
        LIBRARY DESTINATION "${_CFS_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${_CFS_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${_CFS_INSTALL_BINDIR}"
    )

    install(DIRECTORY "${_CFS_INCLUDE_DIR}/"
        DESTINATION "${_CFS_INSTALL_INCLUDEDIR}"
    )

    install(EXPORT cfs-targets
        FILE cfs-targets.cmake
        NAMESPACE cfs::
        DESTINATION "${_CFS_CMAKE_CONFIG_DESTINATION}"
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cfsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfig.cmake"
        INSTALL_DESTINATION "${_CFS_CMAKE_CONFIG_DESTINATION}"
    )

    set(_cfs_arch_opt "")
    if(CFS_HEADER_ONLY)
        set(_cfs_arch_opt ARCH_INDEPENDENT)
    endif()
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ${_cfs_arch_opt}
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfigVersion.cmake"
        DESTINATION "${_CFS_CMAKE_CONFIG_DESTINATION}"
    )
endif()

# Testing (top-level only)
if(_CFS_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
