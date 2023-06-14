cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD_REQUIRED 17)
set(CMAKE_CXX_STANDARD 17)

project(zsim_tests)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -std=c++17 -fopenmp -Wall -Wno-string-conversion -ggdb $ENV{CXXFLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -std=c++17 -fopenmp -Wall -Wno-string-conversion -ggdb $ENV{CXXFLAGS}")

if(CMAKE_BUILD_TYPE STREQUAL "Prof")
  set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -fopenmp -pg -Wall -Wno-string-conversion -ggdb $ENV{CXXFLAGS}")
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# test_read_flash
add_executable(test_read_flash ${PROJECT_SOURCE_DIR}/test_read_flash.cc)
include_directories(test_read_flash PUBLIC ${PROJECT_SOURCE_DIR})
include_directories(test_read_flash PUBLIC ${PROJECT_SOURCE_DIR}/src)
include_directories(test_read_flash PUBLIC ${PROJECT_SOURCE_DIR}/../misc/hooks)
