cmake_minimum_required(VERSION 3.20)
project(tar_to_stream CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only interface library
add_library(tar_to_stream INTERFACE)
target_include_directories(tar_to_stream INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
