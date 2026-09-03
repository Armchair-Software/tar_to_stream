cmake_minimum_required(VERSION 3.20)
project(tar_to_stream_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

include(CTest)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        fa43b77429ba76c462b1898d6cd2f2d7a9416b14  # v3.7.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(test_tar_to_stream test_tar_to_stream.cpp)
target_include_directories(test_tar_to_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(test_tar_to_stream PRIVATE Catch2::Catch2WithMain)

if(ENABLE_COVERAGE)
  target_compile_options(test_tar_to_stream PRIVATE --coverage -O0 -g)
  target_link_options(test_tar_to_stream PRIVATE --coverage)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_tar_to_stream)
