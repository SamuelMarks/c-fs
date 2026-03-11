include(FetchContent)
FetchContent_Declare(
  greatest
  URL https://raw.githubusercontent.com/SamuelMarks/greatest/refs/heads/cmake-and-msvc/greatest.h
  DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(greatest)

add_executable(cfs_test test_main.c)
target_link_libraries(cfs_test PRIVATE cfs)

# Since we don't extract the file to a directory, we include the directory it was downloaded to.
target_include_directories(cfs_test PRIVATE ${greatest_SOURCE_DIR})

add_test(NAME cfs_test COMMAND cfs_test)