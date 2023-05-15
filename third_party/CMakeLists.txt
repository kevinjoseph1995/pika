
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(fmt)

option(BUILD_GMOCK "Builds the googlemock subproject" OFF)
option(INSTALL_GTEST "Disable installing gtest" OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-restrict")
endif ()

add_subdirectory(backward-cpp)
add_subdirectory(googletest)