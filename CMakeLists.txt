cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_COLOR_DIAGNOSTICS ON)
project(fork-plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
include_directories(include/)

message(WARNING "######## test color output")

# option(WITH_LIBINPUT "Build plugin for libinput." ON)
# option(WITH_XORG "Build plugin for Xorg server" ON)
# /usr/share/cmake-3.30/Modules/CMakeDependentOption.cmake

# https://medium.com/@alasher/colored-c-compiler-output-with-ninja-clang-gcc-10bfe7f2b949
# force for Ninja:
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." FALSE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options(-fdiagnostics-color=always)
       add_link_options(-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options(-fcolor-diagnostics -ferror-limit=2)
       add_link_options(-fcolor-diagnostics)
    else()
      error("unknown compiler")
    endif ()
endif ()
#end

# why this? it builds everything with -lgcov !!
include(./CodeCoverage.cmake)
# ai cmake-extras

# configure_file(CMake/config.h.in ${PROJECT_BINARY_DIR}/include/config.h)

add_library(forking_machine INTERFACE)
target_include_directories(forking_machine INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_library(forking_machine::forking_machine ALIAS forking_machine)


find_package(PkgConfig REQUIRED)


# check to have
find_package(GTest)
if(GTest_FOUND)
  enable_testing()
  add_subdirectory(test)
else()
  message("GTest NOT FOUND, hence not building")
endif()

export(EXPORT fork FILE dump.cmake)



if(WIN32)
add_subdirectory(windows)
endif()

