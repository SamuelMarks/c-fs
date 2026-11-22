# File: tests/CMakeLists.txt
# Brief: CMake build configuration for c-fs tests.

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest")
  message(STATUS "Fetching greatest from local directory")
  include(FetchContent)
  FetchContent_Declare(
    greatest
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../greatest"
  )
  FetchContent_MakeAvailable(greatest)
elseif(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18")
  include(FetchContent)
  FetchContent_Declare(
    greatest
    URL https://raw.githubusercontent.com/SamuelMarks/greatest/master/greatest.h
    DOWNLOAD_NO_EXTRACT TRUE
  )
  FetchContent_MakeAvailable(greatest)
else()
  set(greatest_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/greatest-src")
  if(NOT EXISTS "${greatest_SOURCE_DIR}/greatest.h")
    file(MAKE_DIRECTORY "${greatest_SOURCE_DIR}")
    file(DOWNLOAD
      "https://raw.githubusercontent.com/SamuelMarks/greatest/master/greatest.h"
      "${greatest_SOURCE_DIR}/greatest.h"
      STATUS _cfs_greatest_dl_status
    )
    list(GET _cfs_greatest_dl_status 0 _cfs_greatest_dl_rc)
    if(NOT _cfs_greatest_dl_rc EQUAL 0)
      message(FATAL_ERROR "Failed to download greatest.h: ${_cfs_greatest_dl_status}")
    endif()
  endif()
endif()

add_executable(cfs_test
  test_main.c
  test_interop_predef.c
  test_interop_before.c
  test_interop_after.c
)
cfs_apply_c_target_settings(cfs_test)
target_compile_definitions(cfs_test PRIVATE CFS_ENABLE_FAULT_INJECTION)

if(CFS_HEADER_ONLY)
  target_sources(cfs_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/cfs.c")
  target_link_libraries(cfs_test PRIVATE cfs::cfs)
else()
  # Build a dedicated fault-injected library target (named without "test" so
  # Valgrind test-runner discovery does not attempt to execute a shared .so)
  # instead of mutating the production/installed `cfs` target.
  if(CFS_BUILD_SHARED)
    add_library(cfs_fi SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../src/cfs.c")
  else()
    add_library(cfs_fi STATIC "${CMAKE_CURRENT_SOURCE_DIR}/../src/cfs.c")
  endif()
  cfs_configure_compiled_library(cfs_fi)
  target_compile_definitions(cfs_fi PRIVATE CFS_ENABLE_FAULT_INJECTION)
  if(TARGET cfs_internal_coverage)
    target_link_libraries(cfs_fi PRIVATE cfs_internal_coverage)
  endif()

  target_link_libraries(cfs_test PRIVATE cfs_fi)
endif()

if(TARGET cfs_internal_coverage)
  if(CFS_HEADER_ONLY)
    target_link_libraries(cfs_test PRIVATE cfs_internal_coverage)
  else()
    target_link_options(cfs_test PRIVATE "--coverage")
  endif()
endif()

if(EMSCRIPTEN)
  target_link_options(cfs_test PRIVATE
    "-sASYNCIFY"
    "-sINITIAL_MEMORY=67108864"
    "-sNODERAWFS=1"
    "$<$<BOOL:${_CFS_USE_MULTITHREAD}>:-pthread>"
  )
endif()

target_include_directories(cfs_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(cfs_test SYSTEM PRIVATE "${greatest_SOURCE_DIR}")

add_test(NAME cfs_test COMMAND cfs_test)

# Optional C++ header interoperability test (verifies <sal.h>, extern "C" safety,
# and linking against the unmodified production `cfs::cfs` target)
if(NOT CMAKE_CXX_COMPILER)
  include(CheckLanguage)
  check_language(CXX)
endif()
if(CMAKE_CXX_COMPILER AND NOT CMAKE_SYSTEM_NAME STREQUAL "DOS" AND NOT DOS)
  enable_language(CXX)
  if(CFS_IS_TOP_LEVEL AND MSVC AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU" AND NOT _CFS_RTC_FLAG STREQUAL "")
    foreach(_cxx_flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
      if(DEFINED ${_cxx_flag_var})
        string(REGEX REPLACE "/RTC[1csu]+" "" ${_cxx_flag_var} "${${_cxx_flag_var}}")
        string(REGEX REPLACE "/O(1|2|x|b1|b2)" "/Od" ${_cxx_flag_var} "${${_cxx_flag_var}}")
        string(REPLACE "/GL" "" ${_cxx_flag_var} "${${_cxx_flag_var}}")
      endif()
    endforeach()
  endif()
  if(CMAKE_CXX_COMPILER_ID STREQUAL CMAKE_C_COMPILER_ID)
    add_executable(cfs_cxx_interop_test test_interop_cxx.cpp)
    cfs_apply_common_target_properties(cfs_cxx_interop_test)

    if(CFS_HEADER_ONLY)
      target_sources(cfs_cxx_interop_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src/cfs.c")
      if("c_std_90" IN_LIST CMAKE_C_COMPILE_FEATURES)
        set_target_properties(cfs_cxx_interop_test PROPERTIES
          C_STANDARD 90
          C_STANDARD_REQUIRED ON
          C_EXTENSIONS OFF
        )
      endif()
      target_link_libraries(cfs_cxx_interop_test PRIVATE cfs_internal_flags)
    endif()

    target_include_directories(cfs_cxx_interop_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(cfs_cxx_interop_test PRIVATE cfs::cfs)

    if(MSVC AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
      target_compile_options(cfs_cxx_interop_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:/W4>"
        "$<$<COMPILE_LANGUAGE:CXX>:/WX>"
      )
      if(NOT _CFS_RTC_FLAG STREQUAL "")
        target_compile_options(cfs_cxx_interop_test PRIVATE
          "$<$<COMPILE_LANGUAGE:CXX>:/Od>"
          "$<$<COMPILE_LANGUAGE:CXX>:${_CFS_RTC_FLAG}>"
        )
      endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
      target_compile_options(cfs_cxx_interop_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Werror>"
      )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Watcom")
      target_compile_options(cfs_cxx_interop_test PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-wx>"
        "$<$<COMPILE_LANGUAGE:CXX>:-we>"
      )
    endif()

    add_test(NAME cfs_cxx_interop_test COMMAND cfs_cxx_interop_test)
  endif()
endif()
