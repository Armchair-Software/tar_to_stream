include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_tar_to_stream test_tar_to_stream.cpp)
target_link_libraries(test_tar_to_stream PRIVATE tar_to_stream Catch2::Catch2WithMain)

if(ENABLE_COVERAGE)
  target_compile_options(test_tar_to_stream PRIVATE --coverage -O0 -g)
  target_link_options(test_tar_to_stream PRIVATE --coverage)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_tar_to_stream)
