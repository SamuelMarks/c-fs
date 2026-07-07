# File: tests/CMakeLists.txt
# Brief: CMake build configuration for c-fs tests.

include(FetchContent)
FetchContent_Declare(
  greatest
  URL https://raw.githubusercontent.com/SamuelMarks/greatest/master/greatest.h
  DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(greatest)

add_executable(cfs_test test_main.c)
target_link_libraries(cfs_test PRIVATE cfs)

if(EMSCRIPTEN)
    set_target_properties(cfs_test PROPERTIES LINK_FLAGS "-pthread -sUSE_PTHREADS=1 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sNODERAWFS=1")
endif()

# Since we don't extract the file to a directory, we include the directory it was downloaded to.
target_include_directories(cfs_test PRIVATE ${greatest_SOURCE_DIR})

add_test(NAME cfs_test COMMAND cfs_test)
