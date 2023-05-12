cmake_minimum_required(VERSION 3.22)
project(pika)

# Compile commands json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# CCACHE Setup
if (CCACHE_FOUND)
    message(CCACHE_FOUND)
    set(CMAKE_CXX_COMPILER_LAUNCHER ccache)
endif (CCACHE_FOUND)
find_program(CCACHE_FOUND ccache)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Use mold as linker if found
find_program(MOLD_FOUND "mold")
IF (MOLD_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -B/usr/local/libexec/mold")
ENDIF ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
endif()

add_subdirectory(src)
add_subdirectory(third_party)
add_subdirectory(tests)
