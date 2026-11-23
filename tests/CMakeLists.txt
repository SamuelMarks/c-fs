# File: tests/CMakeLists.txt
# Brief: Target-scoped CMake build configuration for c-fs tests.

include(FetchContent)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest")
  message(STATUS "Fetching greatest from local directory")
  FetchContent_Declare(
    greatest
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest"
  )
else()
  FetchContent_Declare(
    greatest
    URL https://raw.githubusercontent.com/SamuelMarks/greatest/master/greatest.h
    DOWNLOAD_NO_EXTRACT TRUE
  )
endif()
FetchContent_MakeAvailable(greatest)

add_library(cfs_greatest INTERFACE)
target_include_directories(cfs_greatest SYSTEM INTERFACE "${greatest_SOURCE_DIR}")

add_executable(cfs_test
  test_main.c
  test_interop_predef.c
  test_interop_before.c
  test_interop_after.c
)
cfs_apply_target_defaults(cfs_test)
target_compile_definitions(cfs_test PRIVATE CFS_ENABLE_FAULT_INJECTION)
target_include_directories(cfs_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(cfs_test PRIVATE cfs_greatest)

if(CFS_HEADER_ONLY)
  target_sources(cfs_test PRIVATE "${_CFS_SOURCE_FILE}")
  target_link_libraries(cfs_test PRIVATE cfs::cfs)
  if(TARGET cfs_coverage_flags)
    target_link_libraries(cfs_test PRIVATE cfs_coverage_flags)
  endif()
else()
  # Dedicated fault-injected library target (named without "test" so Valgrind
  # test discovery does not attempt to execute a shared .so) rather than
  # mutating the production/installed `cfs` target.
  if(CFS_BUILD_SHARED)
    add_library(cfs_fi SHARED "${_CFS_SOURCE_FILE}")
  else()
    add_library(cfs_fi STATIC "${_CFS_SOURCE_FILE}")
  endif()
  cfs_configure_compiled_library(cfs_fi)
  target_compile_definitions(cfs_fi PRIVATE CFS_ENABLE_FAULT_INJECTION)
  if(TARGET cfs_coverage_flags)
    target_link_libraries(cfs_fi PRIVATE cfs_coverage_flags)
    target_link_options(cfs_test PRIVATE "--coverage")
  endif()

  target_link_libraries(cfs_test PRIVATE cfs_fi)
endif()

if(EMSCRIPTEN)
  target_link_options(cfs_test PRIVATE
    "-sASYNCIFY"
    "-sINITIAL_MEMORY=67108864"
    "-sNODERAWFS=1"
    "$<$<BOOL:${_CFS_USE_MULTITHREAD}>:-pthread>"
  )
endif()

add_test(NAME cfs_test COMMAND cfs_test)

# Optional C++ header interoperability test (verifies <sal.h>, extern "C" safety,
# and linking against the unmodified production `cfs::cfs` target)
if(NOT CMAKE_CXX_COMPILER AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT DOS)
  include(CheckLanguage)
  check_language(CXX)
endif()
if(CMAKE_CXX_COMPILER AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT DOS)
  enable_language(CXX)
  if(CMAKE_CXX_COMPILER_ID STREQUAL CMAKE_C_COMPILER_ID)
    add_executable(cfs_cxx_interop_test test_interop_cxx.cpp)
    cfs_apply_target_defaults(cfs_cxx_interop_test)
    target_include_directories(cfs_cxx_interop_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(cfs_cxx_interop_test PRIVATE cfs::cfs)

    if(CFS_HEADER_ONLY)
      target_sources(cfs_cxx_interop_test PRIVATE "${_CFS_SOURCE_FILE}")
    endif()
    if(TARGET cfs_coverage_flags)
      target_link_options(cfs_cxx_interop_test PRIVATE "--coverage")
    endif()

    add_test(NAME cfs_cxx_interop_test COMMAND cfs_cxx_interop_test)
  endif()
endif()
