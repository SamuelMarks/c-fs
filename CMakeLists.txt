# File: CMakeLists.txt
# Brief: CMake build configuration for c-fs project.
cmake_minimum_required(VERSION 3.15...3.31)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()

project(c-fs VERSION 0.1.0 LANGUAGES C)

if(DEFINED PROJECT_IS_TOP_LEVEL)
    set(CFS_IS_TOP_LEVEL "${PROJECT_IS_TOP_LEVEL}")
elseif(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(CFS_IS_TOP_LEVEL TRUE)
else()
    set(CFS_IS_TOP_LEVEL FALSE)
endif()

string(TOUPPER "${PROJECT_NAME}" PROJECT_PREFIX)
string(REPLACE "-" "_" PROJECT_PREFIX "${PROJECT_PREFIX}")

set(_CFS_CLI_HAD_COVERAGE FALSE)
if(CFS_IS_TOP_LEVEL AND "${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_DEBUG}" MATCHES "(--coverage|-fprofile-arcs|-ftest-coverage)")
    set(_CFS_CLI_HAD_COVERAGE TRUE)
endif()

# Strip global coverage flags from directory scope: prevents parent flags from leaking
# into subprojects, and converts top-level CLI -DCMAKE_C_FLAGS=--coverage into the
# target-scoped `cfs_internal_coverage` INTERFACE target.
foreach(_flag_var
    CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_EXE_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS_DEBUG
    CMAKE_SHARED_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS_DEBUG)
    if(DEFINED ${_flag_var})
        string(REPLACE "--coverage" "" ${_flag_var} "${${_flag_var}}")
        string(REPLACE "-fprofile-arcs" "" ${_flag_var} "${${_flag_var}}")
        string(REPLACE "-ftest-coverage" "" ${_flag_var} "${${_flag_var}}")
    endif()
endforeach()

# Suppress unused-variable warnings from variables passed by shared c-ci matrix scripts
set(CDD_CHARSET "" CACHE STRING "c-ci matrix override for character set")
set(CDD_DEPS "" CACHE STRING "c-ci matrix override for dependencies")
set(CDD_MSVC_RTC "" CACHE STRING "c-ci matrix override for MSVC runtime checks")
set(CDD_THREADING "" CACHE STRING "c-ci matrix override for threading mode")
mark_as_advanced(CDD_CHARSET CDD_DEPS CDD_MSVC_RTC CDD_THREADING)

# Phase 1: Build System Options
option(CFS_HEADER_ONLY "Use c-fs as a header-only INTERFACE library" OFF)

set(_CFS_EXPLICIT_SHARED_SET FALSE)
if(DEFINED CFS_BUILD_SHARED)
    set(_CFS_EXPLICIT_SHARED_SET TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "DOS" OR DOS)
    option(CFS_BUILD_SHARED "Build c-fs as a shared library" OFF)
    option(CFS_BUILD_STATIC "Build c-fs as a static library" ON)
    set(CFS_BUILD_SHARED OFF)
    set(CFS_BUILD_STATIC ON)
elseif(NOT _CFS_EXPLICIT_SHARED_SET AND DEFINED BUILD_SHARED_LIBS)
    if(BUILD_SHARED_LIBS)
        option(CFS_BUILD_SHARED "Build c-fs as a shared library" ON)
        option(CFS_BUILD_STATIC "Build c-fs as a static library" OFF)
        set(CFS_BUILD_SHARED ON)
        set(CFS_BUILD_STATIC OFF)
    else()
        option(CFS_BUILD_SHARED "Build c-fs as a shared library" OFF)
        option(CFS_BUILD_STATIC "Build c-fs as a static library" ON)
        set(CFS_BUILD_SHARED OFF)
        set(CFS_BUILD_STATIC ON)
    endif()
else()
    option(CFS_BUILD_SHARED "Build c-fs as a shared library" OFF)
    option(CFS_BUILD_STATIC "Build c-fs as a static library" ON)
endif()

if(NOT CFS_HEADER_ONLY AND NOT CFS_BUILD_SHARED AND NOT CFS_BUILD_STATIC)
    message(FATAL_ERROR "At least one of CFS_BUILD_STATIC, CFS_BUILD_SHARED, or CFS_HEADER_ONLY must be ON.")
endif()

set(${PROJECT_PREFIX}_CHARSET "Unicode" CACHE STRING "Charset to use: Unicode, ANSI, or MBCS")
set_property(CACHE ${PROJECT_PREFIX}_CHARSET PROPERTY STRINGS "Unicode" "ANSI" "MBCS")

set(${PROJECT_PREFIX}_THREADING "Multi-threaded" CACHE STRING "Threading: Multi-threaded or Single-threaded")
set_property(CACHE ${PROJECT_PREFIX}_THREADING PROPERTY STRINGS "Multi-threaded" "Single-threaded")

set(${PROJECT_PREFIX}_LTO OFF CACHE BOOL "Enable Link-Time Optimization (LTO)")

set(${PROJECT_PREFIX}_MSVC_RTC "" CACHE STRING "MSVC Runtime Checks: RTC1, RTCs, RTCu, or empty")
set_property(CACHE ${PROJECT_PREFIX}_MSVC_RTC PROPERTY STRINGS "" "RTC1" "RTCs" "RTCu" "OFF")

set(${PROJECT_PREFIX}_CRT_LINKAGE "Shared" CACHE STRING "MSVC CRT Linkage: Static or Shared")
set_property(CACHE ${PROJECT_PREFIX}_CRT_LINKAGE PROPERTY STRINGS "Shared" "Static")

option(CFS_INSTALL "Generate install and CMake package export rules for c-fs" ${CFS_IS_TOP_LEVEL})
option(${PROJECT_PREFIX}_INSTALL "Generate install and CMake package export rules for c-fs" ${CFS_INSTALL})

if(CFS_IS_TOP_LEVEL)
    option(${PROJECT_PREFIX}_COVERAGE "Enable code coverage instrumentation" OFF)
    option(CFS_COVERAGE "Enable code coverage instrumentation" ${${PROJECT_PREFIX}_COVERAGE})
    option(CFS_BUILD_TESTING "Build the c-fs test suite" OFF)
    option(${PROJECT_PREFIX}_BUILD_TESTING "Build the c-fs test suite" ${CFS_BUILD_TESTING})
    option(CFS_TESTING "Build the c-fs test suite" OFF)
    option(${PROJECT_PREFIX}_TESTING "Build the c-fs test suite" ${CFS_TESTING})
    option(c_fs_TESTING "Build the c-fs test suite" ${CFS_TESTING})
endif()

# Honor parent / alias / c-ci configuration overrides
set(_CFS_EFFECTIVE_CHARSET "${${PROJECT_PREFIX}_CHARSET}")
if(NOT CDD_CHARSET STREQUAL "")
    set(_CFS_EFFECTIVE_CHARSET "${CDD_CHARSET}")
elseif(DEFINED CFS_CHARSET AND NOT CFS_CHARSET STREQUAL "")
    set(_CFS_EFFECTIVE_CHARSET "${CFS_CHARSET}")
elseif(DEFINED CFS_UNICODE AND NOT CFS_UNICODE STREQUAL "")
    if(CFS_UNICODE)
        set(_CFS_EFFECTIVE_CHARSET "Unicode")
    else()
        set(_CFS_EFFECTIVE_CHARSET "ANSI")
    endif()
elseif(NOT CFS_IS_TOP_LEVEL)
    get_directory_property(_PARENT_COMPILE_DEFS COMPILE_DEFINITIONS)
    if("${_PARENT_COMPILE_DEFS};${CMAKE_C_FLAGS}" MATCHES "(^|[;/ -]D?)_MBCS(=[^; ]*)?($|[; ])")
        set(_CFS_EFFECTIVE_CHARSET "ANSI")
    elseif("${_PARENT_COMPILE_DEFS};${CMAKE_C_FLAGS}" MATCHES "(^|[;/ -]D?)_?UNICODE(=[^; ]*)?($|[; ])")
        set(_CFS_EFFECTIVE_CHARSET "UNICODE")
    endif()
endif()

set(_CFS_EFFECTIVE_THREADING "${${PROJECT_PREFIX}_THREADING}")
if(NOT CDD_THREADING STREQUAL "")
    set(_CFS_EFFECTIVE_THREADING "${CDD_THREADING}")
elseif(DEFINED CFS_THREADING AND NOT CFS_THREADING STREQUAL "")
    set(_CFS_EFFECTIVE_THREADING "${CFS_THREADING}")
endif()

set(_CFS_EFFECTIVE_RTC "${${PROJECT_PREFIX}_MSVC_RTC}")
if(NOT CDD_MSVC_RTC STREQUAL "")
    set(_CFS_EFFECTIVE_RTC "${CDD_MSVC_RTC}")
elseif(DEFINED CFS_MSVC_RTC AND NOT CFS_MSVC_RTC STREQUAL "")
    set(_CFS_EFFECTIVE_RTC "${CFS_MSVC_RTC}")
endif()

set(_CFS_EFFECTIVE_CRT "${${PROJECT_PREFIX}_CRT_LINKAGE}")
if(DEFINED CFS_CRT_LINKAGE AND NOT CFS_CRT_LINKAGE STREQUAL "")
    set(_CFS_EFFECTIVE_CRT "${CFS_CRT_LINKAGE}")
endif()

string(TOUPPER "${_CFS_EFFECTIVE_CHARSET}" _CFS_CHARSET_UPPER)
string(TOUPPER "${_CFS_EFFECTIVE_THREADING}" _CFS_THREADING_UPPER)
string(TOUPPER "${_CFS_EFFECTIVE_CRT}" _CFS_CRT_UPPER)

# Identify compiler frontend (handles clang.exe GNU driver vs clang-cl / cl.exe on Windows)
set(_CFS_IS_MSVC_FRONTEND FALSE)
set(_CFS_IS_GNU_FRONTEND FALSE)
if(MSVC AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    set(_CFS_IS_MSVC_FRONTEND TRUE)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    set(_CFS_IS_GNU_FRONTEND TRUE)
endif()

# Compute per-target MSVC CRT linkage & debug info properties (avoiding global mutations)
set(_CFS_MSVC_RUNTIME_LIBRARY "")
set(_CFS_MSVC_DEBUG_FORMAT "")
if(_CFS_IS_MSVC_FRONTEND)
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "")
        set(_CFS_MSVC_RUNTIME_LIBRARY "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    elseif(CFS_IS_TOP_LEVEL OR DEFINED CFS_CRT_LINKAGE OR NOT "${${PROJECT_PREFIX}_CRT_LINKAGE}" STREQUAL "Shared")
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

# Normalize MSVC RTC setting
set(_CFS_RTC_FLAG "")
if(_CFS_IS_MSVC_FRONTEND AND NOT _CFS_EFFECTIVE_RTC STREQUAL "" AND NOT _CFS_EFFECTIVE_RTC STREQUAL "OFF")
    string(REGEX REPLACE "^/" "" _CFS_RTC_CLEAN "${_CFS_EFFECTIVE_RTC}")
    if(_CFS_RTC_CLEAN STREQUAL "1" OR _CFS_RTC_CLEAN STREQUAL "s" OR _CFS_RTC_CLEAN STREQUAL "u")
        set(_CFS_RTC_FLAG "/RTC${_CFS_RTC_CLEAN}")
    else()
        set(_CFS_RTC_FLAG "/${_CFS_RTC_CLEAN}")
    endif()

    if(CFS_IS_TOP_LEVEL)
        # cl.exe rejects '/O2 ... /Od /RTC1' with command-line error D8016, so strip
        # conflicting optimization/LTO flags only in top-level RTC test builds.
        foreach(_flag_var
            CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL)
            if(DEFINED ${_flag_var})
                string(REGEX REPLACE "/RTC[1csu]+" "" ${_flag_var} "${${_flag_var}}")
                string(REGEX REPLACE "/O(1|2|x|b1|b2)" "/Od" ${_flag_var} "${${_flag_var}}")
                string(REPLACE "/GL" "" ${_flag_var} "${${_flag_var}}")
            endif()
        endforeach()
    endif()
endif()

# Internal build-only target encapsulating warning flags & private OS feature-test macros
add_library(cfs_internal_flags INTERFACE)

if(_CFS_IS_MSVC_FRONTEND)
    if(MSVC_VERSION LESS 1400)
        message(WARNING "MSVC version earlier than 2005 is not officially supported.")
    endif()
    target_compile_options(cfs_internal_flags INTERFACE "/W4" "/WX")
    if(NOT _CFS_RTC_FLAG STREQUAL "")
        target_compile_options(cfs_internal_flags INTERFACE "/Od" "${_CFS_RTC_FLAG}")
    endif()
elseif(_CFS_IS_GNU_FRONTEND)
    if(NOT "c_std_90" IN_LIST CMAKE_C_COMPILE_FEATURES)
        target_compile_options(cfs_internal_flags INTERFACE "$<$<COMPILE_LANGUAGE:C>:-ansi>")
    endif()
    target_compile_options(cfs_internal_flags INTERFACE
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
        target_compile_definitions(cfs_internal_flags INTERFACE _DARWIN_C_SOURCE)
    elseif(MINGW)
        target_compile_definitions(cfs_internal_flags INTERFACE __USE_MINGW_ANSI_STDIO=1)
    elseif(CYGWIN)
        target_compile_definitions(cfs_internal_flags INTERFACE _DEFAULT_SOURCE _GNU_SOURCE)
    elseif(UNIX OR EMSCRIPTEN)
        target_compile_definitions(cfs_internal_flags INTERFACE _DEFAULT_SOURCE _GNU_SOURCE)
    endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "Watcom")
    target_compile_options(cfs_internal_flags INTERFACE "-wx" "-we")
endif()

# Determine testing & coverage state before target configuration
set(_CFS_ENABLE_TESTING FALSE)
if(CFS_IS_TOP_LEVEL AND (BUILD_TESTING OR CFS_BUILD_TESTING OR ${PROJECT_PREFIX}_BUILD_TESTING OR CFS_TESTING OR ${PROJECT_PREFIX}_TESTING OR c_fs_TESTING))
    set(_CFS_ENABLE_TESTING TRUE)
endif()

set(_CFS_COVERAGE_ACTIVE FALSE)
if(CFS_IS_TOP_LEVEL AND (${PROJECT_PREFIX}_COVERAGE OR CFS_COVERAGE OR _CFS_CLI_HAD_COVERAGE))
    set(_CFS_COVERAGE_ACTIVE TRUE)
endif()

if(_CFS_COVERAGE_ACTIVE AND _CFS_IS_GNU_FRONTEND)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-fprofile-update=atomic" _CFS_HAS_PROFILE_ATOMIC)
    add_library(cfs_internal_coverage INTERFACE)
    target_compile_options(cfs_internal_coverage INTERFACE
        "--coverage"
        "$<$<BOOL:${_CFS_HAS_PROFILE_ATOMIC}>:-fprofile-update=atomic>"
    )
    target_link_options(cfs_internal_coverage INTERFACE "--coverage")
endif()

# Link-Time Optimization (IPO) detection
set(_CFS_WANT_LTO FALSE)
if(${PROJECT_PREFIX}_LTO OR CFS_LTO OR CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    if(_CFS_RTC_FLAG STREQUAL "" AND NOT (APPLE AND CMAKE_C_COMPILER_ID STREQUAL "GNU") AND NOT CMAKE_C_COMPILER_ID MATCHES "Watcom" AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT DOS)
        set(_CFS_WANT_LTO TRUE)
    endif()
endif()

set(_CFS_IPO_SUPPORTED FALSE)
if(_CFS_WANT_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _CFS_IPO_SUPPORTED OUTPUT _CFS_IPO_ERROR)
    if(NOT _CFS_IPO_SUPPORTED)
        message(WARNING "IPO/LTO is not supported: ${_CFS_IPO_ERROR}")
    endif()
endif()

set(_CFS_USE_UNICODE FALSE)
if(_CFS_CHARSET_UPPER STREQUAL "UNICODE" OR _CFS_CHARSET_UPPER STREQUAL "ON" OR
   _CFS_CHARSET_UPPER STREQUAL "TRUE" OR _CFS_CHARSET_UPPER STREQUAL "1")
    set(_CFS_USE_UNICODE TRUE)
endif()

set(_CFS_USE_MULTITHREAD FALSE)
if(_CFS_THREADING_UPPER STREQUAL "MULTI-THREADED" OR
   _CFS_THREADING_UPPER STREQUAL "MULTITHREADED" OR
   _CFS_THREADING_UPPER STREQUAL "MULTI" OR
   _CFS_THREADING_UPPER STREQUAL "ON" OR
   _CFS_THREADING_UPPER STREQUAL "TRUE" OR
   _CFS_THREADING_UPPER STREQUAL "1")
    set(_CFS_USE_MULTITHREAD TRUE)
endif()

if(_CFS_USE_MULTITHREAD AND NOT WIN32 AND NOT EMSCRIPTEN AND NOT DOS AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBDIR AND (CMAKE_SYSTEM_NAME STREQUAL "DOS" OR DOS))
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Object code libraries")
endif()
include(GNUInstallDirs)

set(CFS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CFS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/cfs.c")

# Per-target configuration helpers (eliminating global directory/flag side effects)
function(cfs_apply_common_target_properties target_name)
    if(CFS_IS_TOP_LEVEL)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
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
endfunction()

function(cfs_apply_c_target_settings target_name)
    cfs_apply_common_target_properties(${target_name})

    if("c_std_90" IN_LIST CMAKE_C_COMPILE_FEATURES)
        set_target_properties(${target_name} PROPERTIES
            C_STANDARD 90
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
        )
    endif()

    target_link_libraries(${target_name} PRIVATE $<BUILD_INTERFACE:cfs_internal_flags>)
endfunction()

function(cfs_configure_compiled_library target_name)
    cfs_apply_c_target_settings(${target_name})

    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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

# Primary library target definition
if(CFS_HEADER_ONLY)
    add_library(cfs INTERFACE)
    target_include_directories(cfs INTERFACE
        $<BUILD_INTERFACE:${CFS_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
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
        add_library(cfs SHARED "${CFS_SOURCES}")
    else()
        add_library(cfs STATIC "${CFS_SOURCES}")
    endif()

    cfs_configure_compiled_library(cfs)

    if(TARGET cfs_internal_coverage AND NOT _CFS_ENABLE_TESTING)
        target_link_libraries(cfs PUBLIC $<BUILD_INTERFACE:cfs_internal_coverage>)
    endif()
endif()

if(NOT TARGET cfs::cfs)
    add_library(cfs::cfs ALIAS cfs)
endif()

# Install & CMake package config export rules
if(CFS_INSTALL AND ${PROJECT_PREFIX}_INSTALL)
    include(CMakePackageConfigHelpers)

    set(_CFS_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cfs")
    set(_CFS_NEEDS_THREADS_DEPENDENCY FALSE)
    if(TARGET Threads::Threads)
        set(_CFS_NEEDS_THREADS_DEPENDENCY TRUE)
    endif()

    install(TARGETS cfs
        EXPORT cfs-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY "${CFS_INCLUDE_DIR}/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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

    if(CFS_HEADER_ONLY)
        write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/cfsConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
            ARCH_INDEPENDENT
        )
    else()
        write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/cfsConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
        )
    endif()

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cfsConfigVersion.cmake"
        DESTINATION "${_CFS_CMAKE_CONFIG_DESTINATION}"
    )
endif()

# Testing (strictly top-level only)
if(_CFS_ENABLE_TESTING)
    include(CTest)
    add_subdirectory(tests)
endif()
