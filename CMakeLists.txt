project(videoutils)
cmake_minimum_required(VERSION 3.9)

option(INCLUDE_TESTS "Include Tests" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

# add ffmpeg build output
include_directories(${CMAKE_BINARY_DIR}/ffmpeg/include)
link_directories(${CMAKE_BINARY_DIR}/ffmpeg/lib)

add_subdirectory(src)

if (INCLUDE_TESTS)
  add_subdirectory(tests)
endif()
